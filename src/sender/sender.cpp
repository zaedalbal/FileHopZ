#include <sender/sender.hpp>
#include <packet.hpp>
#include <iostream>

Sender::Sender(boost::asio::io_context& context, const std::string& ip, unsigned short port, std::filesystem::path& files_to_send)
: Data_transfer(context, ip, port), files_to_send_(files_to_send), file_walker_(files_to_send_)
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

    while(file_walker_.next())
    {
        bytes_to_transfer_ += std::filesystem::file_size(file_walker_.current_path());
    }
    file_walker_.reset();
    
    Packet packet;
    packet.set_payload(&bytes_to_transfer_, sizeof(bytes_to_transfer_));

    ec = send_packet(&packet, nullptr);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        return ec;
    }
    ec = receive_packet(&packet, nullptr);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        return ec;
    }
    if(packet.header.type == PacketType::CONFIRM)
        return start_transfer();
    else
    {
        std::cout << "Receiver refused files\n";
        return ec;
    }
}

boost::system::error_code Sender::start_transfer()
{
    socket_.non_blocking(true);
    std::cout << "sender ok\n";

    return {};
}

void Sender::ack_handler(uint32_t seq)
{
    std::unique_lock<std::mutex> lock(mtx_);
    auto it = in_flight_.find(seq);
    if(it == in_flight_.end())
        return;
    in_flight_.erase(it);
    increase_window();
}

void Sender::resend_packet(uint32_t seq)
{
    std::unique_lock<std::mutex> lock(mtx_);
    auto it = in_flight_.find(seq);
    if(it == in_flight_.end())
        return;
    it->second.send_time = std::chrono::steady_clock::now();
    send_packet(&it->second.packet, nullptr);
    decrease_window();
}

void Sender::increase_window()
{
    if(cwnd_ < sshthresh_)
        cwnd_ *= 2;
    else
        cwnd_ += 1;
}

void Sender::decrease_window()
{
    sshthresh_ = cwnd_ / 2;
    cwnd_ = 1;
    if(sshthresh_ < 1)
        sshthresh_ = 1;
}

void Sender::timeout_loop()
{
    while(true)
    {
        std::vector<uint32_t> packets_to_resend;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            auto now_time_point = std::chrono::steady_clock::now();
            for(auto& [seq, packet] : in_flight_)
            {
                if(now_time_point - packet.send_time > TIMEOUT)
                    packets_to_resend.push_back(seq);
            }
        }
        for(auto seq : packets_to_resend)
        {
            thread_pool_.submit([this, seq]()
            {
                resend_packet(seq);
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // в будущем поменять, пока что это просто чтоб не грузился проц
    }
}