#include <boost/asio.hpp>

class Timer : public std::enable_shared_from_this<Timer>
{
    public:
        explicit Timer(boost::asio::any_io_executor& executor, std::size_t interval); // конструктор

        void start(); // запустить таймер

        void refresh(); // обновить таймер

        void stop();  // остановить таймер

        void set_callback_func(std::function<void()> func); // установить функцию, которая вызовется по истечению таймера

    private:
        void arm(); // запуск таймера (вызывается из всех публичных методов)

    private:
        boost::asio::steady_timer timer_; // таймер

        std::size_t interval_; // сколько таймер будет длится (В МИЛЛИСЕКУНДАХ!!!)

        std::function<void()> callback_; // callback функция

        bool is_running_; // запущен ли таймер
};