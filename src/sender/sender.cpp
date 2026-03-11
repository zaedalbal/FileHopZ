#include <sender/sender.hpp>
#include <packet.hpp>
#include <iostream>

Sender::Sender(boost::asio::io_context& context, const std::string& ip, unsigned short port, std::ifstream& file)
: context_(context), receiver_address_(ip), receiver_port_(port), file_to_send_(file),
socket_(context_, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0))
{
    std::cout << "Sender constructor called\n";
    start();
}

boost::system::error_code Sender::start()
{
    auto ec = send_file();
    if(!ec)
        std::cout << "File successfully sent!\n";
    return ec;
}

boost::system::error_code Sender::send_file()
{
    std::error_code return_code;
    boost::system::error_code ec;
    boost::asio::ip::udp::endpoint receiver(boost::asio::ip::make_address(receiver_address_), receiver_port_);
    boost::asio::steady_timer timer(context_);
    socket_.non_blocking(true);
    uint32_t seq = 0;
    while (true)
    {
        Packet packet;
        file_to_send_.read(packet.data, PACKET_SIZE);
        packet.sequense = seq;
        packet.size = file_to_send_.gcount();

        socket_.send_to(boost::asio::buffer(&packet, sizeof(uint32_t) + sizeof(uint16_t) + packet.size), receiver);

        timer.expires_after(std::chrono::milliseconds(TIMEOUT));
        bool ack_received = false;

        while(!ack_received)
        {
            uint32_t ack;
            auto len = socket_.receive_from(boost::asio::buffer(&ack, sizeof(ack)), receiver, 0, ec);
            if(!ec && ack == seq)
                ack_received = true;
            else
            {
                if(timer.expiry() <= std::chrono::steady_clock::now())
                {
                    socket_.send_to(boost::asio::buffer(&packet, sizeof(uint32_t) + sizeof(uint16_t) + packet.size), receiver);
                    timer.expires_after(std::chrono::milliseconds(TIMEOUT));
                }
            }
        }
        ++seq;
        if(packet.size < PACKET_SIZE)
            break;
    }
    Packet end_packet;
    end_packet.sequense = seq;
    end_packet.size = 0;
    socket_.send_to(boost::asio::buffer(&end_packet, sizeof(uint32_t) + sizeof(uint16_t)), receiver);
    return ec;
}