#pragma once
#include <boost/asio.hpp>
#include <deque>
#include <memory>
#include <algorithm>

template <typename T>
class Async_value
{
    public:
        explicit Async_value(boost::asio::any_io_executor executor, T init = T{})
        :   strand_(boost::asio::make_strand(executor)),
            value_(std::move(init)),
            version_(0)
        {}

        T get() const
        {
            return value_;
        }

        void set(T new_value)
        {
            boost::asio::dispatch(
                strand_,
                [self = this, value = std::move(new_value)]() mutable
                {
                    self->value_ = std::move(value);
                    ++self->version_;

                    for(auto& w : self->waiters_)
                    {
                        auto ex = boost::asio::get_associated_executor(
                            w->handler,
                            self->strand_
                        );

                        boost::asio::dispatch(
                            ex,
                            [w, v = self->value_]() mutable
                            {
                                w->handler(boost::system::error_code{}, std::move(v));
                            }
                        );
                    }

                    self->waiters_.clear();
                }
            );
        }

        template <typename CompletionToken = boost::asio::use_awaitable_t<>>
        auto wait(CompletionToken token = {})
        {
            return boost::asio::async_initiate<
                CompletionToken,
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
                                    strand_
                                );

                                boost::asio::dispatch(
                                    ex,
                                    [waiter]() mutable
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
        };

        boost::asio::strand<boost::asio::any_io_executor> strand_;

        T value_;
        std::size_t version_;

        std::deque<std::shared_ptr<Waiter>> waiters_;
};