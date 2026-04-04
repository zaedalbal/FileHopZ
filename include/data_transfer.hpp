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
        {}

        explicit Data_transfer
        (boost::asio::io_context& context, const std::string& peer_adress, unsigned short peer_port)
        : context_(context),
        socket_(context_, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)),
        peer_endpoint_(boost::asio::ip::make_address(peer_adress), peer_port)
        {}

        virtual ~Data_transfer() = default;

        virtual boost::asio::awaitable<boost::system::error_code> 
        start() = 0;

    protected:
        virtual boost::asio::awaitable<boost::system::error_code>
        transfer_confirmation() = 0;

        virtual boost::asio::awaitable<boost::system::error_code>
        start_transfer() = 0;

        template<typename T>
        boost::asio::awaitable<boost::system::error_code>
        send_packet(const T* source, std::size_t* bytes_transferred_ptr)
        {
            static_assert(std::is_same_v<T, Packet> || std::is_same_v<T, PacketHeader>,
            "Unsupporetd type");

            boost::system::error_code ec;

            if(!source)
                co_return boost::asio::error::invalid_argument;
            
            std::size_t buffer_size;

            if constexpr(std::is_same_v<T, Packet>)
            {
                if(source->header.size > PACKET_SIZE)
                    co_return boost::asio::error::message_size;
                buffer_size = sizeof(PacketHeader) + source->header.size;
            }
            else
            {
                if(source->size > PACKET_SIZE)
                    co_return boost::asio::error::message_size;
                buffer_size = sizeof(PacketHeader);
            }

            auto bytes_transferred = co_await socket_.async_send_to
            (boost::asio::buffer(source, buffer_size),
            peer_endpoint_,
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));

            if(bytes_transferred_ptr)
                *bytes_transferred_ptr = bytes_transferred;
            
            co_return ec;
        }

        template<typename T>
        boost::asio::awaitable<boost::system::error_code>
        receive_packet(T* destination, std::size_t* bytes_transferred_ptr)
        {
            static_assert(std::is_same_v<T, Packet> || std::is_same_v<T, PacketHeader>,
            "Unsupporetd type");

            boost::system::error_code ec;

            if(!destination)
                co_return boost::asio::error::invalid_argument;
            
            std::size_t buffer_size = sizeof(T);

            auto bytes_transferred = co_await socket_.async_receive_from
            (boost::asio::buffer(destination, buffer_size),
            peer_endpoint_,
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            
            if constexpr(std::is_same_v<T, PacketHeader>)
            {
                if(destination->size > PACKET_SIZE)
                    co_return boost::asio::error::message_size;
            }
            else
                if(destination->header.size > PACKET_SIZE)
                    co_return boost::asio::error::message_size;
            if(bytes_transferred_ptr)
                *bytes_transferred_ptr = bytes_transferred;

            co_return ec;
        }

        // error код всегда пустой, он сделан чтобы в будущем можно было че то добавить и не менять возращаемый тип
        virtual boost::system::error_code
        print_progress(std::size_t bytes_transferred);

    protected:
        boost::asio::io_context& context_;

        boost::asio::ip::udp::socket socket_;

        boost::asio::ip::udp::endpoint peer_endpoint_;

        uint64_t bytes_to_transfer_;
};