#pragma once
#include <cstdint>
#include <span>
#include <cstring>
#include "errors/filehopz_error.hpp"
#include "protohopz/protohopz_packet.hpp"

// FTProto - File Transfer Protocol
namespace FTProto
{
    enum PacketType : uint8_t
    {
        CONFIRM = 0, // подтверждение что receiver готов принять файл
        CONFIRM_FAILED = 1, // оповещение отправителя, если пользователь отказался принимать файл

        CREATE_DIRECTORY = 2, // название директории для создания передаётся в data
        CREATE_FILE = 3, // название файла для создания передаётся в data
        FILE_DATA = 4, // данный тип указывает что передается просто кусок данных
        END_FILE = 5, // данный тип указывает что достигнут конец файла

        END_TRANSFER = 6, // данный тип указывает что передача всех файлов окончена
        CREATE_SYMLINK = 7 // путь ссылки и её target передаются в data
    };

    enum PacketFlags : uint8_t
    {

    };

    #pragma pack(push, 1) // выравнивание по 1 байту

    struct PacketHeader
    {
        PacketType type;
        PacketFlags flags;
        uint16_t size;
        uint32_t file_id;
    };

    constexpr std::size_t PACKET_SIZE = PHZ::PACKET_PAYLOAD_SIZE - sizeof(PacketHeader);

    struct Packet
    {
        PacketHeader header;
        char data[PACKET_SIZE];

        static constexpr std::size_t serialized_size(const Packet& packet)
        {
            return sizeof(PacketHeader) + packet.header.size;
        }

        static std::span<const std::byte> as_bytes(const Packet& packet)
        {
            return std::as_bytes(std::span(
                reinterpret_cast<const std::byte*>(&packet),
                serialized_size(packet)
            ));
        }

        boost::system::error_code set_payload(const void* src, std::size_t size)
        {
            if(size > PACKET_SIZE)
                return filehopz::Error_code::packet_too_large;
            std::memcpy(data, src, size);
            header.size = static_cast<uint16_t>(size);
            return {};
        }

        const char* get_payload() const
        {return data;}

        std::size_t get_payload_size() const
        {return header.size;}
    };

    #pragma pack(pop) // восстановление выравнивания
}
