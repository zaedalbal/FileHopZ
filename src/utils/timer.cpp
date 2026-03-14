#include "utils/timer.hpp"
#include <chrono>

Timer::Timer(boost::asio::any_io_executor& executor, std::size_t interval)
: timer_(executor), interval_(interval), is_running_(false)
{}

void Timer::start()
{
    if(is_running_)
        return;
    is_running_ = true;
    arm();
}

void Timer::refresh()
{
    if(!is_running_)
        start();
    else
        arm();
}

void Timer::stop()
{
    is_running_ = false;
    timer_.cancel();
}

void Timer::set_callback_func(std::function<void()> func)
{
    callback_ = (std::move(func));
}

void Timer::arm()
{
    timer_.cancel();
    timer_.expires_after(std::chrono::milliseconds(interval_));
    auto self = shared_from_this();
    timer_.async_wait([self](const boost::system::error_code& ec)
    {
        if(!ec && self->is_running_ && self->callback_)
            self->callback_();
    });
}