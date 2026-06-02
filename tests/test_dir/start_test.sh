#!/bin/bash

TEST_DIR="test_transfer"
RECV_DIR="test_transfer_recv"

rm -rf "$RECV_DIR"

# массив для хранения PID дочерних процессов
declare -a child_pids=()

# функция очистки процессов
cleanup() {
    if [ ${#child_pids[@]} -eq 0 ]; then
        return
    fi
    echo "Cleaning up ${#child_pids[@]} processes..."
    for pid in "${child_pids[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "Killing process $pid"
            kill -TERM "$pid" 2>/dev/null || true
        fi
    done
    # немного подождать для нормального завершения
    sleep 1
    # принудительное убийство оставшихся
    for pid in "${child_pids[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "Force killing process $pid"
            kill -KILL "$pid" 2>/dev/null || true
        fi
    done
}

# обработчик сигналов для очистки при зависании
trap cleanup EXIT INT TERM

# запуск recv и сразу дать ему "y"
printf "y\n" | filehopz recv 12345 "$RECV_DIR" &
recv_pid=$!
child_pids+=($recv_pid)

# время подняться
sleep 0.2

# отправка
filehopz send 127.0.0.1 12345 "$TEST_DIR" &
send_pid=$!
child_pids+=($send_pid)

# ждать завершения с таймаутом (30 секунд)
# timeout в фоне, который убьёт процессы при истечении
(
    sleep 30
    echo "Timeout reached, killing processes..."
    for pid in "${child_pids[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "Force killing process $pid"
            kill -KILL "$pid" 2>/dev/null || true
        fi
    done
) &
timeout_monitor_pid=$!
child_pids+=($timeout_monitor_pid)

# ждать оба процесса, но без infinite wait
# Если send завершился - снимаем его из списка
wait $send_pid 2>/dev/null && child_pids=("${child_pids[@]/$send_pid}") || true
wait $recv_pid 2>/dev/null && child_pids=("${child_pids[@]/$recv_pid}") || true

# удаление timeout monitor из списка перед очисткой
child_pids=("${child_pids[@]/$timeout_monitor_pid}")
kill $timeout_monitor_pid 2>/dev/null || true

# проверка результата
if diff -rq "$TEST_DIR" "$RECV_DIR" > /dev/null; then
    echo "test successful"
else
    echo "test failed"
    diff -rq "$TEST_DIR" "$RECV_DIR"
fi

exit 1
