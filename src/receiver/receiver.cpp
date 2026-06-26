#include "receiver/receiver.hpp"
#include "ftp_packet.hpp"
#include <cstring>
#include <iostream>

namespace
{
    inline std::filesystem::path extract_path(const FTProto::Packet& packet)
    {
        return std::filesystem::path(std::string(packet.data, packet.header.size));
    }

    boost::system::error_code extract_symlink(
        const FTProto::Packet& packet,
        std::filesystem::path& relative_path,
        std::filesystem::path& target
    )
    {
        auto payload_size = packet.get_payload_size();
        if(payload_size < sizeof(uint16_t))
            return boost::system::errc::make_error_code(boost::system::errc::bad_message);

        uint16_t link_path_size = 0;
        std::memcpy(&link_path_size, packet.data, sizeof(link_path_size));

        if(link_path_size > payload_size - sizeof(uint16_t))
            return boost::system::errc::make_error_code(boost::system::errc::bad_message);

        auto link_path = packet.data + sizeof(uint16_t);
        auto target_path = link_path + link_path_size;
        auto target_size = payload_size - sizeof(uint16_t) - link_path_size;

        relative_path = std::filesystem::path(std::string(link_path, link_path_size));
        target = std::filesystem::path(std::string(target_path, target_size));

        return {};
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
    if(chunk.empty())
        co_return boost::system::errc::make_error_code(
            boost::system::errc::bad_address
        );

    if(chunk.size_ < sizeof(FTProto::PacketHeader))
        co_return boost::system::errc::make_error_code(boost::system::errc::bad_message);

    FTProto::Packet packet;
    std::memcpy(&packet.header, chunk.data_.get(), sizeof(FTProto::PacketHeader));
    if(chunk.size_ < FTProto::Packet::serialized_size(packet))
        co_return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    if(packet.get_payload_size() < sizeof(uint64_t))
        co_return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    std::memcpy(packet.data, chunk.data_.get() + sizeof(FTProto::PacketHeader), packet.header.size);
    std::memcpy(&bytes_to_transfer_, packet.get_payload(), sizeof(uint64_t));

    std::cout << "Receive files size = " << bytes_to_transfer_ << "\n";
    std::string confirm;
    while(true)
    {
        std::cout << "Enter 'y' to continue or 'n' to exit: ";
        std::getline(std::cin, confirm);
        if(confirm == "Y" || confirm == "y")
        {
            FTProto::Packet confirm_packet;
            confirm_packet.header =
            {
                .type = FTProto::PacketType::CONFIRM,
                .flags = {},
                .size = 0,
                .file_id = 0
            };

            ec = co_await protostream_.send(FTProto::Packet::as_bytes(confirm_packet));
            if(ec)
            {
                std::cerr << ec.message() << "\n";
                co_return ec;
            }

            co_return co_await start_transfer();
        }
        else if(confirm == "n" || confirm == "N")
        {
            FTProto::Packet confirm_packet;
            confirm_packet.header =
            {
                .type = FTProto::PacketType::CONFIRM_FAILED,
                .flags = {},
                .size = 0,
                .file_id = 0
            };

            ec = co_await protostream_.send(FTProto::Packet::as_bytes(confirm_packet));
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
    while(!end_transfer_flag_)
    {
        auto chunk = co_await protostream_.receive();

        if(chunk.size_ < sizeof(FTProto::PacketHeader))
            co_return boost::system::errc::make_error_code(boost::system::errc::bad_message);

        FTProto::Packet packet;
        std::memcpy(&packet.header, chunk.data_.get(), sizeof(FTProto::PacketHeader));
        if(chunk.size_ < FTProto::Packet::serialized_size(packet))
            co_return boost::system::errc::make_error_code(boost::system::errc::bad_message);
        std::memcpy(packet.data, chunk.data_.get() + sizeof(FTProto::PacketHeader), packet.header.size);

        auto ec = handle_packet(std::move(packet));
        if(ec)
            co_return ec;
    }

    co_await protostream_.close();

    co_return boost::system::error_code();
}

boost::system::error_code Receiver::handle_packet(FTProto::Packet packet)
{
    switch(packet.header.type)
    {
        case FTProto::PacketType::CREATE_DIRECTORY:
        {
            auto ec = file_builder_.create_directory(extract_path(packet));
            if(ec)
                return ec;

            break;
        }

        case FTProto::PacketType::CREATE_FILE:
        {
            auto ec = file_builder_.create_file(extract_path(packet), packet.header.file_id);
            if(ec)
                return ec;
            
            break;
        }

        case FTProto::PacketType::CREATE_SYMLINK:
        {
            std::filesystem::path relative_path;
            std::filesystem::path target;
            auto ec = extract_symlink(packet, relative_path, target);
            if(ec)
                return ec;

            ec = file_builder_.create_symlink(relative_path, target);
            if(ec)
                return ec;

            break;
        }

        case FTProto::PacketType::FILE_DATA:
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

        case FTProto::PacketType::END_FILE:
        {
            auto ec = file_builder_.close_file(packet.header.file_id);
            if(ec)
                return ec;

            break;
        }

        case FTProto::PacketType::END_TRANSFER:
        {
            if(bytes_to_transfer_ != 0)
            {
                std::cerr << "END_TRANSFER received before all file data: "
                << bytes_to_transfer_ << " bytes left\n";
                return boost::system::errc::make_error_code(boost::system::errc::bad_message);
            }

            end_transfer_flag_ = true;

            break;
        }

        default:
        {std::cerr << "unknown packet type\n";}
    }
    
    return {};
}
