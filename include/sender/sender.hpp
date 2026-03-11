#include <fstream>
#include <boost/asio.hpp>

class Sender
{
    public:
        // constructor
        explicit Sender(boost::asio::io_context& context, const std::string& ip, unsigned short port, std::ifstream& file);

        boost::system::error_code start();
    
    private:
        boost::system::error_code send_file();

    private:
        boost::asio::io_context& context_;

        std::ifstream& file_to_send_;

        const std::string& receiver_address_;

        unsigned short receiver_port_;

        boost::asio::ip::udp::socket socket_;
};