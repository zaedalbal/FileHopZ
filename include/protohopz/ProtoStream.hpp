#pragma once
#include "protohopz.hpp"
#include <memory>

class ProtoStream
{
    public:
        struct Chunk
        {
            explicit Chunk(const void* data, std::size_t size)
            : size_(size), data_(std::make_unique<char[]>(size_))
            {
                std::memcpy(data_.get(), data, size_);
            }

            Chunk(const Chunk&) = delete;
            Chunk& operator=(const Chunk&) = delete;

            std::size_t size_;
            std::unique_ptr<char[]> data_;
        };

        explicit ProtoStream
        (boost::asio::ip::udp::socket socket, boost::asio::ip::udp::endpoint peer_endpoint);

        ProtoStream(const ProtoStream&) = delete;
        ProtoStream& operator=(const ProtoStream&) = delete;

        boost::asio::awaitable<boost::system::error_code>
        send(char* data, std::size_t size);

        Chunk receive();

        void close();

    private:
        ProtoHopZ transport_;

        std::deque<Chunk> ready_chunks_;

        bool loops_started = false;
};