#pragma once
#include <filesystem>

class File_walker
{
    public:
        explicit File_walker(const std::filesystem::path& root);

        bool next(); // переход к следующему файлу/директории

        const std::filesystem::path& current_path(); // текущий путь (файл/директория)

        std::filesystem::path relative_path(); // путь относительно root_

        void reset(); // сброс итератора

    private:
        std::filesystem::path root_;

        bool single_file_ = false;

        bool single_file_returned_ = false;

        std::filesystem::recursive_directory_iterator it_;

        std::filesystem::recursive_directory_iterator end_;

        std::filesystem::path current_;
};