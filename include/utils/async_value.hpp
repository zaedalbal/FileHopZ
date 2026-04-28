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
            struct awaiter
            {
                Async_value* self;
                std::size_t observed_version;
                std::optional<T> result;

                bool await_ready() const noexcept
                {
                    return self->version_ != observed_version;
                }
            
                void await_suspend(std::coroutine_handle<> handle)
                {
                    auto waiter = std::make_shared<waiter_state>();

                    waiter->resume = [this, handle](T value)
                    {
                        result = std::move(value);
                        handle.resume();
                    };

                    self->waiters_.push_back(waiter);
                }

                T await_resume()
                {
                    return *result;
                }
            };

            return awaiter{
                this,
                version_,
                std::nullopt
            };
        }

    private:
        struct waiter_state
        {
            std::function<void(T)> resume;
        };

        boost::asio::strand<boost::asio::any_io_executor> strand_;

        T value_;
        std::size_t version_;

        std::deque<std::shared_ptr<waiter_state>> waiters_;
};