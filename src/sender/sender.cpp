#include <sender/sender.hpp>
#include <packet.hpp>
#include <iostream>

Sender::Sender(boost::asio::io_context& context, const std::string& ip, unsigned short port, std::ifstream& file)
: Data_transfer(context, ip, port), file_to_send_(file)
{
    std::cout << "Sender constructor called\n";
    start();
}

boost::system::error_code Sender::start()
{
    return transfer_confirmation();
}

boost::system::error_code Sender::transfer_confirmation()
{
    boost::system::error_code ec;
    int64_t filesize;
    file_to_send_.seekg(0, std::ios::end);
    filesize = file_to_send_.tellg();
    file_to_send_.seekg(0, std::ios::beg);
    
    Packet packet;
    std::memcpy(packet.data, &filesize, sizeof(filesize));

    socket_.send_to(boost::asio::buffer(&packet, sizeof(packet)), peer_endpoint_, 0, ec);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        return ec;
    }
    socket_.receive_from(boost::asio::buffer(&packet, sizeof(packet)), peer_endpoint_, 0, ec);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        return ec;
    }
    if(packet.header.type == PacketType::CONFIRM)
        return start_transfer();
    else
        return ec;
}

boost::system::error_code Sender::start_transfer()
{
    std::error_code return_code;
    boost::system::error_code ec;
    boost::asio::steady_timer timer(context_);
    socket_.non_blocking(true);
    uint32_t seq = 0;
    while (true)
    {
        Packet packet;
        file_to_send_.read(packet.data, PACKET_SIZE);
        packet.header.sequence = seq;
        packet.header.size = file_to_send_.gcount();

        socket_.send_to(boost::asio::buffer(&packet, sizeof(PacketHeader) + packet.header.size), peer_endpoint_);

        timer.expires_after(std::chrono::milliseconds(TIMEOUT));
        bool ack_received = false;

        while(!ack_received)
        {
            uint32_t ack;
            auto len = socket_.receive_from(boost::asio::buffer(&ack, sizeof(ack)), peer_endpoint_, 0, ec);
            if(!ec && ack == seq)
                ack_received = true;
            else
            {
                if(timer.expiry() <= std::chrono::steady_clock::now())
                {
                    socket_.send_to(boost::asio::buffer(&packet, sizeof(PacketHeader) + packet.header.size), peer_endpoint_);
                    timer.expires_after(std::chrono::milliseconds(TIMEOUT));
                }
            }
        }
        ++seq;
        if(packet.header.size < PACKET_SIZE)
            break;
    }
    Packet end_packet;
    end_packet.header.sequence = seq;
    end_packet.header.size = 0;
    socket_.send_to(boost::asio::buffer(&end_packet, sizeof(PacketHeader)), peer_endpoint_);
    return ec;
}