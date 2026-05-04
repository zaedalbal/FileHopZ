#include "protohopz/protostream.hpp"
#include <iostream>
#include <map>

ProtoStream::ProtoStream(boost::asio::ip::udp::socket socket)
:   executor_(socket.get_executor()),
    transport_(std::move(socket), boost::asio::ip::udp::endpoint()),
    ready_chunks_(executor_)
{}

ProtoStream::ProtoStream(boost::asio::ip::udp::socket socket, boost::asio::ip::udp::endpoint peer_endpoint)
:   executor_(socket.get_executor()),
    transport_(std::move(socket), std::move(peer_endpoint)),
    ready_chunks_(executor_)
{}

boost::asio::awaitable<boost::system::error_code>
ProtoStream::send(std::span<const std::byte> data)
{
    if(!loops_running_)
        start_loops();

    if(data.size() > PHZ::PACKET_SIZE) // в будущем сделать разбиение передаваемых данных на несколько пакетов
        co_return boost::system::errc::make_error_code(boost::system::errc::message_size);
    
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

    std::memcpy(packet.data, data.data(), packet.header.size);

    co_return co_await transport_.send_packet(&packet);
}

boost::asio::awaitable<ProtoStream::Chunk>
ProtoStream::receive()
{
    if(!loops_running_)
        start_loops();

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
            std::cerr << ec.message() << "\n";
        co_return Chunk{};
    }

    co_return chunk;
}

boost::asio::awaitable<void> ProtoStream::close()
{
    PHZ::Packet packet =
    {
        .header =
        {
            .type = PHZ::PacketType::END_TRANSFER,
            .flags = {},
            .size = 0,
            .sequence = 0 // sequence контроллируется в ProtoHopZ
        }
    };
    
    co_await transport_.send_packet(&packet);

    loops_running_ = false;
    cancellation_signal_receive_chunks_loop_.emit(boost::asio::cancellation_type::all);
    transport_.stop();
}

void ProtoStream::start_loops()
{
        transport_.start();

        loops_running_ = true;

        boost::asio::co_spawn(
            executor_,
            receive_chunks_loop(),
            boost::asio::bind_cancellation_slot(
                cancellation_signal_receive_chunks_loop_.slot(),
                boost::asio::detached
            )
        );
}

boost::asio::awaitable<void> ProtoStream::receive_chunks_loop()
{
    constexpr int BUFFER_SIZE = 1024;
    constexpr int SEQUENCE_WINDOW_SIZE = 4096;

    boost::system::error_code ec;
    std::unordered_map<decltype(PHZ::PacketHeader::sequence), PHZ::Packet> packets_buffer;
    decltype(PHZ::PacketHeader::sequence) expected_sequence = 0;
    
    while(true)
    {
        PHZ::Packet packet;

        ec = co_await transport_.receive_packet(&packet);
        if(ec)
            co_return;

        if(packet.header.sequence < expected_sequence)
            continue;

        if(packet.header.sequence > expected_sequence + SEQUENCE_WINDOW_SIZE)
        {
            co_await close();
            std::cerr <<  "Out of sequnce window: possible mailicious input\n";
            co_return;
        }
 
        if(packet.header.type != PHZ::PacketType::DATA)
        {
            // в будущем сделать обработчик пакетов
            std::cout << "skip packet in receive_chunks_loop\n";
            continue;
        }

        if(packets_buffer.contains(packet.header.sequence))
            continue; // скип дубликатов

        if(packets_buffer.size() > BUFFER_SIZE)
        {
            co_await close();
            std::cerr <<  "Buffer overflow: possible malicious input\n";
            co_return;
        }
        
        packets_buffer.emplace(packet.header.sequence, std::move(packet));

        while(true)
        {
            auto it = packets_buffer.find(expected_sequence);
            if(it == packets_buffer.end())
                break;
            
            ready_chunks_.push(ProtoStream::Chunk(it->second.data, it->second.header.size));
            packets_buffer.erase(it);
            ++expected_sequence;
        }
    }
}