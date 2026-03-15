#pragma once
#include "data_transfer.hpp"
#include <fstream>
#include <boost/asio.hpp>

class Receiver : public Data_transfer
{
    public:
        explicit Receiver(boost::asio::io_context& context, unsigned short port, std::ofstream& output_file);

        boost::system::error_code start() override;
    
    private:
        boost::system::error_code transfer_confirmation() override;

        boost::system::error_code start_transfer() override;
    
    private:
        std::ofstream& output_file_;
        
        uint64_t receive_file_size_;
};