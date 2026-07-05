#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>

class Cli_context final
{
public:
    // Выбранный сценарий запуска
    enum class Mode
    {
        send,
        recv
    };

    struct Send_args
    {
        std::string address;
        std::uint16_t port = 0;
        std::filesystem::path input_path;
    };

    struct Recv_args
    {
        std::uint16_t port = 0;
        std::filesystem::path output_dir;
        bool auto_accept = false;
    };

    using Command = std::variant<Send_args, Recv_args>;

    // Результат init для немедленного завершения main
    struct Init_result
    {
        bool should_exit = false;
        int exit_code = 0;
    };

    // Разбирает argv, печатает help/errors и подготавливает singleton
    static Init_result init(int argc, char* argv[]);

    // Возвращает распарсенный контекст после успешного init
    static const Cli_context& instance();

    // Возвращает выбранный режим без доступа к аргументам режима
    [[nodiscard]] Mode mode() const;

    // Возвращает выбранную команду для обработки через std::visit
    [[nodiscard]] const Command& command() const;

    // Возвращает аргументы send или nullptr, если выбран не send
    [[nodiscard]] const Send_args* send_args() const;

    // Возвращает аргументы recv или nullptr, если выбран не recv
    [[nodiscard]] const Recv_args* recv_args() const;

private:
    explicit Cli_context(Command command);

    Command command_;
};
