#pragma once
#include <boost/asio.hpp>

class Data_transfer
{
    public:
        explicit Data_transfer
        (boost::asio::io_context& context, unsigned short port)
        : context_(context),
        socket_(context_, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)){}

        explicit Data_transfer
        (boost::asio::io_context& context, const std::string& peer_adress, unsigned short peer_port)
        : context_(context),
        socket_(context_, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)),
        peer_endpoint_(boost::asio::ip::make_address(peer_adress), peer_port){}

        virtual ~Data_transfer() = default;

        virtual boost::system::error_code start() = 0;

    protected:
        virtual boost::system::error_code transfer_confirmation() = 0;

        virtual boost::system::error_code start_transfer() = 0;

        virtual boost::system::error_code send_packet(); // пока что нет реализации

        virtual boost::system::error_code receive_packet(); // пока что нет реализации

    protected:
        boost::asio::io_context& context_;

        boost::asio::ip::udp::socket socket_;

        boost::asio::ip::udp::endpoint peer_endpoint_;
};