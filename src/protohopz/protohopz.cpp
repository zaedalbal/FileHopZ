#include "protohopz/protohopz.hpp"
#include <iostream>

ProtoHopZ::ProtoHopZ(
    boost::asio::ip::udp::socket socket,
    boost::asio::ip::udp::endpoint peer_endpoint
)
:   socket_(std::move(socket)),
    peer_endpoint_(std::move(peer_endpoint)),
    received_packets_queue_(socket_.get_executor())
{}

void ProtoHopZ::start()
{
    auto executor = socket_.get_executor();
    running_ = true;

    boost::asio::co_spawn(executor, receive_loop(), boost::asio::detached);
    boost::asio::co_spawn(executor, timeout_loop(), boost::asio::detached);
}

void ProtoHopZ::stop()
{
    // в будущем переписать stop() так, чтобы отправлялись все пакеты из in_flight_,
    // и только после этого все завершалось
    
    boost::system::error_code ec;
    running_ = false;
    socket_.cancel(ec);
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::send_packet(const PHZ::Packet* source)
{
    boost::system::error_code ec;
    auto executor = co_await boost::asio::this_coro::executor;

    while(in_flight_.size() > static_cast<std::size_t>(cwnd_))
    {
        // в будущем поменять, тк эта штука очень сильно ест CPU!!!
        co_await boost::asio::post(executor, boost::asio::use_awaitable); 
    }

    auto packet = *source;
    packet.header.sequence = sequence_counter_++;

    co_await socket_.async_send_to(
    boost::asio::buffer(&packet, sizeof(PHZ::PacketHeader) + packet.header.size),
    peer_endpoint_, 
    boost::asio::redirect_error(boost::asio::use_awaitable, ec)
);

    if(ec)
        co_return ec;
    
    in_flight_[packet.header.sequence] =
    {std::chrono::steady_clock::now(), packet};

   co_return ec;
}

boost::asio::awaitable<void> ProtoHopZ::receive_packet(PHZ::Packet* destination)
{
    PHZ::Packet packet;
    packet = co_await received_packets_queue_.pop();
    *destination = packet;

    co_return;
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
    while(running_)
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
                co_await send_ack(packet.header.sequence);

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

    while(running_)
    {
        auto now = std::chrono::steady_clock::now();

        for(auto& [seq, packet] : in_flight_)
        {
            if(now - packet.send_time > PHZ::TIMEOUT)
            {
                sshthresh_ = cwnd_ / 2.0;
                cwnd_ = 1.0;

                ec = co_await resend_packet(seq);
                if(ec)
                    co_return ec;
            }
        }

        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait(boost::asio::use_awaitable);
    }

    co_return ec;
}

boost::asio::awaitable<boost::system::error_code>
ProtoHopZ::send_ack(uint32_t sequence)
{
    boost::system::error_code ec;

    PHZ::Packet packet{};
    packet.header.sequence = sequence;
    packet.header.type = PHZ::PacketType::ACK;
    packet.header.size = 0;

    co_await socket_.async_send_to
    (boost::asio::buffer(&packet, sizeof(PHZ::PacketHeader) + packet.header.size),
    peer_endpoint_,
    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if(ec) // проверка, если в будущем что то будет ещё добавляться
        co_return ec;

    co_return ec;
}

void ProtoHopZ::ack_handler(uint32_t sequence)
{
    in_flight_.erase(sequence);

    if(cwnd_ < sshthresh_)
        cwnd_ *= 2;
    else
        cwnd_ += 1.0 / cwnd_;
}