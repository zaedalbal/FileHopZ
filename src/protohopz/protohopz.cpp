#include "protohopz/protohopz.hpp"
#include <boost/asio/detached.hpp>
#include <iostream>
#include <memory>
#include <vector>

ProtoHopZ::ProtoHopZ(
    boost::asio::ip::udp::socket socket,
    boost::asio::ip::udp::endpoint peer_endpoint
)
:   socket_(std::move(socket)),
    peer_endpoint_(std::move(peer_endpoint)),
    received_packets_queue_(socket_.get_executor()),
    cwnd_(socket_.get_executor(), 1.0)
{}

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
}

void ProtoHopZ::stop_loops()
{
    // в будущем переписать stop_loops() так, чтобы отправлялись все пакеты из in_flight_,
    // и только после этого все завершалось
    
    boost::system::error_code ec;

    cancellation_signal_receive_loop.emit(boost::asio::cancellation_type::all);
    cancellation_signal_timeout_loop.emit(boost::asio::cancellation_type::all);

    socket_.cancel(ec);

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

    received_packets_queue_.push(std::move(end_packet));

    loops_started = false;
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::handshake_initiator()
{
    if(!loops_started)
    {
        std::cerr << "ProtoHopZ: loops not running\n";
        co_return boost::system::errc::make_error_code(
            boost::system::errc::operation_canceled
        );
    }
    auto ec = crypto_context_.init();
    if(ec)
        co_return ec;

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

    ec = co_await send_packet(&self_handshake_packet);
    if(ec)
        co_return ec;

    PHZ::Packet peer_handshake_packet;
    ec = co_await receive_packet(&peer_handshake_packet);
    if(ec)
        co_return ec;
   
    if(peer_handshake_packet.header.size != X25519_LEN)
        co_return boost::system::errc::make_error_code(
            boost::system::errc::bad_message
        );
    
    std::span<std::byte, X25519_LEN> peer_public_key(
        reinterpret_cast<std::byte*>(peer_handshake_packet.payload),
        X25519_LEN
    );

    ec = crypto_context_.set_peer_public_key(peer_public_key);
    if(ec)
        co_return ec;
    
    connection_encrypted_ = true;
    std::cout << "handshake successful\n";
    co_return ec;
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::handshake_responder()
{
    if(!loops_started)
    {
        std::cerr << "ProtoHopZ: loops not running\n";
        co_return boost::system::errc::make_error_code(
            boost::system::errc::operation_canceled
        );
    }

    auto ec = crypto_context_.init();
    if(ec)
        co_return ec;

    PHZ::Packet peer_handshake_packet;
    ec = co_await receive_packet(&peer_handshake_packet);
    if(ec)
        co_return ec;
   
    if(peer_handshake_packet.header.size != X25519_LEN)
        co_return boost::system::errc::make_error_code(
            boost::system::errc::bad_message
        );
    
    std::span<std::byte, X25519_LEN> peer_public_key(
        reinterpret_cast<std::byte*>(peer_handshake_packet.payload),
        X25519_LEN
    );

    ec = crypto_context_.set_peer_public_key(peer_public_key);
    if(ec)
        co_return ec;
    

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

    ec = co_await send_packet(&self_handshake_packet);
    if(ec)
        co_return ec;

    connection_encrypted_ = true;
    std::cout << "handshake successful\n";
    co_return ec;
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::send_packet(const PHZ::Packet* source)
{
    boost::system::error_code ec;
    auto executor = co_await boost::asio::this_coro::executor;

    while(in_flight_.size() >= static_cast<std::size_t>(cwnd_.get()))
    {
        co_await cwnd_.wait(
            boost::asio::redirect_error(
                boost::asio::use_awaitable,
                ec
            )
        );

        if(ec)
            co_return ec;
    }

    auto packet = *source;
    packet.header.sequence = sequence_counter_++;

    if(connection_encrypted_)
    {
        if(packet.header.size > PHZ::PACKET_PAYLOAD_SIZE)
            co_return boost::system::errc::make_error_code(
                boost::system::errc::message_size
            );

        std::span<std::byte> data_to_encrypt(
            reinterpret_cast<std::byte*>(packet.payload),
            packet.header.size
        );

        auto result = crypto_context_.encrypt_data(data_to_encrypt);
        if(!result)
            co_return boost::system::errc::make_error_code(
                boost::system::errc::bad_address
            );

        if(result.value().size() > PHZ::PACKET_SIZE)
            co_return boost::system::errc::make_error_code(
                boost::system::errc::message_size
            );

        std::memcpy(PHZ::payload_region(packet), result.value().data(), result.value().size());
        packet.header.size = static_cast<uint16_t>(result.value().size());
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
        co_return ec;
    
    in_flight_[packet.header.sequence] =
    {std::chrono::steady_clock::now(), packet};

   co_return ec;
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::receive_packet(PHZ::Packet* destination)
{
    boost::system::error_code ec;

    PHZ::Packet packet;
    packet = co_await received_packets_queue_.pop(
        boost::asio::redirect_error(
            boost::asio::use_awaitable,
            ec
        )
    );

    if(ec)
    {
        destination = nullptr;
        co_return ec;
    }

    *destination = packet;

    co_return boost::system::error_code{};
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::resend_packet(uint32_t sequense)
{
    boost::system::error_code ec;

    auto it = in_flight_.find(sequense);
    if(it == in_flight_.end())
        co_return ec;
    
    co_await socket_.async_send_to(
        boost::asio::buffer(
            &it->second.packet,
            sizeof(PHZ::PacketHeader) + it->second.packet.header.size
        ),
        peer_endpoint_,
        boost::asio::redirect_error(boost::asio::use_awaitable, ec)
    );

    it->second.send_time = std::chrono::steady_clock::now();

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
            co_return ec;

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
                    std::cerr << "data size > PACKET_SIZE\n";
                    co_return boost::system::errc::make_error_code(
                        boost::system::errc::bad_message
                    );
                }

                send_ack(packet.header.sequence);

                if(received_packets_.contains(packet.header.sequence))
                    continue;
                if(connection_encrypted_)
                {
                    std::span<std::byte> data_to_decrypt(
                        reinterpret_cast<std::byte*>(PHZ::payload_region(packet)),
                        packet.header.size
                    );
                    
                    auto result = crypto_context_.decrypt_data(data_to_decrypt);
                    if(!result)
                    {
                        std::cerr << "decrypt error\n";
                        co_return boost::system::errc::make_error_code(
                            boost::system::errc::bad_address
                        );
                    }

                    packet.header.size = result.value().size();
                    std::memcpy(packet.payload, result.value().data(), packet.header.size);
                }
                received_packets_.insert(packet.header.sequence);
                received_packets_queue_.push(std::move(packet));

                break;
            }

            case PHZ::PacketType::HANDSHAKE :
            {
                if(packet.header.size > PHZ::PACKET_SIZE)
                {
                    std::cerr << "data size > PACKET_SIZE\n";
                    co_return boost::system::errc::make_error_code(
                        boost::system::errc::bad_message
                    );
                }

                send_ack(packet.header.sequence);

                if(received_packets_.contains(packet.header.sequence))
                    continue;

                received_packets_.insert(packet.header.sequence);
                received_packets_queue_.push(std::move(packet));

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
            sshthresh_ = std::max(cwnd_.get() / 2.0, 2.0);
            cwnd_.set(1.0);
        }

        for(uint32_t seq : timed_out)
        {
            if(!in_flight_.contains(seq))
                continue;

            ec = co_await resend_packet(seq);
            if(ec)
                co_return ec;
        }

        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec)
        );

        if(ec)
            co_return ec;
    }

    co_return ec;
}

void ProtoHopZ::send_ack(uint32_t sequence)
{
    boost::system::error_code ec;

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
        boost::asio::detached
    );
}

void ProtoHopZ::ack_handler(uint32_t sequence)
{
    auto erased = in_flight_.erase(sequence);
    // проверка на уже подтвержденные пакеты
    if(!erased) return;

    if(cwnd_.get() < sshthresh_)
        cwnd_.set(cwnd_.get() * 2);
    else
        cwnd_.set(cwnd_.get() + 1.0);
}
