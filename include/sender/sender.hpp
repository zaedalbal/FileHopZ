#pragma once
#include "file_transfer.hpp"
#include "utils/file_walker.hpp"
#include <fstream>
#include <boost/asio.hpp>
#include <unordered_map>
#include <chrono>

class Sender : public File_transfer
{
    public:
        // constructor
        explicit Sender(
            boost::asio::io_context& context,
            const std::string& ip,
            unsigned short port,
            std::filesystem::path& files_to_send
        );

        boost::asio::awaitable<boost::system::error_code> 
        start() override;
    
    private:
        boost::asio::awaitable<boost::system::error_code>
        transfer_confirmation() override;

        boost::asio::awaitable<boost::system::error_code>
        start_transfer() override;

        boost::asio::awaitable<boost::system::error_code>
        path_handler(const std::filesystem::path& file);

    private:
        uint32_t file_id_counter_ = 0;

        std::filesystem::path& files_to_send_;

        File_walker file_walker_;
};