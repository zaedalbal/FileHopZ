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

                    auto waiters = std::move(self->waiters_);

                    for(auto& w : waiters)
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
                }
            );
        }

        // молча обновить value_ без будить waiter'ов; используется, когда выпуск
        // waiter'ов управляется отдельно через notify_one() (например, cwnd: на
        // каждый ACK значение меняется через update(), а отпускается ровно один
        // waiter через notify_one() — без wake-all burst'а)
        void update(T new_value)
        {
            boost::asio::dispatch(
                strand_,
                [self = this, value = std::move(new_value)]() mutable
                {
                    self->value_ = std::move(value);
                    ++self->version_;
                }
            );
        }

        // разбудить ровно одну ожидающую корутину (FIFO) без изменения value_;
        // используется для rate-based контроля cwnd: на каждый ACK значение
        // меняется через update() (молча, без будить-всех), а notify_one()
        // отпускает ровно одного waiter'а — на каждый ACK ровно одна новая
        // отправка, без пачек и без 10-мс лага (всё синхронно на strand_)
        void notify_one()
        {
            boost::asio::dispatch(
                strand_,
                [self = this]() mutable
                {
                    if(self->waiters_.empty())
                        return;

                    auto waiter = std::move(self->waiters_.front());
                    self->waiters_.pop_front();

                    auto ex = boost::asio::get_associated_executor(
                        waiter->handler,
                        self->strand_
                    );

                    auto v = self->value_;

                    boost::asio::dispatch(
                        ex,
                        [waiter, v = std::move(v)]() mutable
                        {
                            waiter->handler(boost::system::error_code{}, std::move(v));
                        }
                    );
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