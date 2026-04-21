#include <sender/sender.hpp>
#include <packet.hpp>
#include <iostream>

Sender::Sender(boost::asio::io_context& context, const std::string& ip, unsigned short port, std::filesystem::path& files_to_send)
: File_transfer(context, ip, port), files_to_send_(files_to_send), file_walker_(files_to_send_)
{
    std::cout << "Sender constructor called\n";
}

boost::asio::awaitable<boost::system::error_code>
Sender::start()
{
    co_return co_await transfer_confirmation();
}

boost::asio::awaitable<boost::system::error_code>
Sender::transfer_confirmation()
{
/*
    boost::system::error_code ec;

    while(file_walker_.next())
    {
        bytes_to_transfer_ += std::filesystem::file_size(file_walker_.current_path());
    }
    file_walker_.reset();
    
    Packet packet;
    packet.set_payload(&bytes_to_transfer_, sizeof(bytes_to_transfer_));

    ec = co_await send_packet(&packet, nullptr);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        co_return ec;
    }
    ec = co_await receive_packet(&packet, nullptr);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        co_return ec;
    }
    if(packet.header.type == PacketType::CONFIRM)
        co_return co_await start_transfer();
    else
    {
        std::cout << "Receiver refused files\n";
        co_return ec;
    }
*/
}

boost::asio::awaitable<boost::system::error_code>
Sender::start_transfer()
{
    std::cout << "sender ok\n";

    co_return boost::system::error_code();
}