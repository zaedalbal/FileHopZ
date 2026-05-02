#!/bin/bash

set -e

TEST_DIR="test_transfer"
RECV_DIR="test_transfer_recv"

rm -rf "$RECV_DIR"

# запускаем recv и сразу даём ему "y"
printf "y\n" | filehopz recv 12345 "$RECV_DIR" &
recv_pid=$!

# даём ему время подняться
sleep 0.2

# отправка
filehopz send 127.0.0.1 12345 "$TEST_DIR" &
send_pid=$!

# ждём завершения отправки
wait $send_pid
wait $recv_pid

# проверка результата
if diff -rq "$TEST_DIR" "$RECV_DIR" > /dev/null; then
    echo "test successful"
else
    echo "test failed"
    diff -rq "$TEST_DIR" "$RECV_DIR"
fi
