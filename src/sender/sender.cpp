#include <sender/sender.hpp>
#include <packet.hpp>
#include <iostream>

Sender::Sender(boost::asio::io_context& context, const std::string& ip, unsigned short port, std::filesystem::path& files_to_send)
: Data_transfer(context, ip, port), files_to_send_(files_to_send), file_walker_(files_to_send_)
{
    std::cout << "Sender constructor called\n";
    start();
}

boost::system::error_code Sender::start()
{
    return transfer_confirmation();
}

boost::system::error_code Sender::transfer_confirmation()
{
    boost::system::error_code ec;

    while(file_walker_.next())
    {
        bytes_to_transfer_ += std::filesystem::file_size(file_walker_.current_path());
    }
    file_walker_.reset();
    
    Packet packet;
    packet.set_payload(&bytes_to_transfer_, sizeof(bytes_to_transfer_));

    ec = send_packet(&packet, nullptr);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        return ec;
    }
    ec = receive_packet(&packet, nullptr);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        return ec;
    }
    if(packet.header.type == PacketType::CONFIRM)
        return start_transfer();
    else
    {
        std::cout << "Receiver refused files\n";
        return ec;
    }
}

boost::system::error_code Sender::start_transfer()
{
    std::cout << "sender ok\n";
    return {};
}