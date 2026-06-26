#!/bin/bash

TEST_DIR="test_transfer"
RECV_DIR="test_transfer_recv"
SEND_LOG="filehopz_send.log"
RECV_LOG="filehopz_recv.log"

rm -rf "$RECV_DIR"
rm -f "$SEND_LOG" "$RECV_LOG"

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
printf "y\n" | filehopz recv 12345 "$RECV_DIR" > "$RECV_LOG" 2>&1 &
recv_pid=$!
child_pids+=($recv_pid)

# время подняться
sleep 0.2

# отправка
filehopz send 127.0.0.1 12345 "$TEST_DIR" > "$SEND_LOG" 2>&1 &
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
check_regular_files() {
    while IFS= read -r dir; do
        local relative_dir="${dir#$TEST_DIR/}"
        if [ "$dir" = "$TEST_DIR" ]; then
            relative_dir=""
        fi

        if [ ! -d "$RECV_DIR/$relative_dir" ]; then
            echo "missing directory: $RECV_DIR/$relative_dir"
            return 1
        fi
    done < <(find "$TEST_DIR" -type d)

    while IFS= read -r file; do
        local relative_file="${file#$TEST_DIR/}"
        if [ ! -f "$RECV_DIR/$relative_file" ]; then
            echo "missing file: $RECV_DIR/$relative_file"
            return 1
        fi

        if ! cmp -s "$file" "$RECV_DIR/$relative_file"; then
            echo "file differs: $relative_file"
            return 1
        fi
    done < <(find "$TEST_DIR" -type f)
}

check_symlink() {
    local path="$1"
    local expected_target="$2"

    if [ ! -L "$RECV_DIR/$path" ]; then
        echo "missing symlink: $RECV_DIR/$path"
        return 1
    fi

    local actual_target
    actual_target=$(readlink "$RECV_DIR/$path")
    if [ "$actual_target" != "$expected_target" ]; then
        echo "bad symlink target: $RECV_DIR/$path"
        echo "expected: $expected_target"
        echo "actual:   $actual_target"
        return 1
    fi
}

if check_regular_files \
    && check_symlink "link_to_file_n1" "file_n1" \
    && check_symlink "link_to_nested_dir" "nested_dir" \
    && check_symlink "broken_link" "missing_target" \
    && check_symlink "relative_target_link" "../test_transfer/file_n2"; then
    echo "test successful"
    exit 0
else
    echo "test failed"
    exit 1
fi
