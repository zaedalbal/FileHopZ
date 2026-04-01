#pragma once
#include <thread>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class Thread_pool
{
    public:
        // берется кол-во потоков исходя из характеристик системы
        explicit Thread_pool();

        // кол-во потоков указывается напрямую
        explicit Thread_pool(std::size_t thread_count);

        ~Thread_pool();

        // добавление задачи в очередь
        void submit(std::function<void()> task);
    
    private:
        // запуск жизненого цикла worker'а
        void worker_loop();

    private:
        std::vector<std::thread> workers_;

        std::queue<std::function<void()>> tasks_;
        
        std::mutex mutex_;

        std::condition_variable cv_;

        std::atomic<bool> done_;
};