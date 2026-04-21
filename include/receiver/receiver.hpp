#pragma once
#include "file_transfer.hpp"
#include "utils/file_builder.hpp"
#include <boost/asio.hpp>

class Receiver : public File_transfer
{
    public:
        explicit Receiver(boost::asio::io_context& context, unsigned short port, std::filesystem::path& output_file);

        boost::asio::awaitable<boost::system::error_code>
        start() override;
    
    private:
        boost::asio::awaitable<boost::system::error_code>
        transfer_confirmation() override;

        boost::asio::awaitable<boost::system::error_code>
        start_transfer() override;
    
    private:
        std::filesystem::path& output_directory_;

        File_builder file_builder_;
};