#include "logger/logger.hpp"
#include <memory>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <filesystem>
#include <cstdlib>

void logger::init()
{
    std::filesystem::path log_path = std::getenv("HOME");
    log_path /= ".local/share/filehopz/logs/filehopz.log";
    std::filesystem::create_directories(log_path.parent_path());

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    auto file_sync =
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path.string(),
            1024ULL * 1024 * 1024,
            3
        );

    auto logger =
        std::make_shared<spdlog::logger>(
                "filehopz",
                spdlog::sinks_init_list{
                    console_sink,
                    file_sync
                }
            );

    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::trace);
}
