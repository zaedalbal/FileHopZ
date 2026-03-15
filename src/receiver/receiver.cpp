#include "receiver/receiver.hpp"
#include "packet.hpp"
#include <iostream>

Receiver::Receiver(boost::asio::io_context& context, unsigned short port, std::ofstream& output_file)
: Data_transfer(context, port), output_file_(output_file)
{
    std::cout << "Receiver constructor called\n";
    start();
}

boost::system::error_code Receiver::start()
{
    return transfer_confirmation();
}

boost::system::error_code Receiver::transfer_confirmation()
{
    boost::system::error_code ec;
    Packet packet;
    socket_.receive_from(boost::asio::buffer(&packet, sizeof(packet)), peer_endpoint_, 0, ec);
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
            confirm_packet.header.type = PacketType::CONFIRM;
            confirm_packet.header.size = 0;
            socket_.send_to(boost::asio::buffer(&confirm_packet, sizeof(confirm_packet)), peer_endpoint_, 0 ,ec);
            if(ec)
            {
                std::cerr << ec.message() << "\n";
                return ec;
            }
            return start_transfer();
        }
        else if(confirm == "n" || confirm == "N")
        {
            Packet confirm_packet;
            confirm_packet.header.type = PacketType::CONFIRM_FAILED;
            confirm_packet.header.size = 0;
            socket_.send_to(boost::asio::buffer(&confirm_packet, sizeof(confirm_packet)), peer_endpoint_, 0 ,ec);
            if(ec)
            {
                std::cerr << ec.message() << "\n";
                return ec;
            }
            return ec;
        }
    }
}

boost::system::error_code Receiver::start_transfer()
{
    boost::system::error_code ec;

    uint32_t expected_seq = 0;

    while(true)
    {
        Packet packet;
        auto len = socket_.receive_from(boost::asio::buffer(&packet, sizeof(packet)), peer_endpoint_, 0, ec);
        if(ec)
        {
            std::cerr << ec.message() << "\n";
            return ec;
        }
        if(packet.header.size == 0)
        {
            break;
        }
        if(packet.header.sequence == expected_seq)
        {
            output_file_.write(packet.data, packet.header.size);
            ++expected_seq;
        }
        socket_.send_to(boost::asio::buffer(&packet.header.sequence, sizeof(packet.header.sequence)), peer_endpoint_, 0, ec);
        if(ec)
        {
            std::cerr << ec.message() << "\n";
            return ec;
        }
    }
    output_file_.close();
    return ec;
}