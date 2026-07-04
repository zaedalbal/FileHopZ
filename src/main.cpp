#include <iostream>
#include <boost/asio.hpp>
#include "cli/cli_context.hpp"
#include "errors/filehopz_error.hpp"
#include "sender/sender.hpp"
#include "receiver/receiver.hpp"
#include "logger/logger.hpp"
#include <spdlog/spdlog.h>

namespace
{
    void print_error(const boost::system::error_code& error)
    {
        std::cout << "Error: " << error.message() << "\n";
    }
}

int main(int argc, char* argv[])
{
    auto cli_result = Cli_context::init(argc, argv);
    if(cli_result.should_exit)
    {
        return cli_result.exit_code;
    }

    logger::init();
    SPDLOG_TRACE("logger init");

    const auto& cli = Cli_context::instance();
    boost::asio::io_context io_context;

    if(auto send_args = cli.send_args())
    {
        auto input_path = send_args->input_path;
        auto sender = std::make_shared<Sender>(
            io_context,
            send_args->address,
            send_args->port,
            input_path
        );
        auto errc = boost::asio::co_spawn(
            io_context,
            [sender]()->boost::asio::awaitable<boost::system::error_code>
            {
            co_return co_await sender->start();
            },
            boost::asio::use_future
        );

        io_context.run();

        auto error = errc.get();
        if(error)
        {
            print_error(error);
            return 1;
        }
    }
    else if(auto recv_args = cli.recv_args())
    {
        auto output_dir = recv_args->output_dir;
        auto receiver = std::make_shared<Receiver>(
            io_context,
            recv_args->port,
            output_dir
        );

        auto errc = boost::asio::co_spawn(
            io_context,
            [receiver]()->boost::asio::awaitable<boost::system::error_code>
            {
                co_return co_await receiver->start();
            },
            boost::asio::use_future
        );

        io_context.run();

        auto error = errc.get();
        if(error)
        {
            print_error(error);
            return 1;
        }
    }

    return 0;
}
