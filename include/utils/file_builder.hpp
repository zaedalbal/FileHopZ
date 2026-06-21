#pragma once
#include <filesystem>
#include <fstream>
#include <memory>
#include <boost/system.hpp>
#include <unordered_map>
#include "packet.hpp"

class File_builder
{
    public:
        // конструктор: создаёт root_, если её нет
        explicit File_builder(const std::filesystem::path& root);

        // метод для создания директории
        boost::system::error_code create_directory(const std::filesystem::path& relative_path);

        // метод для создания файла
        boost::system::error_code create_file(const std::filesystem::path& relative_path, uint32_t file_id);

        // file_id — ключ для одновременной записи в несколько файлов
        boost::system::error_code write(const char* data, std::size_t size, uint32_t file_id);

        // метод для закрытия файла
        boost::system::error_code close_file(uint32_t file_id);

    private:
        // корневая директория
        std::filesystem::path root_;

        // открытые файлы
        std::unordered_map<uint32_t, std::unique_ptr<std::ofstream>> open_files_;
};