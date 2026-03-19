#include "utils/file_walker.hpp"

File_walker::File_walker(const std::filesystem::path& root)
: root_(root), it_(root_, std::filesystem::directory_options::skip_permission_denied), end_()
{}

bool File_walker::next()
{
    while(it_ != end_)
    {
        const auto& entry = *it_;
        ++it_;
        if(!entry.is_regular_file())
            continue;
        current_ = entry.path();
        return true;
    }
    return false;
}

const std::filesystem::path& File_walker::current_path()
{
    return current_;
}

std::filesystem::path File_walker::releative_path()
{
    return std::filesystem::relative(current_, root_);
}