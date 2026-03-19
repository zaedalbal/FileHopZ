#pragma once
#include <boost/asio.hpp>
#include "packet.hpp"

class Data_transfer
{
    public:
        explicit Data_transfer
        (boost::asio::io_context& context, unsigned short port)
        : context_(context),
        socket_(context_, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port))
        {socket_.non_blocking(true);}

        explicit Data_transfer
        (boost::asio::io_context& context, const std::string& peer_adress, unsigned short peer_port)
        : context_(context),
        socket_(context_, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)),
        peer_endpoint_(boost::asio::ip::make_address(peer_adress), peer_port)
        {socket_.non_blocking(true);}

        virtual ~Data_transfer() = default;

        virtual boost::system::error_code start() = 0;

    protected:
        virtual boost::system::error_code transfer_confirmation() = 0;

        virtual boost::system::error_code start_transfer() = 0;

        virtual boost::system::error_code send_packet(Packet* source, std::size_t* bytes_transferred_ptr);
        virtual boost::system::error_code send_packet(PacketHeader* source, std::size_t* bytes_transferred_ptr);

        virtual boost::system::error_code receive_packet(Packet* destination, std::size_t* bytes_transferred_ptr);
        virtual boost::system::error_code receive_packet(PacketHeader* destination, std::size_t* bytes_transferred_ptr);

        // error код всегда пустой, он сделан чтобы в будущем можно было че то добавить и не менять возращаемый тип
        virtual boost::system::error_code print_progress(std::size_t bytes_transferred);

    protected:
        boost::asio::io_context& context_;

        boost::asio::ip::udp::socket socket_;

        boost::asio::ip::udp::endpoint peer_endpoint_;

        std::size_t bytes_to_transfer;
};