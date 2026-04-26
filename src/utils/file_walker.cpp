#include "utils/file_walker.hpp"
#include <iostream>

// В БУДУЩЕМ ПЕРЕПИСАТЬ РЕАЛИЗАЦИЮ КЛАССА НА РЕЖИМЫ РАБОТЫ С ОДИНОЧНЫМ ФАЙЛОМ И ДЕРИКТОРИЯМИ ВМЕСТО ФЛАГОВ

// НА ДАННЫЙ МОМЕНТ КОД ОЧЕНЬ КОСТЫЛЬНЫЙ И НЕПОНЯТНЫЙ ИЗ ЗА ФЛАГОВ single_file и single_file_returned

File_walker::File_walker(const std::filesystem::path& root)
: root_(root), end_()
{
    if(std::filesystem::is_regular_file(root_))
    {
        single_file_ = true;
        current_ = root;
        it_ = std::filesystem::recursive_directory_iterator();
    }
    else
        it_ = std::filesystem::recursive_directory_iterator(root_, std::filesystem::directory_options::skip_permission_denied);
}

bool File_walker::next()
{
    if(single_file_)
    {
        if(!single_file_returned_)
        {
            single_file_returned_ = true;
            return true;
        }
        return false;
    }
    while(it_ != end_)
    {
        // !!!entry КОПИЯ, А НЕ ССЫЛКА. ЕСЛИ СДЕЛАТЬ ССЫЛКУ, А ПОТОМ ++it_ ВСЕ ЛОМАЕТСЯ!!!
        // В БУДУЩЕМ ИСПРАВИТЬ
        const auto entry = *it_;
        ++it_;
        std::error_code ec;
        if(!entry.is_regular_file(ec) && !entry.is_directory(ec))
        {
            if(ec)
            {
                std::cerr << "(file_walker):" << ec.message() << "\n"
                << "path: " << entry.path().string();
                continue;
            }
            else 
            {
                std::cerr << "(file_walker): unsupported file type: "
                << entry.path().string() << "\n";
                continue;
            }
        }
        current_ = entry.path();
        return true;
    }
    return false;
}

const std::filesystem::path& File_walker::current_path()
{
    return current_;
}

std::filesystem::path File_walker::relative_path()
{
    if(single_file_)
        return current_.filename();

    return std::filesystem::relative(current_, root_);
}

void File_walker::reset()
{
    if(single_file_)
    {
        single_file_returned_ = false;
        current_ = root_;
    }
    else
    {
        it_ = std::filesystem::recursive_directory_iterator(root_, std::filesystem::directory_options::skip_permission_denied);
    }
}