#pragma once
#include "data_transfer.hpp"
#include "utils/file_walker.hpp"
#include <fstream>
#include <boost/asio.hpp>
#include <unordered_map>
#include <chrono>

class Sender : public Data_transfer
{
    public:
        // constructor
        explicit Sender(boost::asio::io_context& context, const std::string& ip, unsigned short port, std::filesystem::path& files_to_send);

        boost::system::error_code start() override;
    
    private:
        struct PacketLocal
        {
            std::chrono::steady_clock::time_point send_time;
            Packet packet; 
        };

        boost::system::error_code transfer_confirmation() override;

        boost::system::error_code start_transfer() override;

        void ack_handler(uint32_t seq);
        void resend_packet(uint32_t seq);
        void increase_window();
        void decrease_window();
        void timeout_loop();

    private:
        std::filesystem::path& files_to_send_;
        File_walker file_walker_;

        std::unordered_map<uint32_t, PacketLocal> in_flight_;
        std::mutex mtx_;
        uint64_t cwnd_ = 1; // окно перегрузки
        uint64_t sshthresh_ = 16; // граница медленного старта (если меньше cwnd меньше чем sshthresh, то
                                        // cwnd увеличивается в 2 раза при increase_window, если меньше, то
                                        // cwnd инкрементируется при increase_window)
};