#!/bin/bash

SEE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PID_FILE="${SEE_DIR}/.see.pid"

if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if ps -p "$PID" > /dev/null 2>&1; then
        kill "$PID"
        rm "$PID_FILE"
        echo "see stopped (PID: $PID)"
    else
        echo "see not running (stale PID file removed)"
        rm "$PID_FILE"
    fi
else
    echo "see not running"
fi