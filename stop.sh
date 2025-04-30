#!/bin/bash

SEE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PID_FILE="${SEE_DIR}/.see.pid"

# remove cron job
remove_cron() {
    crontab -l | grep -v "launch.sh --update-github" | crontab -
}

# check if PID file exists
if [ ! -f "$PID_FILE" ]; then
    echo "see is not running (no PID file found)"
    remove_cron
    exit 0
fi

# read PID and stop process
PID=$(cat "$PID_FILE")
if ps -p "$PID" > /dev/null 2>&1; then
    # try gentle SIGTERM
    kill "$PID"
    sleep 2

    # force kill
    if ps -p "$PID" > /dev/null 2>&1; then
        kill -9 "$PID"
        sleep 1
    fi

    # verify
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "error: failed to stop see (PID: $PID)"
        exit 1
    else
        echo "see stopped (PID: $PID)"
    fi
else
    echo "see was not running (stale PID file)"
fi

# clean
rm -f "$PID_FILE"
remove_cron

echo "cleanup complete!"