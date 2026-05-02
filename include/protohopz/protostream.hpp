#pragma once
#include "protohopz.hpp"
#include <memory>
#include <expected>

class ProtoStream
{
    public:
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

        explicit ProtoStream(boost::asio::ip::udp::socket socket);

        explicit ProtoStream(
            boost::asio::ip::udp::socket socket,
            boost::asio::ip::udp::endpoint peer_endpoint
        );

        ProtoStream(const ProtoStream&) = delete;
        ProtoStream& operator=(const ProtoStream&) = delete;

        boost::asio::awaitable<boost::system::error_code>
        send(std::span<const std::byte> data);

        boost::asio::awaitable<Chunk> receive();

        boost::asio::awaitable<void> close();

    private:
        void start_loops();

        boost::asio::awaitable<void> receive_chunks_loop();

    private:
        boost::asio::any_io_executor executor_;

        ProtoHopZ transport_;

        Async_queue<Chunk> ready_chunks_;

        bool transport_loops_running_ = false;
        bool receive_chunks_loop_running_ = false;
};