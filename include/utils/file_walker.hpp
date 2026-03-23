#pragma once
#include <filesystem>

class File_walker
{
    public:
        explicit File_walker(const std::filesystem::path& root);

        bool next(); // переход к следующиему файлу/дериктории

        const std::filesystem::path& current_path(); // получить путь к текущему файлу/дериктории

        std::filesystem::path relative_path(); // получить путь к текущему файлу/дериктории относительно корневой дериктории (root_)

    private:
        std::filesystem::path root_;

        std::filesystem::recursive_directory_iterator it_;

        std::filesystem::recursive_directory_iterator end_;

        std::filesystem::path current_;
};