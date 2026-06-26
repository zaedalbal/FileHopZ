#include "utils/file_builder.hpp"

namespace
{
    inline std::filesystem::path extract_path(const FTProto::Packet& packet)
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
    auto full_path = root_ / relative_path; // path::operator/ — склейка путей (std::filesystem)
    std::filesystem::create_directories(full_path, ec);
    return ec;
}

boost::system::error_code File_builder::create_file(const std::filesystem::path& relative_path, uint32_t file_id)
{
    std::error_code ec;
    auto full_path = root_ / relative_path; // path::operator/ — склейка путей (std::filesystem)
    std::filesystem::create_directories(full_path.parent_path(), ec);
    if(ec)
        return ec;

    auto file = std::make_unique<std::ofstream>(full_path, std::ios::binary | std::ios::trunc);

    if(!file->is_open())
    {
        file.reset(); // освободить ресурс при ошибке открытия
        return boost::system::errc::make_error_code(boost::system::errc::io_error);
    }

    open_files_[file_id] = std::move(file);

    return {};
}

boost::system::error_code File_builder::create_symlink(
    const std::filesystem::path& relative_path,
    const std::filesystem::path& target
)
{
    std::error_code ec;
    auto full_path = root_ / relative_path; // path::operator/ — склейка путей (std::filesystem)
    std::filesystem::create_directories(full_path.parent_path(), ec);
    if(ec)
        return ec;

    // symlink_status возвращает not_found для нового пути, это не ошибка создания
    auto status = std::filesystem::symlink_status(full_path, ec);
    if(ec && status.type() != std::filesystem::file_type::not_found)
        return ec;
    ec.clear();

    // Перезаписываем только старую symlink, обычные файлы и директории не трогаем
    if(std::filesystem::is_symlink(status))
    {
        std::filesystem::remove(full_path, ec);
        if(ec)
            return ec;
    }
    else if(std::filesystem::exists(status))
        return boost::system::errc::make_error_code(boost::system::errc::file_exists);

    std::filesystem::create_symlink(target, full_path, ec);
    return ec;
}

boost::system::error_code File_builder::write(const char* data, std::size_t size, uint32_t file_id)
{
    auto it = open_files_.find(file_id);
    if(it == open_files_.end())
        return boost::system::errc::make_error_code(boost::system::errc::bad_file_descriptor);

    // static_cast: ofstream::write принимает std::streamsize, а size — std::size_t
    it->second->write(data, static_cast<std::streamsize>(size));

    if(it->second->bad() || it->second->fail())
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
