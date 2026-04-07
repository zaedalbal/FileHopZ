#pragma once
#include "protohopz_packet.hpp"
#include <boost/asio.hpp>
#include <unordered_map>

class ProtoHopZ
{
    public:
        explicit ProtoHopZ();

        // packets_count - количество пакетов которые находится для отправки/чтения от начала указателя
        boost::asio::awaitable<boost::system::error_code>
        send(const PHZ::Packet* source, int packets_count);

        boost::asio::awaitable<boost::system::error_code>
        receive(PHZ::Packet* destination, int packets_count);

    private:
        boost::asio::awaitable<boost::system::error_code>
        resend_packet(uint32_t sequense);

        boost::asio::awaitable<boost::system::error_code>
        timeout_loop();

        void ack_handler(uint32_t sequence);

    private:
        boost::asio::ip::udp::socket socket_;
        boost::asio::ip::udp::endpoint peer_endpoint_;

        std::unordered_map<uint32_t, PHZ::PacketLocal> in_flight_;

        double bandwidth_bytes_per_second = 0.0;
        double cwnd_ = 1.0;
        double sshthresh_ = 64.0;
};