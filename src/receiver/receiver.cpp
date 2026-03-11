#include "receiver/receiver.hpp"
#include "packet.hpp"
#include <iostream>

Receiver::Receiver(boost::asio::io_context& context, unsigned short port, std::ofstream& output_file)
: context_(context), port_(port), output_file_(output_file),
socket_(context_, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port_))
{
    std::cout << "Receiver constructor called\n";
    start();
}

boost::system::error_code Receiver::start()
{
    auto ec = start_receive_file();
    if(!ec)
        std::cout << "File successfully received!\n";
    return ec;
}

boost::system::error_code Receiver::start_receive_file()
{
    boost::system::error_code ec;
    boost::asio::ip::udp::endpoint sender_endpoint;

    uint32_t expected_seq = 0;

    while(true)
    {
        Packet packet;
        auto len = socket_.receive_from(boost::asio::buffer(&packet, sizeof(packet)), sender_endpoint, 0, ec);
        if(ec)
        {
            std::cerr << ec.message() << "\n";
            return ec;
        }
        if(packet.size == 0)
        {
            break;
        }
        if(packet.sequense == expected_seq)
        {
            output_file_.write(packet.data, packet.size);
            ++expected_seq;
        }
        socket_.send_to(boost::asio::buffer(&packet.sequense, sizeof(packet.sequense)), sender_endpoint, 0, ec);
        if(ec)
        {
            std::cerr << ec.message() << "\n";
            return ec;
        }
    }
    output_file_.close();
    return ec;
}