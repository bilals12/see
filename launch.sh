#!/bin/bash

SEE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PID_FILE="${SEE_DIR}/.see.pid"
LOG_FILE="${SEE_DIR}/see.log"

RUNNING_PID=$(pgrep -x see || echo "")

if [ -n "$RUNNING_PID" ]; then
    echo "see is already running (PID: $RUNNING_PID)"
    echo $RUNNING_PID > "$PID_FILE"
    exit 0
fi

if [ -f "$PID_FILE" ]; then
    rm "$PID_FILE"
fi

# .env file existence
if [ ! -f "${SEE_DIR}/.env" ]; then
    echo "warning: .env file not found in ${SEE_DIR}"
else
    # check
    if ! grep -q "GITHUB_TOKEN" "${SEE_DIR}/.env"; then
        echo "warning: GITHUB_TOKEN not found in .env file"
    fi
    if ! grep -q "GITHUB_REPO" "${SEE_DIR}/.env"; then
        echo "warning: GITHUB_REPO not found in .env file"
    fi
fi

# start in background
cd "$SEE_DIR"
nohup ./see > "$LOG_FILE" 2>&1 &
PID=$!

# verify process is still alive after startup
sleep 2
if ! ps -p "$PID" > /dev/null 2>&1; then
    echo "error: see exited immediately after launch"
    echo "check logs at: $LOG_FILE"
    exit 1
fi

# save PID and confirm
echo $PID > "$PID_FILE"
echo "see started with PID: $PID"
echo "logs available at: $LOG_FILE"
