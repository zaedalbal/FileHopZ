#include "protohopz/protostream.hpp"
#include <spdlog/spdlog.h>
#include <iostream>
#include <map>
#include <chrono>

ProtoStream::ProtoStream(boost::asio::ip::udp::socket socket, HANDSHAKE_MODE handshake_mode)
:   handshake_mode_(handshake_mode),
    executor_(socket.get_executor()),
    transport_(std::move(socket), boost::asio::ip::udp::endpoint()),
    ready_chunks_(executor_)
{
    SPDLOG_TRACE("ProtoStream constructed: handshake_mode={}",
                 handshake_mode_ == INITIATOR ? "INITIATOR" : "RESPONDER");
}

ProtoStream::ProtoStream(
    boost::asio::ip::udp::socket socket,
    boost::asio::ip::udp::endpoint peer_endpoint,
    HANDSHAKE_MODE handshake_mode
)
:   handshake_mode_(handshake_mode),
    executor_(socket.get_executor()),
    transport_(std::move(socket), std::move(peer_endpoint)),
    ready_chunks_(executor_)
{
    SPDLOG_TRACE("ProtoStream constructed: handshake_mode={}",
                 handshake_mode_ == INITIATOR ? "INITIATOR" : "RESPONDER");
}

boost::asio::awaitable<boost::system::error_code>
ProtoStream::send(std::span<const std::byte> data)
{
    SPDLOG_TRACE("ProtoStream::send: called size={}", data.size());

    if(!loops_running_)
    {
        auto ec = co_await start_loops();
        if(ec)
            co_return ec;
    }

    if(data.size() > PHZ::PACKET_PAYLOAD_SIZE) // в будущем будет разбиение передаваемых данных на несколько пакетов
    {
        spdlog::critical("ProtoStream::send: data size {} > PACKET_PAYLOAD_SIZE {}",
                         data.size(), PHZ::PACKET_PAYLOAD_SIZE);
        co_return boost::system::errc::make_error_code(boost::system::errc::message_size);
    }

    PHZ::Packet packet =
    {
        .header =
        {
            .type = PHZ::PacketType::DATA,
            .flags = {},
            .size = data.size(),
            .sequence = 0 // sequence контроллируется в ProtoHopZ
        }
    };

    std::memcpy(packet.payload, data.data(), packet.header.size);
    SPDLOG_TRACE("ProtoStream::send: forwarding DATA size={} to transport", packet.header.size);
    co_return co_await transport_.send_packet(&packet);
}

boost::asio::awaitable<ProtoStream::Chunk>
ProtoStream::receive()
{
    SPDLOG_TRACE("ProtoStream::receive: enter");

    if(!loops_running_)
    {
        auto ec = co_await start_loops();
        if(ec)
            co_return Chunk();
    }

    boost::system::error_code ec;

    auto executor = co_await boost::asio::this_coro::executor;

    ProtoStream::Chunk chunk = co_await ready_chunks_.pop(
        boost::asio::redirect_error(
            boost::asio::use_awaitable,
            ec
        )
    );

    if(ec)
    {
        if(ec != boost::asio::error::operation_aborted)
            spdlog::critical("ProtoStream::receive: ready_chunks_.pop error: {}", ec.message());
        else
            SPDLOG_TRACE("ProtoStream::receive: ready_chunks_.pop aborted");
        co_return Chunk{};
    }

    SPDLOG_TRACE("ProtoStream::receive: got chunk size={}", chunk.size_);
    co_return chunk;
}

boost::asio::awaitable<void> ProtoStream::close()
{
    SPDLOG_TRACE("ProtoStream::close: draining in_flight");

    auto drained = co_await transport_.wait_in_flight_drained(
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            PHZ::CLOSE_DRAIN_TIMEOUT
        )
    );

    SPDLOG_TRACE("ProtoStream::close: drain result={}", drained);

    // помечается loops_running_ = false до отмены сигналов, чтобы receive_chunks_loop
    // мог на это опереться в проверке, если в будущем захочется graceful exit
    loops_running_ = false;

    // останавливается приём чанков и затем транспорт
    cancellation_signal_receive_chunks_loop_.emit(boost::asio::cancellation_type::all);

    // дренаж ready_chunks_: cancel выше разбудил всех ожидающих в pop()
    // (operation_aborted); оставшиеся в очереди элементы заберёт деструктор
    // Async_queue, так что дополнительной очистки не требуется

    co_await transport_.stop_loops();

    SPDLOG_TRACE("ProtoStream::close: done");
}

boost::asio::awaitable<boost::system::error_code>
ProtoStream::start_loops()
{
    SPDLOG_TRACE("ProtoStream::start_loops: start, handshake_mode={}",
                 handshake_mode_ == INITIATOR ? "INITIATOR" : "RESPONDER");
    transport_.start_loops();

    boost::system::error_code ec;
    if(handshake_mode_ == INITIATOR)
        ec = co_await transport_.handshake_initiator();
    else
        ec = co_await transport_.handshake_responder();

    if(ec)
    {
        spdlog::critical("ProtoStream::start_loops: handshake failed: {}", ec.message());
        co_return ec;
    }
    SPDLOG_TRACE("ProtoStream::start_loops: handshake ok, spawning receive_chunks_loop");

    loops_running_ = true;

    boost::asio::co_spawn(
        executor_,
        receive_chunks_loop(),
        boost::asio::bind_cancellation_slot(
            cancellation_signal_receive_chunks_loop_.slot(),
            boost::asio::detached
        )
    );

    co_return ec;
}

boost::asio::awaitable<void> ProtoStream::receive_chunks_loop()
{
    constexpr int SEQUENCE_WINDOW_SIZE = 65536;

    boost::system::error_code ec;
    std::unordered_map<decltype(PHZ::PacketHeader::sequence), PHZ::Packet> packets_buffer;
    decltype(PHZ::PacketHeader::sequence) expected_sequence = 1;

    while(true)
    {
        PHZ::Packet packet;

        ec = co_await transport_.receive_packet(&packet);
        if(ec)
        {
            SPDLOG_TRACE("ProtoStream::receive_chunks_loop: transport_.receive_packet error: {}",
                         ec.message());
            co_return;
        }

        if(packet.header.type != PHZ::PacketType::DATA)
        {
            // в будущем сделать обработчик пакетов
            SPDLOG_TRACE("ProtoStream::receive_chunks_loop: skip non-DATA type={} seq={} (kept in transport, not buffered)",
                         static_cast<int>(packet.header.type), packet.header.sequence);
            continue;
        }

        if(packet.header.sequence < expected_sequence)
        {
            SPDLOG_TRACE("ProtoStream::receive_chunks_loop: old seq={} < expected={}, dropping",
                         packet.header.sequence, expected_sequence);
            continue;
        }

        if(packet.header.sequence > expected_sequence + SEQUENCE_WINDOW_SIZE)
        {
            SPDLOG_TRACE("ProtoStream::receive_chunks_loop: seq={} > expected+window={} (out of window, sender will resend after timeout)",
                         packet.header.sequence, expected_sequence + SEQUENCE_WINDOW_SIZE);
            continue; // пакет вне окна — отправитель переотправит после таймаута
        }

        if(packets_buffer.contains(packet.header.sequence))
        {
            SPDLOG_TRACE("ProtoStream::receive_chunks_loop: duplicate seq={} in buffer (size={})",
                         packet.header.sequence, packets_buffer.size());
            continue; // скип дубликатов
        }

        // hard cap: если буфер раздулся (например, sequence прыгнул из-за
        // потери ACK'ов), буфер сбрасывается и expected_sequence сдвигается
        // к текущему пакету; это страховка от OOM на стороне получателя
        if(packets_buffer.size() >= PHZ::MAX_PACKETS_BUFFER)
        {
            SPDLOG_CRITICAL("ProtoStream::receive_chunks_loop: packets_buffer exceeded {} ({} entries), resetting to seq={}",
                            PHZ::MAX_PACKETS_BUFFER, packets_buffer.size(), packet.header.sequence);
            packets_buffer.clear();
            expected_sequence = packet.header.sequence;
        }

        packets_buffer.emplace(packet.header.sequence, std::move(packet));
        SPDLOG_TRACE("ProtoStream::receive_chunks_loop: buffered seq={}, buffer size={}",
                     packet.header.sequence, packets_buffer.size());

        std::size_t flushed = 0;
        while(true)
        {
            auto it = packets_buffer.find(expected_sequence);
            if(it == packets_buffer.end())
                break;

            ready_chunks_.push(ProtoStream::Chunk(it->second.payload, it->second.header.size));
            packets_buffer.erase(it);
            ++expected_sequence;
            ++flushed;
        }
        if(flushed > 0)
        {
            SPDLOG_TRACE("ProtoStream::receive_chunks_loop: flushed {} chunk(s), expected_seq now {}, buffer size={}",
                         flushed, expected_sequence, packets_buffer.size());
        }
    }
}