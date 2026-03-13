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
    return confirmation_request();
}

boost::system::error_code Receiver::confirmation_request()
{
    boost::system::error_code ec;
    Packet packet;
    socket_.receive_from(boost::asio::buffer(&packet, sizeof(packet)), sender_endpoint_, 0, ec);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        return ec;
    }
    std::memcpy(&receive_file_size_, packet.data, sizeof(uint64_t));
    std::cout << "Receive file size = " << receive_file_size_ << "\n";
    std::string confirm;
    while(true)
    {
        std::cout << "Enter 'y' to continue or 'n' to exit: ";
        std::getline(std::cin, confirm);
        if(confirm == "Y" || confirm == "y")
        {
            Packet confirm_packet;
            confirm_packet.type = PacketType::CONFIRM;
            confirm_packet.size = 0;
            socket_.send_to(boost::asio::buffer(&confirm_packet, sizeof(confirm_packet)), sender_endpoint_, 0 ,ec);
            if(ec)
            {
                std::cerr << ec.message() << "\n";
                return ec;
            }
            return start_receive_file();
        }
        else if(confirm == "n" || confirm == "N")
        {
            Packet confirm_packet;
            confirm_packet.type = PacketType::CONFIRM_FAILED;
            confirm_packet.size = 0;
            socket_.send_to(boost::asio::buffer(&confirm_packet, sizeof(confirm_packet)), sender_endpoint_, 0 ,ec);
            if(ec)
            {
                std::cerr << ec.message() << "\n";
                return ec;
            }
            return ec;
        }
    }
}

boost::system::error_code Receiver::start_receive_file()
{
    boost::system::error_code ec;

    uint32_t expected_seq = 0;

    while(true)
    {
        Packet packet;
        auto len = socket_.receive_from(boost::asio::buffer(&packet, sizeof(packet)), sender_endpoint_, 0, ec);
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
        socket_.send_to(boost::asio::buffer(&packet.sequense, sizeof(packet.sequense)), sender_endpoint_, 0, ec);
        if(ec)
        {
            std::cerr << ec.message() << "\n";
            return ec;
        }
    }
    output_file_.close();
    return ec;
}