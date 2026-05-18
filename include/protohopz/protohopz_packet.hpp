#pragma once
#include <cstdint>
#include <chrono>
#include "utils/crypto_context.hpp"

namespace PHZ
{
    constexpr std::size_t CRYPT_OVERHEAD = NONCE_LEN + TAG_LEN; // nonce + tag
    constexpr std::size_t PACKET_PAYLOAD_SIZE = 1400; // plaintext до шифрования
    constexpr std::size_t PACKET_SIZE = PACKET_PAYLOAD_SIZE + CRYPT_OVERHEAD; // 1428 < MTU
    constexpr std::chrono::milliseconds TIMEOUT(128); // в миллисекундах

    enum PacketType : uint8_t
    {
        KEEP_ALIVE, // проверка соеденения

        DATA, // пакет с данными

        HANDSHAKE,

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
        char payload[PACKET_PAYLOAD_SIZE];
        char crypt_info[CRYPT_OVERHEAD];
    };

    // payload и crypt_info подряд — единая область (до PACKET_SIZE байт)
    inline char* payload_region(Packet& packet) noexcept
    {
        return packet.payload;
    }

    inline const char* payload_region(const Packet& packet) noexcept
    {
        return packet.payload;
    }

#pragma pack(pop) // восстановление выравнивания

    static_assert(sizeof(Packet) == sizeof(PacketHeader) + PACKET_SIZE);

    struct PacketLocal // структура для реализации переотправки пакетов
    {
        std::chrono::steady_clock::time_point send_time;
        Packet packet;
    };
}