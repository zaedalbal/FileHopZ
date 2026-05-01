#pragma once
#include <boost/asio.hpp>
#include <deque>
#include <optional>
#include <algorithm>

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
                auto waiter = std::move(waiters_.front());
                waiters_.pop_front();
                
                // получение executor'а с которым должен выполняться handler
                auto handler_executor = boost::asio::get_associated_executor(waiter->handler, executor_);

                // dispatch выполняет сразу, если уже в нужном executor'е,
                // иначе отправить в нужный executor на выполнение
                boost::asio::dispatch(
                    handler_executor,
                    // mutable чтобы делать std::move()
                    [waiter = std::move(waiter), value = std::move(value)] mutable
                    {
                    // пробуждение ожидающей коруитны и возращение ей value
                        waiter->handler(boost::system::error_code{}, std::move(value));
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
                void(boost::system::error_code, T)
            >(
                [this](auto handler)
                {
                    auto waiter = std::make_shared<Waiter>();

                    waiter->handler = std::move(handler);

                    auto slot = boost::asio::get_associated_cancellation_slot(waiter->handler);

                    if(slot.is_connected())
                    {
                        slot.assign(
                            [this, waiter](boost::asio::cancellation_type)
                            {
                                waiter->cancelled = true;

                                waiters_.erase(
                                    std::remove(
                                        waiters_.begin(),
                                        waiters_.end(),
                                        waiter
                                    ),
                                    waiters_.end()
                                );
                                auto executor = boost::asio::get_associated_executor(
                                    waiter->handler,
                                    executor_
                                );

                                boost::asio::dispatch(
                                    executor,
                                    [waiter]()
                                    {
                                        waiter->handler(
                                            boost::asio::error::operation_aborted,
                                            T{}
                                        );
                                    }
                                );
                            }
                        );
                    }
                    waiters_.push_back(waiter);
                },
                // указание что надо использовать awaitable
                token
            );
            
            co_return result;
        }

    private:
        struct Waiter
        {
            boost::asio::any_completion_handler<void(boost::system::error_code, T)> handler;

            bool cancelled = false;
        };

        boost::asio::any_io_executor executor_;

        std::deque<T> queue_;

        std::deque<std::shared_ptr<Waiter>> waiters_;
};