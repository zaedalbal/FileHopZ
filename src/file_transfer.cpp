#include "file_transfer.hpp"
#include <iostream>

File_transfer::File_transfer(
    boost::asio::io_context& context,
    unsigned short port
)
:   context_(context),
    protostream_(
        boost::asio::ip::udp::socket(
            context_, 
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)
        )
    )
{}

File_transfer::File_transfer(
    boost::asio::io_context& context,
    const std::string& peer_adress,
    unsigned short peer_port
)
:   context_(context),
    protostream_(
        boost::asio::ip::udp::socket(
            context_,
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)
        ),
        boost::asio::ip::udp::endpoint(
            boost::asio::ip::address::from_string(peer_adress),
            peer_port
        )
    )
{}

boost::system::error_code File_transfer::print_progress(std::size_t bytes_transferred)
{
    static std::size_t total_bytes_transferred = 0;
    static uint8_t last_progress = 0;

    total_bytes_transferred += bytes_transferred;
    uint8_t new_progress = (total_bytes_transferred * 100) / bytes_to_transfer_;

    if(new_progress > last_progress)
    {
        std::cout << "Progress: " << new_progress << "%\n";
        last_progress = new_progress;
    }
    
    return {};
}