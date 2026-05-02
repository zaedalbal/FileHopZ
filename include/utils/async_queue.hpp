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

                auto handler_executor = boost::asio::get_associated_executor(
                    waiter->handler,
                    executor_
                );

                boost::asio::dispatch(
                    handler_executor,
                    [waiter = std::move(waiter), value = std::move(value)] mutable
                    {
                        waiter->handler(boost::system::error_code{}, std::move(value));
                    }
                );
            }
            else
                queue_.push_back(std::move(value));
        }

        template <typename CompletionToken = boost::asio::use_awaitable_t<>>
        auto pop(CompletionToken token = {})
        {
            return boost::asio::async_initiate<
                CompletionToken,
                void(boost::system::error_code, T)
            >(
                [this](auto handler)
                {
                    // есть готовое значение — сразу завершаем
                    if(!queue_.empty())
                    {
                        auto value = std::move(queue_.front());
                        queue_.pop_front();

                        auto ex = boost::asio::get_associated_executor(handler, executor_);

                        boost::asio::dispatch(
                            ex,
                            [h = std::move(handler), v = std::move(value)] mutable
                            {
                                h(boost::system::error_code{}, std::move(v));
                            }
                        );

                        return;
                    }

                    // иначе ставим в очередь ожидания
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

                                auto ex = boost::asio::get_associated_executor(
                                    waiter->handler,
                                    executor_
                                );

                                boost::asio::dispatch(
                                    ex,
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

                    waiters_.push_back(std::move(waiter));
                },
                token
            );
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