#include "protohopz/protostream.hpp"
#include <iostream>
#include <map>

ProtoStream::ProtoStream(boost::asio::ip::udp::socket socket, boost::asio::ip::udp::endpoint peer_endpoint)
: executor_(socket.get_executor()), transport_(std::move(socket), peer_endpoint)
{}

boost::asio::awaitable<boost::system::error_code>
ProtoStream::send(char* data, std::size_t size)
{
    if(!transport_loops_running_)
        start_loops();
    if(size > PHZ::PACKET_SIZE) // в будущем сделать разбиение передаваемых данных на несколько пакетов
        co_return boost::system::errc::make_error_code(boost::system::errc::message_size);
    
    PHZ::Packet packet;
    packet.header.size = size;
    packet.header.type = PHZ::PacketType::DATA;
    std::memcpy(packet.data, data, packet.header.size);

    co_return co_await transport_.send_packet(&packet);
}

boost::asio::awaitable<ProtoStream::Chunk>
ProtoStream::receive()
{
    if(!transport_loops_running_)
        start_loops();

    auto executor = co_await boost::asio::this_coro::executor;

    while(ready_chunks_.empty())
    {
        // в будущем поменять, тк эта штука очень сильно ест CPU!!!
        co_await boost::asio::post(executor, boost::asio::use_awaitable);
    }
    ProtoStream::Chunk chunk = std::move(ready_chunks_.front());
    ready_chunks_.pop_front();
    co_return chunk;
}

void ProtoStream::close()
{
    transport_.stop();
    receive_chunks_loop_running_ = false;
}

void ProtoStream::start_loops()
{
        transport_loops_running_ = true;
        transport_.start();

        receive_chunks_loop_running_ = true;
        boost::asio::co_spawn(executor_, receive_chunks_loop(), boost::asio::detached);
}

boost::asio::awaitable<void> ProtoStream::receive_chunks_loop()
{
    // в будущем добавить контроль размера буфера с пакетами

    boost::system::error_code ec;
    std::map<decltype(PHZ::PacketHeader::sequence), PHZ::Packet> packets_buffer;
    decltype(PHZ::PacketHeader::sequence) expected_sequence = 0;
    while(receive_chunks_loop_running_)
    {
        PHZ::Packet packet;
        co_await transport_.receive_packet(&packet);

        if(packet.header.type != PHZ::PacketType::DATA)
        {
            // в будущем сделать обработчик пакетов
            std::cout << "skip packet in receive_chunks_loop\n";
            continue;
        }

        if(packets_buffer.contains(packet.header.sequence))
            continue; // скип дубликатов
        
        packets_buffer.emplace(packet.header.sequence, std::move(packet));

        while(receive_chunks_loop_running_)
        {
            auto it = packets_buffer.find(expected_sequence);
            if(it == packets_buffer.end())
                break;
            
            ready_chunks_.push_back(ProtoStream::Chunk(it->second.data, it->second.header.size));
            packets_buffer.erase(it);
            ++expected_sequence;
        }
    }
}