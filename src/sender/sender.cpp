#include <sender/sender.hpp>
#include <packet.hpp>
#include <iostream>

Sender::Sender(boost::asio::io_context& context, const std::string& ip, unsigned short port, std::filesystem::path& files_to_send)
: Data_transfer(context, ip, port), files_to_send_(files_to_send), file_walker_(files_to_send_)
{
    std::cout << "Sender constructor called\n";
}

boost::asio::awaitable<boost::system::error_code>
Sender::start()
{
    co_return co_await transfer_confirmation();
}

boost::asio::awaitable<boost::system::error_code>
Sender::transfer_confirmation()
{
    boost::system::error_code ec;

    while(file_walker_.next())
    {
        bytes_to_transfer_ += std::filesystem::file_size(file_walker_.current_path());
    }
    file_walker_.reset();
    
    Packet packet;
    packet.set_payload(&bytes_to_transfer_, sizeof(bytes_to_transfer_));

    ec = co_await send_packet(&packet, nullptr);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        co_return ec;
    }
    ec = co_await receive_packet(&packet, nullptr);
    if(ec)
    {
        std::cerr << ec.message() << "\n";
        co_return ec;
    }
    if(packet.header.type == PacketType::CONFIRM)
        co_return co_await start_transfer();
    else
    {
        std::cout << "Receiver refused files\n";
        co_return ec;
    }
}

boost::asio::awaitable<boost::system::error_code>
Sender::start_transfer()
{
    std::cout << "sender ok\n";

    co_return boost::system::error_code();
}

void Sender::ack_handler(uint32_t seq)
{
    auto it = in_flight_.find(seq);
    if(it == in_flight_.end())
        return;
    in_flight_.erase(it);
    increase_window();
}

boost::asio::awaitable<boost::system::error_code>
Sender::resend_packet(uint32_t seq)
{
    boost::system::error_code ec;
    auto it = in_flight_.find(seq);
    if(it == in_flight_.end())
        co_return ec;
    decrease_window();
    it->second.send_time = std::chrono::steady_clock::now();
    co_return co_await send_packet(&it->second.packet, nullptr);
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

boost::asio::awaitable<boost::system::error_code>
Sender::timeout_loop()
{
    boost::system::error_code ec;
    boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);

    while(true)
    {
        timer.expires_after(std::chrono::milliseconds(5));
        co_await timer.async_wait(boost::asio::use_awaitable);

        auto now = std::chrono::steady_clock::now();

        for(auto& [seq, packet] : in_flight_)
        {
            if(now - packet.send_time > TIMEOUT)
            {
                ec = co_await resend_packet(seq);
                if(ec)
                    co_return ec;
            }
        }
    }
}