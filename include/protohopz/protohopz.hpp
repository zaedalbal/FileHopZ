#pragma once
#include "protohopz_packet.hpp"
#include <boost/asio.hpp>
#include <unordered_map>
#include <unordered_set>
#include <queue>

class ProtoHopZ
{
    public:
        explicit ProtoHopZ(
            boost::asio::ip::udp::socket socket,
            boost::asio::ip::udp::endpoint peer_endpoint
        );

        void start(); // метод для запуска receive_loop и timeout_loop

        void stop(); // метод для остановки receive_loop и timeout_loop

        boost::asio::awaitable<boost::system::error_code>
        send_packet(const PHZ::Packet* source);

        boost::asio::awaitable<void>
        receive_packet(PHZ::Packet* destination);

    private:
        boost::asio::awaitable<boost::system::error_code>
        resend_packet(uint32_t sequense);

        boost::asio::awaitable<boost::system::error_code>
        receive_loop();

        boost::asio::awaitable<boost::system::error_code>
        timeout_loop();

        boost::asio::awaitable<boost::system::error_code>
        send_ack(uint32_t sequence);

        void ack_handler(uint32_t sequence);

    private:
        boost::asio::ip::udp::socket socket_;
        boost::asio::ip::udp::endpoint peer_endpoint_;

        std::atomic<bool> running_;

        uint32_t sequence_counter_ = 0;
        std::queue<PHZ::Packet> received_packets_queue_;
        std::unordered_set<uint32_t> received_packets_;
        std::unordered_map<uint32_t, PHZ::PacketLocal> in_flight_;

        double bandwidth_bytes_per_second = 0.0;
        double cwnd_ = 1.0;
        double sshthresh_ = 64.0;
};