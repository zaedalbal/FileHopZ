#include "utils/file_builder.hpp"

namespace
{
    inline std::filesystem::path extract_path(const Packet& packet)
    {
        return std::filesystem::path(std::string(packet.data, packet.header.size));
    }
}

File_builder::File_builder(const std::filesystem::path& root)
: root_(root)
{
    std::error_code ec;
    std::filesystem::create_directories(root_, ec);
    if(ec)
    {} // пока что нет обработки ошибки
}

boost::system::error_code File_builder::create_directory(const std::filesystem::path& relative_path)
{
    std::error_code ec;
    auto full_path = root_ / relative_path; // оператор '/' перегружен так что склеивает две строки
    std::filesystem::create_directories(full_path, ec);
    return ec;
}

boost::system::error_code File_builder::create_file(const std::filesystem::path& relative_path, std::unique_ptr<std::ofstream>& out_file)
{
    std::error_code ec;
    auto full_path = root_ / relative_path; // оператор '/' перегружен так что склеивает две строки
    std::filesystem::create_directories(full_path.parent_path(), ec);
    if(ec)
        return ec;
    out_file = std::make_unique<std::ofstream>(full_path, std::ios::binary | std::ios::trunc);
    if(!out_file->is_open())
    {
        out_file.reset(); // очистка указателя если че то пошло не так
        return boost::system::errc::make_error_code(boost::system::errc::io_error);
    }
    return {};
}


boost::system::error_code File_builder::write(const char* data, std::size_t size, uint32_t file_id)
{
    auto it = open_files_.find(file_id);
    if(it == open_files_.end())
        return boost::system::errc::make_error_code(boost::system::errc::bad_file_descriptor);
    it->second->write(data, static_cast<std::streamsize>(size)); // static_cast на будущее, если будет передаваться большие объемы данных
    if(it->second->bad() || it->second->fail()) // проверка ошибки записи
        return boost::system::errc::make_error_code(boost::system::errc::io_error);
    return {};
}


boost::system::error_code File_builder::close_file(uint32_t file_id)
{
    auto it = open_files_.find(file_id);
    if(it == open_files_.end())
        return boost::system::errc::make_error_code(boost::system::errc::bad_file_descriptor);
    it->second->close();
    open_files_.erase(it);
    return {};
}

boost::system::error_code File_builder::handle_packet(const Packet& packet)
{
    switch (packet.header.type)
    {
        case PacketType::KEEP_ALIVE:
        case PacketType::CONFIRM:
        case PacketType::CONFIRM_FAILED:

        case PacketType::CREATE_DIRECTORY:
        {
            return create_directory(extract_path(packet));
        }

        case PacketType::CREATE_FILE:
        {
            std::unique_ptr<std::ofstream> file;
            auto ec = create_file(extract_path(packet), file);
            if(ec)
                return ec;
            open_files_[packet.header.file_id] = std::move(file);
            return {};
        }
        case PacketType::FILE_DATA:
        {
            return write(packet.data, packet.header.size, packet.header.file_id);
        }
        case PacketType::END_FILE:
        case PacketType::END_TRANSFER:

        default:
        {
            return {};
        }
    }
}