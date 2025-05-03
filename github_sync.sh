#!/bin/bash

SEE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
GITHUB_LOG="${SEE_DIR}/github_updates.log"

# update github with the latest data
update_github() {
    if [ -f "${SEE_DIR}/.env" ]; then
        source "${SEE_DIR}/.env"

        echo "$(date): starting GitHub sync" >> "$GITHUB_LOG"

        for file in "cumulative_data.csv" "past_24_hours_data.csv"; do
            if [ -f "$SEE_DIR/$file" ]; then
                # first get the SHA of the existing file
                echo "$(date): getting SHA for $file" >> "$GITHUB_LOG"
                
                SHA_RESPONSE=$(curl -s -H "Authorization: Bearer $GITHUB_TOKEN" \
                              -H "Accept: application/vnd.github+json" \
                              "https://api.github.com/repos/$GITHUB_REPO/contents/$file")
                
                # check if file exists and extract SHA
                if echo "$SHA_RESPONSE" | grep -q "sha"; then
                    # extract SHA using grep and cut
                    SHA=$(echo "$SHA_RESPONSE" | grep '"sha":' | head -1 | sed 's/.*"sha": "\([^"]*\)".*/\1/')
                    echo "$(date): found SHA: $SHA" >> "$GITHUB_LOG"
                    
                    # encode file content
                    content=$(base64 -i "$SEE_DIR/$file")
                    timestamp=$(date -u +"%Y-%m-%d %H:%M:%S")
                    
                    echo "$(date): updating $file on GitHub" >> "$GITHUB_LOG"
                    
                    # update file with SHA
                    UPDATE_RESPONSE=$(curl -s -X PUT \
                        -H "Authorization: Bearer $GITHUB_TOKEN" \
                        -H "Accept: application/vnd.github+json" \
                        -H "X-GitHub-Api-Version: 2022-11-28" \
                        "https://api.github.com/repos/$GITHUB_REPO/contents/$file" \
                        -d "{\"message\":\"Update $file - $timestamp\",\"content\":\"$content\",\"sha\":\"$SHA\",\"committer\":{\"name\":\"bilals12\",\"email\":\"bilal.s12@protonmail.com\"}}")
                    
                    echo "$UPDATE_RESPONSE" >> "$GITHUB_LOG"
                    
                    # check response for success
                    if echo "$UPDATE_RESPONSE" | grep -q '"content":'; then
                        echo "$(date): successfully updated $file" >> "$GITHUB_LOG"
                    else
                        echo "$(date): failed to update $file" >> "$GITHUB_LOG"
                    fi
                else
                    # file doesn't exist yet, create it
                    echo "$(date): file $file does not exist yet on GitHub, creating it" >> "$GITHUB_LOG"
                    
                    # encode file content
                    content=$(base64 -i "$SEE_DIR/$file")
                    timestamp=$(date -u +"%Y-%m-%d %H:%M:%S")
                    
                    # create new file
                    CREATE_RESPONSE=$(curl -s -X PUT \
                        -H "Authorization: Bearer $GITHUB_TOKEN" \
                        -H "Accept: application/vnd.github+json" \
                        -H "X-GitHub-Api-Version: 2022-11-28" \
                        "https://api.github.com/repos/$GITHUB_REPO/contents/$file" \
                        -d "{\"message\":\"Create $file - $timestamp\",\"content\":\"$content\",\"committer\":{\"name\":\"bilals12\",\"email\":\"bilal.s12@protonmail.com\"}}")
                    
                    echo "$CREATE_RESPONSE" >> "$GITHUB_LOG"
                    
                    # check response for success
                    if echo "$CREATE_RESPONSE" | grep -q '"content":'; then
                        echo "$(date): successfully created $file" >> "$GITHUB_LOG"
                    else
                        echo "$(date): failed to create $file" >> "$GITHUB_LOG"
                    fi
                fi
            else
                echo "$(date): file $file not found locally, skipping" >> "$GITHUB_LOG"
            fi
        done
    else
        echo "$(date): .env file not found, skipping GitHub sync" >> "$GITHUB_LOG"
    fi
}

# run the update
update_github
