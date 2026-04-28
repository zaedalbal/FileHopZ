#include <boost/asio.hpp>
#include <deque>
#include <coroutine>
#include <optional>
#include <functional>
#include <memory>

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

                    for(auto& it : self->waiters_)
                        it->resume(self->value_);
                    
                    self->waiters_.clear();
                }
            );
        }

        auto wait()
        {
            auto token = boost::asio::use_awaitable;
            return boost::asio::async_initiate<
                decltype(token),
                void(T)
            >(
                [this](auto handler)
                {
                    auto waiter = std::make_shared<waiter_state>();

                    waiter->resume = [handler = std::move(handler)](T value) mutable
                    {
                        handler(std::move(value));
                    };

                    waiters_.push_back(waiter);
                },
                token
            );

        }

    private:
        struct waiter_state
        {
            boost::asio::any_completion_handler<void(T)> resume;
        };

        boost::asio::strand<boost::asio::any_io_executor> strand_;

        T value_;
        std::size_t version_;

        std::deque<std::shared_ptr<waiter_state>> waiters_;
};