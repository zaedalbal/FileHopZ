#include <iostream>
#include <boost/asio.hpp>
#include <fstream>
#include <charconv>
#include <sender/sender.hpp>
#include <receiver/receiver.hpp>

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        std::cout << "Usage:\n";
        std::cout << "Sender:   ./filehopz send <address> <port> <file>\n";
        std::cout << "Receiver: ./filehopz recv <port> <output filename>\n";
        return 1;
    }

    std::string mode = argv[1];

    if(mode == "send" && argc < 5)
    {
        std::cerr << "Sender usage: ./filehopz send <address> <port> <file>\n";
        return 1;
    }

    if(mode == "recv" && argc < 4)
    {
        std::cerr << "Receiver usage: ./filehopz recv <port> <output filename>\n";
        return 1;
    }

    boost::asio::io_context io_context;

    if(mode == "send")
    {
        std::string address = argv[2];
        unsigned short port;
        auto [ptr, ec] = std::from_chars(argv[3], argv[3] + strlen(argv[3]), port);
        if(ec != std::errc())
        {
            std::cerr << "Invalid port\n";
            return 1;
        }
        std::ifstream input(argv[4], std::ios::binary);
        if(!input)
        {
            std::cerr << "Cannot open file\n";
            return 1;
        }
        Sender(io_context, address, port, input);
    }

    else if(mode == "recv")
    {
        std::ofstream output_file(argv[3], std::ios::binary);
        unsigned short port;
        auto [ptr, ec] = std::from_chars(argv[2], argv[2] + strlen(argv[2]), port);
        if(ec != std::errc())
        {
            std::cerr << "Invalid port\n";
            return 1;
        }
        if(!output_file)
        {
            std::cerr << "Cannot open file\n";
            return 1;
        }
        Receiver(io_context, port, output_file);
    }

    else
    {
        std::cerr << "Unknown mode \"" << mode << "\"\n";
        return 1;
    }

    return 0;
}