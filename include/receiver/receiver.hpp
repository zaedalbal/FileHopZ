#pragma once
#include "file_transfer.hpp"
#include "ftp_packet.hpp"
#include "utils/file_builder.hpp"
#include <boost/asio.hpp>
#include <memory>

class Receiver : public File_transfer
{
    public:
        explicit Receiver(
            boost::asio::io_context& context,
            unsigned short port,
            std::filesystem::path& output_file
        );

        boost::asio::awaitable<boost::system::error_code>
        start() override;
    
    private:
        boost::asio::awaitable<boost::system::error_code>
        transfer_confirmation() override;

        boost::asio::awaitable<boost::system::error_code>
        start_transfer() override;

        boost::system::error_code handle_packet(FTProto::Packet packet);
    
    private:
        bool end_transfer_flag_ = false;

        uint64_t bytes_remaining_ = 0;

        std::filesystem::path& output_directory_;

        std::unique_ptr<File_builder> file_builder_;
};
