#include "receiver/receiver.hpp"
#include "packet.hpp"
#include <iostream>

Receiver::Receiver(boost::asio::io_context& context, unsigned short port, std::filesystem::path& output_directory)
: Data_transfer(context, port), output_directory_(output_directory), file_builder_(output_directory_)
{
    std::cout << "Receiver constructor called\n";
}

boost::asio::awaitable<boost::system::error_code> Receiver::start()
{
    co_return co_await transfer_confirmation();
}

boost::asio::awaitable<boost::system::error_code> Receiver::transfer_confirmation()
{
    boost::system::error_code ec;
    Packet packet;
    ec = co_await receive_packet(&packet, nullptr);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        co_return ec;
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
            ec = co_await send_packet(&confirm_packet, nullptr);
            if(ec)
            {
                std::cerr << ec.message() << "\n";
                co_return ec;
            }
            co_return co_await start_transfer();
        }
        else if(confirm == "n" || confirm == "N")
        {
            Packet confirm_packet;
            confirm_packet.header.type = PacketType::CONFIRM_FAILED;
            confirm_packet.header.size = 0;
            ec = co_await send_packet(&confirm_packet, nullptr);
            if(ec)
            {
                std::cerr << ec.message() << "\n";
                co_return ec;
            }
            co_return ec;
        }
    }
}

boost::asio::awaitable<boost::system::error_code> Receiver::start_transfer()
{
    std::cout << "receiver ok\n";
    co_return boost::system::error_code();
}