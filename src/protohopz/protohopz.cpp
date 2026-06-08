#include "protohopz/protohopz.hpp"
#include <boost/asio/detached.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <spdlog/spdlog.h>

// логируем только каждый 100-й пакет по sequence
// события без seq (cwnd, таймауты, handshake фазы) логируются всегда
#define SPDLOG_TRACE_PACKET(seq, ...) \
    do { if(((seq) % 100) == 0) SPDLOG_TRACE(__VA_ARGS__); } while(0)

ProtoHopZ::ProtoHopZ(
    boost::asio::ip::udp::socket socket,
    boost::asio::ip::udp::endpoint peer_endpoint
)
:   socket_(std::move(socket)),
    peer_endpoint_(std::move(peer_endpoint)),
    received_packets_queue_(socket_.get_executor()),
    cwnd_(socket_.get_executor(), 1.0)
{
    SPDLOG_TRACE("ProtoHopZ constructed: peer={}:{}",
                 peer_endpoint_.address().to_string(),
                 peer_endpoint_.port());
}

void ProtoHopZ::start_loops()
{
    auto executor = socket_.get_executor();

    boost::asio::co_spawn(
        executor,
        receive_loop(),
        boost::asio::bind_cancellation_slot(
            cancellation_signal_receive_loop.slot(),
            boost::asio::detached
        )
    );

    boost::asio::co_spawn(
        executor,
        timeout_loop(),
        boost::asio::bind_cancellation_slot(
            cancellation_signal_timeout_loop.slot(),
            boost::asio::detached
        )
    );

    loops_started = true;
    SPDLOG_TRACE("ProtoHopZ loops started");
}

void ProtoHopZ::stop_loops()
{
    // в будущем переписать stop_loops() так, чтобы отправлялись все пакеты из in_flight_,
    // и только после этого все завершалось

    boost::system::error_code ec;

    SPDLOG_TRACE("ProtoHopZ::stop_loops: cancelling loops; in_flight={}, received_packets_seen={}",
                 in_flight_.size(), received_packets_.size());

    cancellation_signal_receive_loop.emit(boost::asio::cancellation_type::all);
    cancellation_signal_timeout_loop.emit(boost::asio::cancellation_type::all);

    socket_.cancel(ec);
    if(ec)
        SPDLOG_CRITICAL("ProtoHopZ::stop_loops: socket_.cancel failed: {}", ec.message());

    // разбудить received_packets_queue.pop()
    PHZ::Packet end_packet =
    {
        .header =
        {
            .type = PHZ::PacketType::END_TRANSFER,
            .flags = {},
            .size = 0,
            .sequence = ++sequence_counter_
        }
    };

    SPDLOG_TRACE_PACKET(end_packet.header.sequence,
        "ProtoHopZ::stop_loops: pushing END_TRANSFER seq={} to received_packets_queue_",
        end_packet.header.sequence);

    received_packets_queue_.push(std::move(end_packet));

    loops_started = false;

    SPDLOG_TRACE("ProtoHopZ loops stoped");
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::handshake_initiator()
{
    SPDLOG_TRACE("ProtoHopZ::handshake_initiator: start");
    if(!loops_started)
    {
        spdlog::critical("ProtoHopZ loops not running");
        co_return boost::system::errc::make_error_code(
            boost::system::errc::operation_canceled
        );
    }
    auto ec = crypto_context_.init();
    if(ec)
    {
        spdlog::critical(ec.what());
        co_return ec;
    }
    SPDLOG_TRACE("ProtoHopZ::handshake_initiator: crypto init done");

    auto self_public_key = crypto_context_.get_own_public_key();

    PHZ::Packet self_handshake_packet =
    {
        .header =
        {
            .type = PHZ::PacketType::HANDSHAKE,
            .flags = {},
            .size = X25519_LEN,
            .sequence = 0 // устанавливается в send_packet
        }
    };

    std::memcpy(self_handshake_packet.payload, self_public_key.data(), X25519_LEN);

    SPDLOG_TRACE("ProtoHopZ::handshake_initiator: sending HANDSHAKE (own pubkey, size={})", X25519_LEN);
    ec = co_await send_packet(&self_handshake_packet);
    if(ec)
    {
        spdlog::critical(ec.message());
        co_return ec;
    }

    PHZ::Packet peer_handshake_packet;
    SPDLOG_TRACE("ProtoHopZ::handshake_initiator: awaiting peer's HANDSHAKE");
    ec = co_await receive_packet(&peer_handshake_packet);
    if(ec)
    {
        spdlog::critical(ec.message());
        co_return ec;
    }
    SPDLOG_TRACE("ProtoHopZ::handshake_initiator: got peer's HANDSHAKE, size={}",
                 peer_handshake_packet.header.size);

    if(peer_handshake_packet.header.size != X25519_LEN)
    {
        spdlog::critical("Peer's key len not equal X25519 key len");
        co_return boost::system::errc::make_error_code(
            boost::system::errc::bad_message
        );
    }

    std::span<std::byte, X25519_LEN> peer_public_key(
        reinterpret_cast<std::byte*>(peer_handshake_packet.payload),
        X25519_LEN
    );

    ec = crypto_context_.set_peer_public_key(peer_public_key);
    if(ec)
    {
        spdlog::critical(ec.message());
        co_return ec;
    }
    SPDLOG_TRACE("ProtoHopZ::handshake_initiator: peer public key set, encryption ready");

    connection_encrypted_ = true;
    co_return ec;
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::handshake_responder()
{
    SPDLOG_TRACE("ProtoHopZ::handshake_responder: start");
    if(!loops_started)
    {
        spdlog::critical("ProtoHopZ: loops not running");
        co_return boost::system::errc::make_error_code(
            boost::system::errc::operation_canceled
        );
    }

    auto ec = crypto_context_.init();
    if(ec)
    {
        spdlog::critical(ec.message());
        co_return ec;
    }
    SPDLOG_TRACE("ProtoHopZ::handshake_responder: crypto init done");

    PHZ::Packet peer_handshake_packet;
    SPDLOG_TRACE("ProtoHopZ::handshake_responder: awaiting peer's HANDSHAKE");
    ec = co_await receive_packet(&peer_handshake_packet);
    if(ec)
    {
        spdlog::critical(ec.message());
        co_return ec;
    }
    SPDLOG_TRACE("ProtoHopZ::handshake_responder: got peer's HANDSHAKE, size={}",
                 peer_handshake_packet.header.size);

    if(peer_handshake_packet.header.size != X25519_LEN)
    {
        spdlog::critical("Peer's key len not equal X25519 key len");
        co_return boost::system::errc::make_error_code(
            boost::system::errc::bad_message
        );
    }

    std::span<std::byte, X25519_LEN> peer_public_key(
        reinterpret_cast<std::byte*>(peer_handshake_packet.payload),
        X25519_LEN
    );

    ec = crypto_context_.set_peer_public_key(peer_public_key);
    if(ec)
    {
        spdlog::critical(ec.message());
        co_return ec;
    }
    SPDLOG_TRACE("ProtoHopZ::handshake_responder: peer public key set, encryption ready");

    auto self_public_key = crypto_context_.get_own_public_key();

    PHZ::Packet self_handshake_packet =
    {
        .header =
        {
            .type = PHZ::PacketType::HANDSHAKE,
            .flags = {},
            .size = X25519_LEN,
            .sequence = 0 // устанавливается в send_packet
        }
    };

    std::memcpy(self_handshake_packet.payload, self_public_key.data(), X25519_LEN);

    SPDLOG_TRACE("ProtoHopZ::handshake_responder: sending HANDSHAKE (own pubkey, size={})", X25519_LEN);
    ec = co_await send_packet(&self_handshake_packet);
    if(ec)
    {
        spdlog::critical(ec.message());
        co_return ec;
    }

    connection_encrypted_ = true;
    co_return ec;
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::send_packet(const PHZ::Packet* source)
{
    boost::system::error_code ec;
    auto executor = co_await boost::asio::this_coro::executor;

    SPDLOG_TRACE("ProtoHopZ::send_packet: enter type={} size={}, in_flight={}, cwnd={}",
                 static_cast<int>(source->header.type), source->header.size,
                 in_flight_.size(), cwnd_.get());

    while(in_flight_.size() >= static_cast<std::size_t>(cwnd_.get()))
    {
        SPDLOG_TRACE("ProtoHopZ::send_packet: BLOCKED on cwnd (in_flight={} >= cwnd={}), waiting",
                     in_flight_.size(), cwnd_.get());
        co_await cwnd_.wait(
            boost::asio::redirect_error(
                boost::asio::use_awaitable,
                ec
            )
        );

        if(ec)
        {
            SPDLOG_CRITICAL("ProtoHopZ::send_packet: cwnd_.wait error: {}", ec.message());
            co_return ec;
        }
        SPDLOG_TRACE("ProtoHopZ::send_packet: cwnd unblocked, retrying (in_flight={}, cwnd={})",
                     in_flight_.size(), cwnd_.get());
    }

    auto packet = *source;
    packet.header.sequence = sequence_counter_++;
    SPDLOG_TRACE_PACKET(packet.header.sequence,
        "ProtoHopZ::send_packet: assigned seq={}, type={}, plaintext size={}",
        packet.header.sequence, static_cast<int>(packet.header.type),
        packet.header.size);

    if(connection_encrypted_)
    {
        if(packet.header.size > PHZ::PACKET_PAYLOAD_SIZE)
        {
            SPDLOG_CRITICAL("ProtoHopZ::send_packet: plaintext size {} > PACKET_PAYLOAD_SIZE {}",
                            packet.header.size, PHZ::PACKET_PAYLOAD_SIZE);
            co_return boost::system::errc::make_error_code(
                boost::system::errc::message_size
            );
        }

        std::span<std::byte> data_to_encrypt(
            reinterpret_cast<std::byte*>(packet.payload),
            packet.header.size
        );

        auto result = crypto_context_.encrypt_data(data_to_encrypt);
        if(!result)
        {
            SPDLOG_CRITICAL("ProtoHopZ::send_packet: encrypt_data failed for seq={}",
                            packet.header.sequence);
            co_return boost::system::errc::make_error_code(
                boost::system::errc::bad_address
            );
        }

        if(result.value().size() > PHZ::PACKET_SIZE)
        {
            SPDLOG_CRITICAL("ProtoHopZ::send_packet: ciphertext size {} > PACKET_SIZE {}",
                            result.value().size(), PHZ::PACKET_SIZE);
            co_return boost::system::errc::make_error_code(
                boost::system::errc::message_size
            );
        }

        std::memcpy(PHZ::payload_region(packet), result.value().data(), result.value().size());
        packet.header.size = static_cast<uint16_t>(result.value().size());
        SPDLOG_TRACE_PACKET(packet.header.sequence,
            "ProtoHopZ::send_packet: encrypted seq={}, ciphertext size={}",
            packet.header.sequence, packet.header.size);
    }

    co_await socket_.async_send_to(
        boost::asio::buffer(
            &packet,
            sizeof(PHZ::PacketHeader) + packet.header.size
        ),
        peer_endpoint_,
        boost::asio::redirect_error(boost::asio::use_awaitable, ec)
    );

    if(ec)
    {
        SPDLOG_CRITICAL("ProtoHopZ::send_packet: async_send_to error for seq={}: {}",
                        packet.header.sequence, ec.message());
        co_return ec;
    }

    in_flight_[packet.header.sequence] =
    {std::chrono::steady_clock::now(), packet};

    SPDLOG_TRACE_PACKET(packet.header.sequence,
        "ProtoHopZ::send_packet: sent seq={}, in_flight now {}",
        packet.header.sequence, in_flight_.size());

   co_return ec;
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::receive_packet(PHZ::Packet* destination)
{
    boost::system::error_code ec;

    SPDLOG_TRACE("ProtoHopZ::receive_packet: awaiting next packet from received_packets_queue_");

    PHZ::Packet packet;
    packet = co_await received_packets_queue_.pop(
        boost::asio::redirect_error(
            boost::asio::use_awaitable,
            ec
        )
    );

    if(ec)
    {
        SPDLOG_TRACE("ProtoHopZ::receive_packet: pop returned error: {}", ec.message());
        destination = nullptr;
        co_return ec;
    }

    *destination = packet;

    SPDLOG_TRACE_PACKET(packet.header.sequence,
        "ProtoHopZ::receive_packet: got packet type={} seq={} size={}",
        static_cast<int>(packet.header.type),
        packet.header.sequence,
        packet.header.size);

    co_return boost::system::error_code{};
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::resend_packet(uint32_t sequense)
{
    boost::system::error_code ec;

    auto it = in_flight_.find(sequense);
    if(it == in_flight_.end())
    {
        SPDLOG_TRACE_PACKET(sequense,
            "ProtoHopZ::resend_packet: seq={} not in_flight (already acked?)", sequense);
        co_return ec;
    }

    SPDLOG_TRACE_PACKET(sequense,
        "ProtoHopZ::resend_packet: resending seq={} size={}",
        sequense, it->second.packet.header.size);

    co_await socket_.async_send_to(
        boost::asio::buffer(
            &it->second.packet,
            sizeof(PHZ::PacketHeader) + it->second.packet.header.size
        ),
        peer_endpoint_,
        boost::asio::redirect_error(boost::asio::use_awaitable, ec)
    );

    if(ec)
    {
        SPDLOG_CRITICAL("ProtoHopZ::resend_packet: async_send_to error for seq={}: {}",
                        sequense, ec.message());
        co_return ec;
    }

    // во время co_await пакет мог быть ACK'нут
    it = in_flight_.find(sequense);
    if(it != in_flight_.end())
        it->second.send_time = std::chrono::steady_clock::now();

    SPDLOG_TRACE_PACKET(sequense,
        "ProtoHopZ::resend_packet: seq={} send_time updated", sequense);

    co_return ec;
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::receive_loop()
{
    boost::system::error_code ec;

    while(true)
    {
        PHZ::Packet packet;

        co_await socket_.async_receive_from(
            boost::asio::buffer(&packet, sizeof(PHZ::Packet)),
            peer_endpoint_,
            boost::asio::redirect_error(boost::asio::use_awaitable, ec)
        );
        if(ec)
        {
            SPDLOG_TRACE("ProtoHopZ::receive_loop: async_receive_from error: {}", ec.message());
            co_return ec;
        }

        SPDLOG_TRACE_PACKET(packet.header.sequence,
            "ProtoHopZ::receive_loop: got type={} seq={} size={}",
            static_cast<int>(packet.header.type),
            packet.header.sequence,
            packet.header.size);

        switch(packet.header.type)
        {
            case PHZ::PacketType::ACK :
            {
                ack_handler(packet.header.sequence);

                break;
            }

            case PHZ::PacketType::DATA :
            {
                if(packet.header.size > PHZ::PACKET_SIZE)
                {
                    spdlog::critical("data size {} > PACKET_SIZE {}", packet.header.size, PHZ::PACKET_SIZE);
                    co_return boost::system::errc::make_error_code(
                        boost::system::errc::bad_message
                    );
                }

                send_ack(packet.header.sequence);

                if(received_packets_.contains(packet.header.sequence))
                {
                    SPDLOG_TRACE_PACKET(packet.header.sequence,
                        "ProtoHopZ::receive_loop: duplicate DATA seq={}, skipping", packet.header.sequence);
                    continue;
                }
                if(connection_encrypted_)
                {
                    std::span<std::byte> data_to_decrypt(
                        reinterpret_cast<std::byte*>(PHZ::payload_region(packet)),
                        packet.header.size
                    );

                    auto result = crypto_context_.decrypt_data(data_to_decrypt);
                    if(!result)
                    {
                        spdlog::critical("decrypt error for DATA seq={}", packet.header.sequence);
                        co_return boost::system::errc::make_error_code(
                            boost::system::errc::bad_address
                        );
                    }

                    packet.header.size = result.value().size();
                    std::memcpy(packet.payload, result.value().data(), packet.header.size);
                    SPDLOG_TRACE_PACKET(packet.header.sequence,
                        "ProtoHopZ::receive_loop: decrypted seq={} to plaintext size={}",
                        packet.header.sequence, packet.header.size);
                }
                received_packets_.insert(packet.header.sequence);
                received_packets_queue_.push(std::move(packet));
                SPDLOG_TRACE_PACKET(packet.header.sequence,
                    "ProtoHopZ::receive_loop: pushed DATA seq={} to received_packets_queue_",
                    packet.header.sequence);

                break;
            }

            case PHZ::PacketType::HANDSHAKE :
            {
                if(packet.header.size > PHZ::PACKET_SIZE)
                {
                    spdlog::critical("HANDSHAKE size {} > PACKET_SIZE {}", packet.header.size, PHZ::PACKET_SIZE);
                    co_return boost::system::errc::make_error_code(
                        boost::system::errc::bad_message
                    );
                }

                send_ack(packet.header.sequence);

                if(received_packets_.contains(packet.header.sequence))
                {
                    SPDLOG_TRACE_PACKET(packet.header.sequence,
                        "ProtoHopZ::receive_loop: duplicate HANDSHAKE seq={}, skipping",
                        packet.header.sequence);
                    continue;
                }

                received_packets_.insert(packet.header.sequence);
                received_packets_queue_.push(std::move(packet));
                SPDLOG_TRACE_PACKET(packet.header.sequence,
                    "ProtoHopZ::receive_loop: pushed HANDSHAKE seq={} to received_packets_queue_",
                    packet.header.sequence);

                break;
            }
        }
    }
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::timeout_loop()
{
    boost::system::error_code ec;
    auto executor = co_await boost::asio::this_coro::executor;
    boost::asio::steady_timer timer(executor);

    while(true)
    {
        auto now = std::chrono::steady_clock::now();

        std::vector<uint32_t> timed_out;

        for(const auto& [seq, pkt] : in_flight_)
        {
            if(now - pkt.send_time > PHZ::TIMEOUT)
                timed_out.push_back(seq);
        }

        if(!timed_out.empty())
        {
            SPDLOG_TRACE("ProtoHopZ::timeout_loop: {} timed out, cwnd {} -> 1.0, ssthresh {} -> {}",
                         timed_out.size(), cwnd_.get(), sshthresh_,
                         std::max(cwnd_.get() / 2.0, 2.0));
            sshthresh_ = std::max(cwnd_.get() / 2.0, 2.0);
            cwnd_.set(1.0);
        }

        for(uint32_t seq : timed_out)
        {
            if(!in_flight_.contains(seq))
            {
                SPDLOG_TRACE_PACKET(seq,
                    "ProtoHopZ::timeout_loop: seq={} already acked before resend", seq);
                continue;
            }

            SPDLOG_TRACE_PACKET(seq,
                "ProtoHopZ::timeout_loop: triggering resend for seq={}", seq);
            ec = co_await resend_packet(seq);
            if(ec)
            {
                SPDLOG_CRITICAL("ProtoHopZ::timeout_loop: resend_packet({}) error: {}", seq, ec.message());
                co_return ec;
            }
        }

        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec)
        );

        if(ec)
        {
            SPDLOG_TRACE("ProtoHopZ::timeout_loop: timer error: {}", ec.message());
            co_return ec;
        }
    }

    co_return ec;
}

void ProtoHopZ::send_ack(uint32_t sequence)
{
    SPDLOG_TRACE_PACKET(sequence,
        "ProtoHopZ::send_ack: sending ACK for seq={} (fire-and-forget)", sequence);

    auto packet = std::make_shared<PHZ::Packet>(PHZ::Packet{
        .header =
        {
            .type = PHZ::PacketType::ACK,
            .flags = {},
            .size = 0,
            .sequence = sequence
        }
    });

    socket_.async_send_to(
        boost::asio::buffer(
            packet.get(),
            sizeof(PHZ::PacketHeader)
        ),
        peer_endpoint_,
        // захват packet
        [packet](boost::system::error_code ec, std::size_t bytes)
        {
            if(ec)
                spdlog::critical("ProtoHopZ::send_ack: async_send_to error: {}", ec.message());
        }
    );
}

void ProtoHopZ::ack_handler(uint32_t sequence)
{
    auto erased = in_flight_.erase(sequence);
    // проверка на уже подтвержденные пакеты
    if(!erased)
    {
        SPDLOG_TRACE_PACKET(sequence,
            "ProtoHopZ::ack_handler: seq={} not in_flight (duplicate ACK?)", sequence);
        return;
    }

    double prev_cwnd = cwnd_.get();
    if(cwnd_.get() < sshthresh_)
        cwnd_.set(cwnd_.get() * 2);
    else
        cwnd_.set(cwnd_.get() + 1.0);

    SPDLOG_TRACE_PACKET(sequence,
        "ProtoHopZ::ack_handler: ACKed seq={}, in_flight={}, cwnd {} -> {} (ssthresh={})",
        sequence, in_flight_.size(), prev_cwnd, cwnd_.get(), sshthresh_);
}
