#pragma once
#include <filesystem>
#include <fstream>
#include <memory>
#include <boost/system.hpp>
#include <unordered_map>
#include "ftp_packet.hpp"

class File_builder
{
    public:
        // конструктор: создаёт root_, если её нет
        explicit File_builder(const std::filesystem::path& root);

        // метод для создания директории во временном дереве
        boost::system::error_code create_directory(const std::filesystem::path& relative_path);

        // метод для создания файла во временном дереве
        boost::system::error_code create_file(const std::filesystem::path& relative_path, uint32_t file_id);

        // метод для создания символической ссылки во временном дереве
        boost::system::error_code create_symlink(
            const std::filesystem::path& relative_path,
            const std::filesystem::path& target
        );

        // file_id — ключ для одновременной записи в несколько файлов
        boost::system::error_code write(const char* data, std::size_t size, uint32_t file_id);

        // метод для закрытия файла
        boost::system::error_code close_file(uint32_t file_id);

        // переносит временное дерево в финальную директорию
        boost::system::error_code commit();

        // удаляет временное дерево после ошибки передачи
        void cleanup();

    private:
        struct Open_file
        {
            std::unique_ptr<std::ofstream> stream;
        };

        boost::system::error_code close_all();

        boost::system::error_code validate_relative_path(const std::filesystem::path& relative_path) const;

        std::filesystem::path staged_path(const std::filesystem::path& relative_path) const;

        std::filesystem::path final_path(const std::filesystem::path& relative_path) const;

    private:
        // финальная корневая директория
        std::filesystem::path root_;

        // временная директория текущей передачи
        std::filesystem::path staging_root_;

        // открытые файлы
        std::unordered_map<uint32_t, Open_file> open_files_;
};
