#pragma once
#include "protohopz_packet.hpp"
#include "utils/async_queue.hpp"
#include "utils/async_value.hpp"
#include "utils/crypto_context.hpp"
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

        void start_loops(); // метод для запуска receive_loop и timeout_loop

        void stop_loops(); // метод для остановки receive_loop и timeout_loop

        boost::asio::awaitable<boost::system::error_code>
        handshake();

        boost::asio::awaitable<boost::system::error_code>
        send_packet(const PHZ::Packet* source);

        boost::asio::awaitable<boost::system::error_code>
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
        bool loops_started = false;
        bool connection_encrypted_ = false;

        uint32_t sequence_counter_ = 0;

        double bandwidth_bytes_per_second = 0.0;
        double sshthresh_ = 64.0;
        Async_value<double> cwnd_; //= 1.0;

        boost::asio::ip::udp::socket socket_;
        boost::asio::ip::udp::endpoint peer_endpoint_;

        boost::asio::cancellation_signal cancellation_signal_receive_loop;
        boost::asio::cancellation_signal cancellation_signal_timeout_loop;

        Async_queue<PHZ::Packet> received_packets_queue_;

        std::unordered_set<uint32_t> received_packets_;
        std::unordered_map<uint32_t, PHZ::PacketLocal> in_flight_;

        Crypto_context crypto_context_;
};