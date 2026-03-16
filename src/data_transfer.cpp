#include "data_transfer.hpp"
#include <iostream>

boost::system::error_code Data_transfer::send_packet(PacketHeader* source, std::size_t* bytes_transferred_ptr)
{
    boost::system::error_code ec;
    if(!source)
        return boost::asio::error::invalid_argument;
    if(source->size > PACKET_SIZE)
        return boost::asio::error::message_size;
    auto bytes_transferred = socket_.send_to(boost::asio::buffer(source, sizeof(PacketHeader)),
    peer_endpoint_, 0, ec);
    if(bytes_transferred_ptr)
        *bytes_transferred_ptr = bytes_transferred;
    return ec;
}

boost::system::error_code Data_transfer::send_packet(Packet* source, std::size_t* bytes_transferred_ptr)
{
    boost::system::error_code ec;
    if(!source)
        return boost::asio::error::invalid_argument;
    if(source->header.size > PACKET_SIZE)
        return boost::asio::error::message_size;
    auto bytes_transferred = socket_.send_to(boost::asio::buffer(source, sizeof(PacketHeader) + source->header.size),
    peer_endpoint_, 0, ec);
    if(bytes_transferred_ptr)
        *bytes_transferred_ptr = bytes_transferred;
    return ec;
}

boost::system::error_code Data_transfer::receive_packet(PacketHeader* destination, std::size_t* bytes_transferred_ptr)
{
    boost::system::error_code ec;
    if(!destination)
        return boost::asio::error::invalid_argument;
    auto bytes_transferred = socket_.receive_from(boost::asio::buffer(destination, sizeof(PacketHeader)), peer_endpoint_, 0, ec);
    if(destination->size > PACKET_SIZE)
        return boost::asio::error::message_size;
    if(bytes_transferred_ptr)
        *bytes_transferred_ptr = bytes_transferred;
    return ec;
}

boost::system::error_code Data_transfer::receive_packet(Packet* destination, std::size_t* bytes_transferred_ptr)
{
    boost::system::error_code ec;
    if(!destination)
        return boost::asio::error::invalid_argument;
    auto bytes_transferred = socket_.receive_from(boost::asio::buffer(destination, sizeof(Packet)), peer_endpoint_, 0, ec);
    if(destination->header.size > PACKET_SIZE)
        return boost::asio::error::message_size;
    if(bytes_transferred_ptr)
        *bytes_transferred_ptr = bytes_transferred;
    return ec;
}

boost::system::error_code Data_transfer::print_progress(std::size_t bytes_transferred)
{
    static std::size_t total_bytes_transferred = 0;
    static uint8_t last_progress = 0;
    total_bytes_transferred += bytes_transferred;
    uint8_t new_progress = (total_bytes_transferred * 100) / bytes_to_transfer;
    if(new_progress > last_progress)
    {
        std::cout << "Progress: " << new_progress << "%\n";
        last_progress = new_progress;
    }
    return {};
}