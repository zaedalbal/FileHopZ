#include "utils/file_builder.hpp"

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