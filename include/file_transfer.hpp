#pragma once
#include <boost/asio.hpp>
#include "protohopz/protostream.hpp"
#include "packet.hpp"

class File_transfer
{
    public:
        // конструктор для Receiver
        explicit File_transfer(
            boost::asio::io_context& context,
            unsigned short port
        );

        // конструктор для Sender
        explicit File_transfer(
            boost::asio::io_context& context,
            const std::string& peer_adress,
            unsigned short peer_port
        );

        virtual ~File_transfer() = default;

        virtual boost::asio::awaitable<boost::system::error_code> 
        start() = 0;

    protected:
        virtual boost::asio::awaitable<boost::system::error_code>
        transfer_confirmation() = 0;

        virtual boost::asio::awaitable<boost::system::error_code>
        start_transfer() = 0;

        // error код всегда пустой, он сделан чтобы в будущем можно было че то добавить и не менять возращаемый тип
        virtual boost::system::error_code
        print_progress(std::size_t bytes_transferred);

    protected:
        boost::asio::io_context& context_;

        ProtoStream protostream_;

        uint64_t bytes_to_transfer_;
};