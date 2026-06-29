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
    constexpr std::chrono::seconds IDLE_TIMEOUT(60); // сколько ждать пакетов от peer

    // начальное окно (slow start); 64 пакета = ~90 КБ — не даётся стартовать
    // с cwnd=1 (это сильно режет RTT на локалке), но и не раздуваем in_flight_
    constexpr double INITIAL_CWND = 64.0;

    // верхний предел окна: 1024 пакета ≈ 1.4 МБ в полёте; достаточно для
    // гигабитного LAN, при этом RAM под in_flight_ ограничена ~1.5 МБ
    constexpr double MAX_CWND = 1024.0;

    // абсолютный предел на размер in_flight_, не зависящий от cwnd;
    // защита от случая, когда cwnd случайно разрастается выше MAX_CWND
    constexpr std::size_t MAX_IN_FLIGHT = 1024;

    // hard cap на буфер переупорядочивания на стороне получателя.
    // Должен быть не меньше окна ProtoStream, иначе Release-сборка успевает
    // заполнить буфер допустимыми out-of-order пакетами до retransmit.
    constexpr std::size_t MAX_PACKETS_BUFFER = 65536;

    // сколько ждать опустошения in_flight_ в close() перед принудительной остановкой
    constexpr std::chrono::seconds CLOSE_DRAIN_TIMEOUT(30);

    enum PacketType : uint8_t
    {
        KEEP_ALIVE, // проверка соединения

        DATA, // пакет с данными

        HANDSHAKE,

        ACK, // подтверждение пакета

        END_TRANSFER // по сети не отправляется; локальный маркер в stop_loops (разбудить pop()).
                     // Реальный сигнал конца передачи — прикладной PacketType::END_TRANSFER (ftp_packet.hpp), идёт как DATA
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
