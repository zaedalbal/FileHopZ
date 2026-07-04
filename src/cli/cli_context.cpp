#include "cli/cli_context.hpp"

#include <CLI/CLI.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

namespace
{
    // Контекст создается один раз после успешного parse
    std::unique_ptr<Cli_context> cli_context;

    bool has_help_arg(int argc, char* argv[])
    {
        for(int i = 1; i < argc; ++i)
        {
            std::string_view arg = argv[i];
            if(arg == "-h" || arg == "--help")
                return true;
        }

        return false;
    }

    void print_root_help()
    {
        std::cout <<
            "FileHopZ - lightweight LAN file transfer tool\n\n"
            "Usage:\n"
            "  filehopz send <address> <port> <file/directory>\n"
            "  filehopz recv <port> <output_directory>\n\n"
            "Modes:\n"
            "  send\n"
            "    address          Receiver address\n"
            "    port             Receiver port in range 1..65535\n"
            "    file/directory   File or directory to send\n\n"
            "  recv\n"
            "    port             Listening port in range 1..65535\n"
            "    output_directory Output directory\n\n"
            "Flags:\n"
            "  -h, --help         Print this help message and exit\n";
    }

    void configure_app(
        CLI::App& app,
        Cli_context::Send_args& send_args,
        Cli_context::Recv_args& recv_args,
        std::uint32_t& send_port,
        std::uint32_t& recv_port
    )
    {
        // Выбор ровно одного сценария запуска
        app.require_subcommand(1, 1);

        auto send = app.add_subcommand("send", "Send file or directory");
        send->add_option("address", send_args.address, "Receiver address")->required();
        // Проверка диапазона до сужения типа до uint16_t
        send->add_option("port", send_port, "Receiver port")
            ->required()
            ->check(CLI::Range(1, 65535));
        send->add_option("file/directory", send_args.input_path, "File or directory to send")
            ->required()
            ->check(CLI::ExistingPath);

        auto recv = app.add_subcommand("recv", "Receive files");
        // Проверка диапазона до сужения типа до uint16_t
        recv->add_option("port", recv_port, "Listening port")
            ->required()
            ->check(CLI::Range(1, 65535));
        recv->add_option("output_directory", recv_args.output_dir, "Output directory")
            ->required();
    }
}

Cli_context::Cli_context(Command command)
    : command_(std::move(command))
{
}

Cli_context::Init_result Cli_context::init(int argc, char* argv[])
{
    if(has_help_arg(argc, argv))
    {
        print_root_help();
        return {
            .should_exit = true,
            .exit_code = 0
        };
    }

    Send_args send_args;
    Recv_args recv_args;
    std::uint32_t send_port = 0;
    std::uint32_t recv_port = 0;

    CLI::App app{"FileHopZ - lightweight LAN file transfer tool", "filehopz"};
    configure_app(app, send_args, recv_args, send_port, recv_port);

    try
    {
        // Изоляция exception-based API CLI11 внутри CLI adapter
        app.parse(argc, argv);
    }
    catch(const CLI::ParseError& error)
    {
        return {
            .should_exit = true,
            .exit_code = app.exit(error)
        };
    }
    catch(const std::exception& error)
    {
        std::cerr << "CLI internal error: " << error.what() << "\n";
        return {
            .should_exit = true,
            .exit_code = 1
        };
    }
    catch(...)
    {
        std::cerr << "CLI internal error\n";
        return {
            .should_exit = true,
            .exit_code = 1
        };
    }

    if(app.got_subcommand("send"))
    {
        // Безопасное сужение после CLI::Range
        send_args.port = static_cast<std::uint16_t>(send_port);
        cli_context.reset(new Cli_context(std::move(send_args)));
        return {};
    }

    if(app.got_subcommand("recv"))
    {
        // Безопасное сужение после CLI::Range
        recv_args.port = static_cast<std::uint16_t>(recv_port);
        cli_context.reset(new Cli_context(std::move(recv_args)));
        return {};
    }

    std::cerr << "CLI internal error: command was not selected\n";
    return {
        .should_exit = true,
        .exit_code = 1
    };
}

const Cli_context& Cli_context::instance()
{
    if(cli_context == nullptr)
    {
        // Защита от доступа до успешного init
        std::cerr << "CLI internal error: context is not initialized\n";
        std::abort();
    }

    return *cli_context;
}

Cli_context::Mode Cli_context::mode() const
{
    if(std::holds_alternative<Send_args>(command_))
    {
        return Mode::send;
    }

    return Mode::recv;
}

const Cli_context::Command& Cli_context::command() const
{
    return command_;
}

const Cli_context::Send_args* Cli_context::send_args() const
{
    return std::get_if<Send_args>(&command_);
}

const Cli_context::Recv_args* Cli_context::recv_args() const
{
    return std::get_if<Recv_args>(&command_);
}
