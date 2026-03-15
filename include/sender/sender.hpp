#pragma once
#include "data_transfer.hpp"
#include <fstream>
#include <boost/asio.hpp>

class Sender : public Data_transfer
{
    public:
        // constructor
        explicit Sender(boost::asio::io_context& context, const std::string& ip, unsigned short port, std::ifstream& file);

        boost::system::error_code start() override;
    
    private:
        boost::system::error_code transfer_confirmation() override;

        boost::system::error_code start_transfer() override;

    private:
        std::ifstream& file_to_send_;
};