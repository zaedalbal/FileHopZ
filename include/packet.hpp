#pragma once
#include <cstdint>

constexpr std::size_t PACKET_SIZE = 1024;
constexpr int TIMEOUT = 1024; // в миллисекундах

enum PacketType : uint8_t
{
    KEEP_ALIVE, // проверка соеденения

    CONFIRM, // подтверждение что receiver готов принять файл
    CONFIRM_FAILED, // оповещение отправителя, если пользователь отказался принимать файл

    CREATE_DIRECTORY, // при этом типе пакета название директории, котрый надо создать передается в data
    CREATE_FILE, // при этом типе пакета название файла, котрый надо создать передается в data
    FILE_DATA, // данный тип указывает что передается просто кусок данных
    END_FILE, // данный тип указывает что достигнут конец файла

    END_TRANSFER // данный тип указывает что передача всех файлов окончена
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
    uint32_t sequence;
};

struct Packet
{
    PacketHeader header;
    char data[PACKET_SIZE];

    boost::system::error_code set_payload(const void* src, std::size_t size)
    {
        if(size > PACKET_SIZE)
            return boost::system::errc::make_error_code(boost::system::errc::value_too_large);
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