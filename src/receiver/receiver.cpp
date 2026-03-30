#include "receiver/receiver.hpp"
#include "packet.hpp"
#include <iostream>

Receiver::Receiver(boost::asio::io_context& context, unsigned short port, std::filesystem::path& output_directory)
: Data_transfer(context, port), output_directory_(output_directory), file_builder_(output_directory_)
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
    ec = receive_packet(&packet, nullptr);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        return ec;
    }
    std::memcpy(&bytes_to_transfer_, packet.get_payload(), sizeof(uint64_t));
    std::cout << "Receive files size = " << bytes_to_transfer_ << "\n";
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
            ec = send_packet(&confirm_packet, nullptr);
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
            ec = send_packet(&confirm_packet, nullptr);
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
    std::cout << "receiver ok\n";
    return {};
}