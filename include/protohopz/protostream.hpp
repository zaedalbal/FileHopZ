#pragma once
#include "protohopz.hpp"
#include <memory>
#include <expected>

class ProtoStream
{
    public:
        enum HANDSHAKE_MODE : bool
        {
            INITIATOR,
            RESPONDER
        };

        struct Chunk
        {
            explicit Chunk()
            : size_(0), data_(nullptr)
            {}

            explicit Chunk(const void* data, std::size_t size)
            : size_(size), data_(std::make_unique<char[]>(size_))
            {
                if(size > 0 && data)
                    std::memcpy(data_.get(), data, size_);
            }

            Chunk(Chunk&&) noexcept = default;

            Chunk(const Chunk&) = delete;
            Chunk& operator=(const Chunk&) = delete;

            bool empty()
            {
                return (size_ == 0 && data_ == nullptr);
            }

            std::size_t size_;
            std::unique_ptr<char[]> data_;
        };

        explicit ProtoStream(
            boost::asio::ip::udp::socket socket,
            HANDSHAKE_MODE handshake_mode
        );

        explicit ProtoStream(
            boost::asio::ip::udp::socket socket,
            boost::asio::ip::udp::endpoint peer_endpoint,
            HANDSHAKE_MODE handshake_mode
        );

        ProtoStream(const ProtoStream&) = delete;
        ProtoStream& operator=(const ProtoStream&) = delete;

        boost::asio::awaitable<boost::system::error_code>
        send(std::span<const std::byte> data);

        boost::asio::awaitable<std::expected<Chunk, boost::system::error_code>>
        receive();

        boost::asio::awaitable<boost::system::error_code> close();

    private:
        boost::asio::awaitable<boost::system::error_code>
        start_loops();

        boost::asio::awaitable<boost::system::error_code> receive_chunks_loop();

        void handle_receive_chunks_result(boost::system::error_code ec);

        void store_stream_error(boost::system::error_code ec);

    private:
        HANDSHAKE_MODE handshake_mode_;
        
        boost::asio::any_io_executor executor_;

        ProtoHopZ transport_;

        Async_queue<Chunk> ready_chunks_;

        bool loops_running_ = false;
        bool stopping_loops_ = false;
        boost::system::error_code stream_error_;
        boost::asio::cancellation_signal cancellation_signal_receive_chunks_loop_;
};
