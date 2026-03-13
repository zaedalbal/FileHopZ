#include <sender/sender.hpp>
#include <packet.hpp>
#include <iostream>

Sender::Sender(boost::asio::io_context& context, const std::string& ip, unsigned short port, std::ifstream& file)
: context_(context), receiver_address_(ip), receiver_port_(port), file_to_send_(file),
socket_(context_, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)),
receiver_endpoint_(boost::asio::ip::make_address(receiver_address_), receiver_port_)
{
    std::cout << "Sender constructor called\n";
    start();
}

boost::system::error_code Sender::start()
{
    return wait_confirm();
}

boost::system::error_code Sender::wait_confirm()
{
    boost::system::error_code ec;
    int64_t filesize;
    file_to_send_.seekg(0, std::ios::end);
    filesize = file_to_send_.tellg();
    file_to_send_.seekg(0, std::ios::beg);
    
    Packet packet;
    std::memcpy(packet.data, &filesize, sizeof(filesize));

    socket_.send_to(boost::asio::buffer(&packet, sizeof(packet)), receiver_endpoint_, 0, ec);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        return ec;
    }
    socket_.receive_from(boost::asio::buffer(&packet, sizeof(packet)), receiver_endpoint_, 0, ec);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        return ec;
    }
    if(packet.type == PacketType::CONFIRM)
        return send_file();
    else
        return ec;
}

boost::system::error_code Sender::send_file()
{
    std::error_code return_code;
    boost::system::error_code ec;
    boost::asio::steady_timer timer(context_);
    socket_.non_blocking(true);
    uint32_t seq = 0;
    while (true)
    {
        Packet packet;
        file_to_send_.read(packet.data, PACKET_SIZE);
        packet.sequense = seq;
        packet.size = file_to_send_.gcount();

        socket_.send_to(boost::asio::buffer(&packet, PacketHeader + packet.size), receiver_endpoint_);

        timer.expires_after(std::chrono::milliseconds(TIMEOUT));
        bool ack_received = false;

        while(!ack_received)
        {
            uint32_t ack;
            auto len = socket_.receive_from(boost::asio::buffer(&ack, sizeof(ack)), receiver_endpoint_, 0, ec);
            if(!ec && ack == seq)
                ack_received = true;
            else
            {
                if(timer.expiry() <= std::chrono::steady_clock::now())
                {
                    socket_.send_to(boost::asio::buffer(&packet, PacketHeader + packet.size), receiver_endpoint_);
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
    socket_.send_to(boost::asio::buffer(&end_packet, PacketHeader), receiver_endpoint_);
    return ec;
}