#pragma once
#include <cstdint>
#include <chrono>

namespace PHZ
{
    constexpr std::size_t PACKET_SIZE = 1024;
    constexpr std::chrono::milliseconds TIMEOUT(128); // в миллисекундах

    enum PacketType : uint8_t
    {
        KEEP_ALIVE, // проверка соеденения

        DATA, // пакет с данными

        ACK, // подтверждение пакета

        END_TRANSFER // данный тип указывает передача окончена
    };

    enum PacketFlags : uint8_t
    {/*пока что нет никаких флагов*/};


#pragma pack(push, 1) // выравнивание по 1 байту

    struct PacketHeader
    {
        PacketType type;
        PacketFlags flags;
        uint16_t size;
        uint32_t sequence;
    };

    struct Packet
    {
        PacketHeader header;
        char data[PACKET_SIZE];
    };

#pragma pack(pop) // восстановление выравнивания

    struct PacketLocal // структура для реализации переотправки пакетов
    {
        std::chrono::steady_clock::time_point send_time;
        Packet packet;
    };
}