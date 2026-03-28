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
        // конструктор принимает имя корневой дериктории и создает её, если её нет
        explicit File_builder(const std::filesystem::path& root);

        // метод для обработки пакета
        boost::system::error_code handle_packet(const Packet& packet);

    private:
        // метод для создания директории
        boost::system::error_code create_directory(const std::filesystem::path& relative_path);

        // метод для создания файла
        boost::system::error_code create_file(const std::filesystem::path& relative_path, std::unique_ptr<std::ofstream>& out_file);

        // данный метод принимает file_id, чтобы в будущем можно было держать много файлов открытыми
        boost::system::error_code write(const char* data, std::size_t size, uint32_t file_id);

        // метод для закрытия файла
        boost::system::error_code close_file(uint32_t file_id);

    private:
        // корневая директория
        std::filesystem::path root_;

        // открытые файлы
        std::unordered_map<uint32_t, std::unique_ptr<std::ofstream>> open_files_;
};