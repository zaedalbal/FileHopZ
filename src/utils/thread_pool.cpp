#include "utils/thread_pool.hpp"

Thread_pool::Thread_pool()
: Thread_pool(std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4)
{}

Thread_pool::Thread_pool(std::size_t thread_count)
: done_(false)
{
    for(std::size_t i = 0; i < thread_count; ++i)
    {
        workers_.emplace_back([this]{worker_loop();});
    }
}

Thread_pool::~Thread_pool()
{
    done_ = true;
    cv_.notify_all();
    for(auto& i : workers_)
    {
        if(i.joinable())
            i.join();
    }
}

void Thread_pool::submit(std::function<void()> task)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    } // фигурные скобки, чтоб unique_lock уничтожился сразу, а не ждал пока cv_.notify_one()
    cv_.notify_one();
}

void Thread_pool::worker_loop()
{
    while(!done_)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]{return done_ || !tasks_.empty();}); // поток начинает работу если done_ = true или в очереди есть задачи
            if(done_ && tasks_.empty())
                return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}