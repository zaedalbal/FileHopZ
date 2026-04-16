#include "protohopz/protostream.hpp"

ProtoStream::ProtoStream(boost::asio::ip::udp::socket socket, boost::asio::ip::udp::endpoint peer_endpoint)
: transport_(std::move(socket), peer_endpoint)
{}

