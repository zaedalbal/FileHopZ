#pragma once
#include "data_transfer.hpp"
#include "utils/file_builder.hpp"
#include <boost/asio.hpp>

class Receiver : public Data_transfer
{
    public:
        explicit Receiver(boost::asio::io_context& context, unsigned short port, std::filesystem::path& output_file);

        boost::system::error_code start() override;
    
    private:
        boost::system::error_code transfer_confirmation() override;

        boost::system::error_code start_transfer() override;
    
    private:
        std::filesystem::path& output_directory_;

        File_builder file_builder_;
};