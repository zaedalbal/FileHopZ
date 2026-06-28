#include "utils/file_builder.hpp"
#include "errors/filehopz_error.hpp"

#include <chrono>
#include <string>
#include <unistd.h>

namespace
{
    // std::filesystem отдаёт std::error_code, остальной проект ждёт boost::system
    boost::system::error_code to_boost_error(const std::error_code& ec)
    {
        if(!ec)
            return {};

        return boost::system::error_code(ec.value(), boost::system::generic_category());
    }

    // Запрещаем компоненты, которые могут вывести путь из staging/output
    bool is_forbidden_path_part(const std::filesystem::path& part)
    {
        return part == ".." || part.empty();
    }
}

File_builder::File_builder(const std::filesystem::path& root)
: root_(root)
{
    std::error_code ec;
    std::filesystem::create_directories(root_, ec);
    if(ec)
        return;

    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto pid = static_cast<long long>(getpid());
    for(std::size_t attempt = 0; attempt < 100; ++attempt)
    {
        // Staging лежит внутри output, чтобы rename не пересекал файловые системы
        auto name =
            ".filehopz-tmp-" + std::to_string(pid) + "-" + std::to_string(now) + "-" + std::to_string(attempt);
        auto candidate = root_ / name;
        if(std::filesystem::create_directory(candidate, ec))
        {
            staging_root_ = std::move(candidate);
            return;
        }

        if(ec && ec != std::errc::file_exists)
            return;

        ec.clear();
    }
}

boost::system::error_code File_builder::create_directory(const std::filesystem::path& relative_path)
{
    if(staging_root_.empty())
        return boost::system::errc::make_error_code(boost::system::errc::no_such_file_or_directory);

    auto ec = validate_relative_path(relative_path);
    if(ec)
        return ec;

    std::error_code fs_ec;
    std::filesystem::create_directories(staged_path(relative_path), fs_ec);
    return to_boost_error(fs_ec);
}

boost::system::error_code File_builder::create_file(const std::filesystem::path& relative_path, uint32_t file_id)
{
    if(staging_root_.empty())
        return boost::system::errc::make_error_code(boost::system::errc::no_such_file_or_directory);

    auto ec = validate_relative_path(relative_path);
    if(ec)
        return ec;

    // До commit пишем только во временное дерево
    auto full_path = staged_path(relative_path);
    std::error_code fs_ec;
    std::filesystem::create_directories(full_path.parent_path(), fs_ec);
    if(fs_ec)
        return to_boost_error(fs_ec);

    auto file = std::make_unique<std::ofstream>(full_path, std::ios::binary | std::ios::trunc);
    if(!file->is_open())
        return filehopz::Error_code::file_open_failed;

    open_files_[file_id] = Open_file
    {
        .stream = std::move(file)
    };

    return {};
}

boost::system::error_code File_builder::create_symlink(
    const std::filesystem::path& relative_path,
    const std::filesystem::path& target
)
{
    if(staging_root_.empty())
        return boost::system::errc::make_error_code(boost::system::errc::no_such_file_or_directory);

    auto ec = validate_relative_path(relative_path);
    if(ec)
        return ec;

    auto full_path = staged_path(relative_path);
    std::error_code fs_ec;
    std::filesystem::create_directories(full_path.parent_path(), fs_ec);
    if(fs_ec)
        return to_boost_error(fs_ec);

    // В staging путь должен быть новым, иначе sender прислал конфликтующее дерево
    auto status = std::filesystem::symlink_status(full_path, fs_ec);
    if(fs_ec && status.type() != std::filesystem::file_type::not_found)
        return to_boost_error(fs_ec);
    fs_ec.clear();

    if(std::filesystem::exists(status) || std::filesystem::is_symlink(status))
        return boost::system::errc::make_error_code(boost::system::errc::file_exists);

    std::filesystem::create_symlink(target, full_path, fs_ec);
    return to_boost_error(fs_ec);
}

boost::system::error_code File_builder::write(const char* data, std::size_t size, uint32_t file_id)
{
    auto it = open_files_.find(file_id);
    if(it == open_files_.end())
        return filehopz::Error_code::unknown_file_id;

    // static_cast: ofstream::write принимает std::streamsize, а size — std::size_t
    it->second.stream->write(data, static_cast<std::streamsize>(size));

    if(it->second.stream->bad() || it->second.stream->fail())
        return filehopz::Error_code::file_write_failed;

    return {};
}

boost::system::error_code File_builder::close_file(uint32_t file_id)
{
    auto it = open_files_.find(file_id);
    if(it == open_files_.end())
        return filehopz::Error_code::unknown_file_id;

    it->second.stream->close();
    if(it->second.stream->bad() || it->second.stream->fail())
        return filehopz::Error_code::file_write_failed;

    open_files_.erase(it);

    return {};
}

boost::system::error_code File_builder::commit()
{
    if(staging_root_.empty())
        return boost::system::errc::make_error_code(boost::system::errc::no_such_file_or_directory);

    auto ec = close_all();
    if(ec)
        return ec;

    std::error_code fs_ec;
    std::filesystem::recursive_directory_iterator it(
        staging_root_,
        std::filesystem::directory_options::skip_permission_denied,
        fs_ec
    );
    if(fs_ec)
        return to_boost_error(fs_ec);

    std::filesystem::recursive_directory_iterator end;
    while(it != end)
    {
        const auto entry = *it;
        it.increment(fs_ec);
        if(fs_ec)
            return to_boost_error(fs_ec);

        auto relative_path = entry.path().lexically_relative(staging_root_);
        auto destination = final_path(relative_path);
        auto status = entry.symlink_status(fs_ec);
        if(fs_ec)
            return to_boost_error(fs_ec);

        // Директории коммитятся созданием, содержимое переносится отдельными rename
        if(std::filesystem::is_directory(status))
        {
            auto destination_status = std::filesystem::symlink_status(destination, fs_ec);
            if(fs_ec && destination_status.type() != std::filesystem::file_type::not_found)
                return to_boost_error(fs_ec);
            fs_ec.clear();

            if(std::filesystem::exists(destination_status) && !std::filesystem::is_directory(destination_status))
                return boost::system::errc::make_error_code(boost::system::errc::file_exists);

            std::filesystem::create_directories(destination, fs_ec);
            if(fs_ec)
                return to_boost_error(fs_ec);

            continue;
        }

        std::filesystem::create_directories(destination.parent_path(), fs_ec);
        if(fs_ec)
            return to_boost_error(fs_ec);

        auto destination_status = std::filesystem::symlink_status(destination, fs_ec);
        if(fs_ec && destination_status.type() != std::filesystem::file_type::not_found)
            return to_boost_error(fs_ec);
        fs_ec.clear();

        if(std::filesystem::is_directory(destination_status))
            return boost::system::errc::make_error_code(boost::system::errc::is_a_directory);

        // Один rename заменяет только конкретный файл или symlink, не всё дерево
        std::filesystem::rename(entry.path(), destination, fs_ec);
        if(fs_ec)
            return to_boost_error(fs_ec);
    }

    std::filesystem::remove_all(staging_root_, fs_ec);
    return to_boost_error(fs_ec);
}

void File_builder::cleanup()
{
    if(staging_root_.empty())
        return;

    close_all();

    // При ошибке передачи удаляем только staging, пользовательские файлы не трогаем
    std::error_code ec;
    std::filesystem::remove_all(staging_root_, ec);
}

boost::system::error_code File_builder::close_all()
{
    for(auto& [_, file] : open_files_)
    {
        file.stream->close();
        if(file.stream->bad() || file.stream->fail())
        {
            open_files_.clear();
            return filehopz::Error_code::file_write_failed;
        }
    }

    open_files_.clear();
    return {};
}

boost::system::error_code
File_builder::validate_relative_path(const std::filesystem::path& relative_path) const
{
    // Пути приходят из сети, поэтому принимаем только безопасный относительный путь
    if(relative_path.empty() || relative_path.is_absolute())
        return boost::system::errc::make_error_code(boost::system::errc::invalid_argument);

    for(const auto& part : relative_path)
    {
        if(is_forbidden_path_part(part))
            return boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
    }

    return {};
}

std::filesystem::path File_builder::staged_path(const std::filesystem::path& relative_path) const
{
    return staging_root_ / relative_path;
}

std::filesystem::path File_builder::final_path(const std::filesystem::path& relative_path) const
{
    return root_ / relative_path;
}
