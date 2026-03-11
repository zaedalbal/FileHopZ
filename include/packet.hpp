#include <cstdint>

constexpr std::size_t PACKET_SIZE = 1024;
constexpr int TIMEOUT = 1024; // в миллисекундах

struct Packet
{
    uint32_t sequense;
    uint16_t size;
    char data[PACKET_SIZE];
};