#!/bin/bash

SEE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PID_FILE="${SEE_DIR}/.see.pid"
LOG_FILE="${SEE_DIR}/see.log"

# check if running
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "see is already running (PID: $PID)"
        exit 0
    else
        rm "$PID_FILE"
    fi
fi

# start in background
cd "$SEE_DIR"
nohup ./see > "$LOG_FILE" 2>&1 &
PID=$!

# save PID and confirm
echo $PID > "$PID_FILE"
echo "see started with PID: $PID"
echo "logs available at: $LOG_FILE"