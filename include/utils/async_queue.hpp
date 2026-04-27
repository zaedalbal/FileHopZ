#pragma once
#include <boost/asio.hpp>
#include <deque>
#include <optional>

template <typename T>
class Async_queue
{
    public:
        Async_queue(boost::asio::any_io_executor executor)
        :   executor_(executor)
        {}

        void push(T value)
        {
            if(!waiters_.empty())
            {
                auto handler = std::move(waiters_.front());
                waiters_.pop_front();
                
                // получение executor'а с которым должен выполняться handler
                auto handler_executor = boost::asio::get_associated_executor(handler, executor_);

                // dispatch выполняет сразу, если уже в нужном executor'е,
                // иначе отправить в нужный executor на выполнение
                boost::asio::dispatch(
                    handler_executor,
                    // mutable чтобы делать std::move()
                    [handler = std::move(handler), value = std::move(value)] mutable
                    {
                    // пробуждение ожидающей коруитны и возращение ей value
                        handler(std::move(value));
                    }
                );
            }
            else
                queue_.push_back(std::move(value));
        }

        boost::asio::awaitable<T> pop()
        {
            if(!queue_.empty())
            {
                auto value = std::move(queue_.front());
                queue_.pop_front();
                co_return std::move(value);
            }

            // указание что будет awaitable результат (для использования co_await)
            auto token = boost::asio::use_awaitable;

            // async_initiate создает асинхронную операцию и даёт handler,
            // который предоставляет продолжение корутины
            auto result = co_await boost::asio::async_initiate<
                decltype(token),
                void(T)
            >(
                [this](auto handler)
                {
                    // сохранение для того чтобы разбудить ожидающего
                    waiters_.push_back(std::move(handler));
                },
                // указание что надо использовать awaitable
                token
            );
            
            co_return result;
        }

    private:
        boost::asio::any_io_executor executor_;

        std::deque<T> queue_;

        std::deque<
        boost::asio::any_completion_handler<void(T)>
        > waiters_;
};