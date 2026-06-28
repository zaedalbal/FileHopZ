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

        // останавливает receive_loop/timeout_loop, чистит in_flight_/received_packets_;
        // корутина — выполняет очистку на strand_ и будит received_packets_queue_.pop()
        // фиктивным END_TRANSFER
        boost::asio::awaitable<void> stop_loops();

        // дожидается, пока in_flight_ не опустеет, или истечёт таймаут;
        // возвращает true, если дренаж успешен; false — если пришлось прервать
        // по таймауту или ошибке
        boost::asio::awaitable<bool> wait_in_flight_drained(
            std::chrono::steady_clock::duration timeout
        );

        boost::asio::awaitable<boost::system::error_code>
        handshake_initiator();

        boost::asio::awaitable<boost::system::error_code>
        handshake_responder();

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

        void handle_loop_result(boost::system::error_code ec);

        void store_loop_error(boost::system::error_code ec);

        void send_ack(uint32_t sequence);

        void ack_handler(uint32_t sequence);

    private:
        bool loops_started_ = false;
        bool stopping_loops_ = false;
        bool connection_encrypted_ = false;
        boost::system::error_code loop_error_;

        uint32_t sequence_counter_ = 0;

        double bandwidth_bytes_per_second = 0.0;
        double sshthresh_ = 64.0;

        boost::asio::ip::udp::socket socket_;
        boost::asio::ip::udp::endpoint peer_endpoint_;
        // Для receiver endpoint становится известен после первого пакета.
        bool peer_endpoint_known_ = false;

        // единый strand для всего общего стейта ProtoHopZ (in_flight_,
        // received_packets_, cwnd_, sshthresh_, sequence_counter_, drained_signal_);
        // всё, что трогает этот стейт, выполняется на strand_:
        //   - receive_loop/timeout_loop спавнятся на strand_;
        //   - send_packet/stop_loops/wait_in_flight_drained входят на strand_ через
        //     co_await post(strand_, use_awaitable);
        // async I/O suspend'ит корутину и отпускает strand, поэтому сеть не
        // сериализуется — strand держится только на синхронных обращениях к стейту
        boost::asio::strand<boost::asio::any_io_executor> strand_;

        Async_value<double> cwnd_;

        boost::asio::cancellation_signal cancellation_signal_receive_loop;
        boost::asio::cancellation_signal cancellation_signal_timeout_loop;

        Async_queue<PHZ::Packet> received_packets_queue_;

        std::unordered_set<uint32_t> received_packets_;
        std::unordered_map<uint32_t, PHZ::PacketLocal> in_flight_;

        // сигнал "in_flight_ опустел" для event-driven дренажа в wait_in_flight_drained;
        // set(true) в ack_handler при in_flight_.empty(); wait() в wait_in_flight_drained
        Async_value<bool> drained_signal_;

        Crypto_context crypto_context_;
};
