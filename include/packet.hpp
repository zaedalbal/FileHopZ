#include <cstdint>

constexpr std::size_t PACKET_SIZE = 1024;
constexpr int TIMEOUT = 1024; // в миллисекундах

enum PacketType : uint8_t
{
    KEEP_ALIVE, // проверка соеденения

    CONFIRM, // подтверждение что receiver готов принять файл
    CONFIRM_FAILED // оповещение отправителя, если пользователь отказался принимать файл
};

enum PacketFlags : uint8_t
{

};

#pragma pack(push, 1) // выравнивание по 1 байту

struct Packet
{
    PacketType type;
    PacketFlags flags;
    uint32_t sequense;
    uint16_t size;
    char data[PACKET_SIZE];
};

#pragma pack(pop) // восстановление выравнивания

constexpr std::size_t PacketHeader = sizeof(uint8_t)
+ sizeof(uint8_t)
+ sizeof(uint32_t)
+ sizeof(uint16_t);