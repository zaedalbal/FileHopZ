#include <sender/sender.hpp>
#include "errors/filehopz_error.hpp"
#include <ftp_packet.hpp>
#include <cstring>
#include <iostream>

Sender::Sender(
    boost::asio::io_context& context,
    const std::string& ip,
    unsigned short port,
    std::filesystem::path& files_to_send
)
:   File_transfer(context, ip, port), files_to_send_(files_to_send), file_walker_(files_to_send_)
{}

boost::asio::awaitable<boost::system::error_code>
Sender::start()
{
    auto ec = co_await transfer_confirmation();
    co_return ec;
}

boost::asio::awaitable<boost::system::error_code>
Sender::transfer_confirmation()
{
    boost::system::error_code ec;

    while(file_walker_.next())
    {
        auto status = std::filesystem::symlink_status(file_walker_.current_path());
        if(std::filesystem::is_regular_file(status))
            bytes_to_transfer_ += std::filesystem::file_size(file_walker_.current_path());
    }
    file_walker_.reset();
    
    FTProto::Packet packet;
    packet.header = {};
    packet.set_payload(&bytes_to_transfer_, sizeof(bytes_to_transfer_));

    ec = co_await protostream_.send(FTProto::Packet::as_bytes(packet));
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        co_return ec;
    }

    auto chunk = co_await protostream_.receive();
    if(chunk.size_ < sizeof(FTProto::PacketHeader))
        co_return filehopz::Error_code::malformed_packet;

    std::memcpy(&packet.header, chunk.data_.get(), sizeof(FTProto::PacketHeader));

    if(packet.header.type == FTProto::PacketType::CONFIRM)
        co_return co_await start_transfer();
    else
    {
        std::cout << "Receiver refused files\n";
        co_return filehopz::Error_code::receiver_refused_transfer;
    }
}

boost::asio::awaitable<boost::system::error_code>
Sender::start_transfer()
{
    while(file_walker_.next())
    {
        auto ec = co_await path_handler(file_walker_.current_path());
        if(ec)
            co_return ec;
    }

    FTProto::Packet end_transfer_packet;
    end_transfer_packet.header =
    {
        .type = FTProto::PacketType::END_TRANSFER,
        .flags = {},
        .size = 0,
        .file_id = 0
    };

    auto ec = co_await protostream_.send(FTProto::Packet::as_bytes(end_transfer_packet));
    if(ec)
        co_return ec;

    co_await protostream_.close();
    co_return boost::system::error_code();
}

boost::asio::awaitable<boost::system::error_code>
Sender::path_handler(const std::filesystem::path& file)
{
    auto status = std::filesystem::symlink_status(file);

    if(std::filesystem::is_symlink(status))
    {
        auto relative_path = file_walker_.relative_path().string();
        auto target = std::filesystem::read_symlink(file).string();
        auto payload_size = sizeof(uint16_t) + relative_path.size() + target.size();
        if(payload_size > FTProto::PACKET_SIZE)
        {
            std::cerr << "symlink payload is too long: " << file.string() << "\n";
            co_return filehopz::Error_code::symlink_payload_too_large;
        }

        FTProto::Packet packet;
        auto link_path_size = static_cast<uint16_t>(relative_path.size());
        // Payload: длина пути ссылки, путь ссылки, затем target как вернул read_symlink
        std::memcpy(packet.data, &link_path_size, sizeof(link_path_size));
        std::memcpy(packet.data + sizeof(link_path_size), relative_path.data(), relative_path.size());
        std::memcpy(
            packet.data + sizeof(link_path_size) + relative_path.size(),
            target.data(),
            target.size()
        );
        packet.header =
        {
            .type = FTProto::PacketType::CREATE_SYMLINK,
            .flags = {},
            .size = static_cast<uint16_t>(payload_size),
            .file_id = 0
        };

        auto ec = co_await protostream_.send(FTProto::Packet::as_bytes(packet));
        if(ec)
            co_return ec;

        co_return boost::system::error_code();
    }

    switch(status.type())
    {
       case std::filesystem::file_type::directory:
       {
            auto file_id = file_id_counter_++;
            auto relative_path = file_walker_.relative_path().string();

            FTProto::Packet packet;
            packet.header =
            {
                .type = FTProto::PacketType::CREATE_DIRECTORY,
                .flags = {},
                .size = 0,
                .file_id = file_id
            };
            packet.set_payload(relative_path.data(), relative_path.size());

            auto ec = co_await protostream_.send(FTProto::Packet::as_bytes(packet));
            if(ec)
                co_return ec;

            break;
       }

       case std::filesystem::file_type::regular:
       {
            std::ifstream file_stream(file, std::ios::binary);
            if(!file_stream)
                co_return filehopz::Error_code::file_open_failed;
            
            auto file_id = file_id_counter_++;
            std::size_t bytes_read = 0;
            auto relative_path = file_walker_.relative_path().string();
            
            FTProto::Packet packet_create_file;
            packet_create_file.set_payload(relative_path.data(), relative_path.size());
            packet_create_file.header.type = FTProto::PacketType::CREATE_FILE;
            packet_create_file.header.flags = {};
            packet_create_file.header.file_id = file_id;

            auto error = co_await protostream_.send(FTProto::Packet::as_bytes(packet_create_file));
            if(error)
                co_return error;

            do
            {
                FTProto::Packet packet;
                packet.header =
                {
                    .type = FTProto::PacketType::FILE_DATA,
                    .flags = {},
                    .size = 0,
                    .file_id = file_id
                };
                
                file_stream.read(packet.data, FTProto::PACKET_SIZE);
                bytes_read = file_stream.gcount();
                packet.header.size = static_cast<uint16_t>(bytes_read);

                auto ec = co_await protostream_.send(FTProto::Packet::as_bytes(packet));
                if(ec)
                    co_return ec;

                ec = print_progress(bytes_read);
                if(ec)
                    co_return ec;

            }
            while(bytes_read == FTProto::PACKET_SIZE);

            FTProto::Packet packet;
            packet.header =
            {
                .type = FTProto::PacketType::END_FILE,
                .flags = {},
                .size = 0,
                .file_id = file_id
            };
            
            auto ec = co_await protostream_.send(FTProto::Packet::as_bytes(packet));
            if(ec)
                co_return ec;

            break;
       }

       default:
       {
            std::cerr << "unknown file type\n";

            co_return filehopz::Error_code::unsupported_file_type;
       }
    }

    co_return boost::system::error_code();
}
