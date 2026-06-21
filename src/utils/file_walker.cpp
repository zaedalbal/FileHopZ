#include "utils/file_walker.hpp"
#include <iostream>

// TODO: переписать на режимы (одиночный файл / директория) вместо флагов
// single_file_ и single_file_returned_ — сейчас из-за них код запутанный

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
        // entry — копия: ссылка инвалидируется после ++it_
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