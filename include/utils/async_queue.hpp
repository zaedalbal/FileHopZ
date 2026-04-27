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

                boost::asio::post(
                    executor_,
                    [handler = std::move(handler), value = std::move(value)]
                    {
                        handler(std::move(value));
                    }
                )
            }
            else
                queue_.push_back(std::move(value));
        }

        boost::asio::awaitable<T> pop()
        {
            if(!queue_.empty())
            {
                auto value = queue_.front();
                queue_.pop_front();
                co_return value;
            }

            auto result = co_await boost::asio::async_initiate<
                boost::asio::use_awaitable_t<>,
                void(T);
            >(
                [this](auto handler)
                {
                    waiters_.push_back(std::move(handler));
                },
                boost::asio::use_awaitable
            );
            
            co_return result;
        }

    private:
        boost::asio::any_io_executor executor_;

        std::deque<T> queue_;

        std::deque<std::function<void(T)>> waiters_;
};