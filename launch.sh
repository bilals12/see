#!/bin/bash

SEE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PID_FILE="${SEE_DIR}/.see.pid"
LOG_FILE="${SEE_DIR}/see.log"
GITHUB_LOG="${SEE_DIR}/github_updates.log"

# update github
update_github() {
    if [ -f "${SEE_DIR}/.env" ]; then
        source "${SEE_DIR}/.env"

        for file in "cumulative_data.csv" "past_24_hours_data.csv"; do
            if [ - f "$file" ]; then
                content=$(base64 "$file")
                timestamp=$(date -u +"%Y-%m-%d %H:%M:%S")

                curl -s -X PUT \
                    -H "Authorization: Bearer $GITHUB_TOKEN" \
                    -H "Accept: application/vnd.github+json" \
                    -H "X-GitHub-Api-Version: 2022-11-28" \
                    "https://api.github.com/repos/$GITHUB_REPO/contents/$file" \
                    -d "{\"message\":\"Update $file - $timestamp\",\"content\":\"$content\",\"committer\":{\"name\":\"bilals12\",\"email\":\"bilal.s12@protonmail.com\"}}" \
                    >> "$GITHUB_LOG" 2>&1
            fi
        done
    fi
}

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

# save PID and confirm
echo $PID > "$PID_FILE"
echo "see started with PID: $PID"
echo "logs available at: $LOG_FILE"

# cron job (runs every 6 hours)
(crontab -l 2>/dev/null; echo "0 */6 * * * cd ${SEE_DIR} && $SEE_DIR/launch.sh --update-github") | crontab -