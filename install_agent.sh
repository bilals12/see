#!/bin/bash

# create LaunchAgent file
AGENT_DIR="$HOME/Library/LaunchAgents"
AGENT_FILE="$AGENT_DIR/com.user.see.plist"
SYNC_AGENT_FILE="$AGENT_DIR/com.user.see.sync.plist"
SEE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

mkdir -p "$AGENT_DIR"

cat > "$AGENT_FILE" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple/DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>com.user.see</string>
	<key>ProgramArguments</key>
	<array>
		<string>${SEE_DIR}/launch.sh</string>
	</array>
	<key>RunAtLoad</key>
	<true/>
	<key>KeepAlive</key>
	<true/>
	<key>StandardErrorPath</key>
	<string>${SEE_DIR}/see_agent.log</string>
	<key>StandardOutPath</key>
	<string>${SEE_DIR}/see_agent.log</string>
</dict>
</plist>
EOF

cat > "$SYNC_AGENT_FILE" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple/DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.user.see.sync</string>
    <key>ProgramArguments</key>
    <array>
        <string>${SEE_DIR}/github_sync.sh</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>StartInterval</key>
    <integer>21600</integer>
    <key>StandardErrorPath</key>
    <string>${SEE_DIR}/github_sync.log</string>
    <key>StandardOutPath</key>
    <string>${SEE_DIR}/github_sync.log</string>
</dict>
</plist>
EOF

chmod +x "$SEE_DIR/launch.sh"
echo "LaunchAgent file created at $AGENT_FILE"
chmod +x "$SEE_DIR/github_sync.sh"

echo "unloading any existing LaunchAgent..."
launchctl unload "$AGENT_FILE" 2>/dev/null || true
echo "loading LaunchAgent..."
launchctl load -w "$AGENT_FILE"

echo "loading sync LaunchAgent..."
launchctl unload "$SYNC_AGENT_FILE" 2>/dev/null || true
launchctl load -w "$SYNC_AGENT_FILE"
echo "agents installed!"

LOADED=$(launchctl list | grep com.user.see || echo "not found")
if [[ "$LOADED" != "not found" ]]; then
    echo "LaunchAgent successfully installed and loaded!"
else
    echo "warning: LaunchAgent may not have loaded correctly."
    echo "you can try manually with: launchctl load -w $AGENT_FILE"
fi

echo "to remove, use: launchctl unload $AGENT_FILE && rm $AGENT_FILE"
