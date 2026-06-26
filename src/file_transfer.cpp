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
        ),
        ProtoStream::HANDSHAKE_MODE::RESPONDER
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
            boost::asio::ip::make_address(peer_adress),
            peer_port
        ),
        ProtoStream::HANDSHAKE_MODE::INITIATOR
    )
{}

boost::system::error_code File_transfer::print_progress(std::size_t bytes_transferred)
{
    if(bytes_to_transfer_ == 0)
        return {};

    bytes_transferred_ += bytes_transferred;
    auto new_progress = static_cast<uint16_t>((bytes_transferred_ * 100) / bytes_to_transfer_);
    if(new_progress > 100)
        new_progress = 100;

    if(new_progress > last_progress_)
    {
        std::cout << "\rProgress: " << new_progress << "%" << std::flush;
        if(new_progress == 100)
            std::cout << "\n";

        last_progress_ = new_progress;
    }
    
    return {};
}
