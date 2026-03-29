#include "utils/file_walker.hpp"

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
        if(current_.empty())
            return false;
        single_file_ = false;
        return true;
    }
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

std::filesystem::path File_walker::relative_path()
{
    return std::filesystem::relative(current_, root_);
}

void File_walker::reset()
{
    if(std::filesystem::is_regular_file(root_))
    {
        single_file_ = true;
        current_ = root_;
        it_ = std::filesystem::recursive_directory_iterator();
    }
    else
    {
        single_file_ = true;
        it_ = std::filesystem::recursive_directory_iterator(root_, std::filesystem::directory_options::skip_permission_denied);
    }
}