#include <fstream>
#include <boost/asio.hpp>

class Receiver
{
    public:
        explicit Receiver(boost::asio::io_context& context, unsigned short port, std::ofstream& output_file);

        boost::system::error_code start();
    
    private:
        boost::system::error_code confirmation_request();

        boost::system::error_code start_receive_file();
    
    private:
        boost::asio::io_context& context_;

        unsigned short port_;

        std::ofstream& output_file_;
        
        uint64_t receive_file_size_;

        boost::asio::ip::udp::socket socket_;

        boost::asio::ip::udp::endpoint sender_endpoint_;
};