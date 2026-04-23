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

boost::system::error_code File_builder::create_file(const std::filesystem::path& relative_path, uint32_t file_id)
{
    std::error_code ec;
    auto full_path = root_ / relative_path; // оператор '/' перегружен так что склеивает две строки
    std::filesystem::create_directories(full_path.parent_path(), ec);
    if(ec)
        return ec;

    auto file = std::make_unique<std::ofstream>(full_path, std::ios::binary | std::ios::trunc);

    if(!file->is_open())
    {
        file.reset(); // очистка указателя если че то пошло не так
        return boost::system::errc::make_error_code(boost::system::errc::io_error);
    }

    open_files_[file_id] = std::move(file);

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