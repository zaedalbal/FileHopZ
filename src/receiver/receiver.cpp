#include "receiver/receiver.hpp"
#include "packet.hpp"
#include <iostream>

namespace
{
    inline std::filesystem::path extract_path(const Packet& packet)
    {
        return std::filesystem::path(std::string(packet.data, packet.header.size));
    }
}

Receiver::Receiver(
    boost::asio::io_context& context,
    unsigned short port,
    std::filesystem::path& output_directory
)
:   File_transfer(context, port),
    output_directory_(output_directory),
    file_builder_(output_directory_)
{}

boost::asio::awaitable<boost::system::error_code> Receiver::start()
{
    auto ec = co_await transfer_confirmation();

    co_return ec;
}

boost::asio::awaitable<boost::system::error_code> Receiver::transfer_confirmation()
{
    boost::system::error_code ec;
    auto chunk = co_await protostream_.receive();

    Packet packet;

    std::memcpy(&packet, chunk.data_.get(), sizeof(packet));
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

            ec = co_await protostream_.send(std::as_bytes(std::span{&confirm_packet, 1}));
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

            ec = co_await protostream_.send(std::as_bytes(std::span{&confirm_packet, 1}));
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

    while(!end_transfer_flag_)
    {
        auto chunk = co_await protostream_.receive();

        Packet packet;
        std::memcpy(&packet, chunk.data_.get(), sizeof(packet));

        auto ec = handle_packet(std::move(packet));
        if(ec)
            co_return ec;
    }

    co_await protostream_.close();

    co_return boost::system::error_code();
}

boost::system::error_code Receiver::handle_packet(Packet packet)
{
    switch(packet.header.type)
    {
        case PacketType::CREATE_DIRECTORY:
        {
            auto ec = file_builder_.create_directory(extract_path(packet));
            if(ec)
                return ec;

            break;
        }

        case PacketType::CREATE_FILE:
        {
            auto ec = file_builder_.create_file(extract_path(packet), packet.header.file_id);
            if(ec)
                return ec;
            
            break;
        }

        case PacketType::FILE_DATA:
        {
            if(bytes_to_transfer_ >= packet.get_payload_size())
            {
                auto ec =
                file_builder_.write(packet.get_payload(), packet.get_payload_size(), packet.header.file_id);
                if(ec)
                {
                    std::cerr << "ec: " << ec.what() << "\n";
                    return ec;
                }

                bytes_to_transfer_ -= packet.get_payload_size();
            }
            else
            {
                std::cerr << 
                "Sender is attempting to send more data than agreed upon: possible malicious input\n"
                << "bytes_to_transfer_ = " << bytes_to_transfer_ << "\n"
                << "bytes received in last packet = " << packet.get_payload_size() << "\n";

                return boost::system::errc::make_error_code(boost::system::errc::file_too_large);
            }

            break;
        }

        case PacketType::END_FILE:
        {
            auto ec = file_builder_.close_file(packet.header.file_id);
            if(ec)
                return ec;

            break;
        }

        case PacketType::END_TRANSFER:
        {
            end_transfer_flag_ = true;

            break;
        }

        default:
        {std::cerr << "unknown packet type\n";}
    }
    
    return {};
}