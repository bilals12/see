#!/bin/bash
# installs two LaunchAgents: `see` (restarted by launchd, also after a reboot)
# and a job that commits + pushes the CSVs every 15 minutes
set -euo pipefail

SEE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
AGENT_DIR="$HOME/Library/LaunchAgents"
DOMAIN="gui/$(id -u)"
mkdir -p "$AGENT_DIR"

cat > "$AGENT_DIR/com.user.see.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>com.user.see</string>
	<key>ProgramArguments</key>
	<array>
		<string>${SEE_DIR}/see</string>
	</array>
	<key>WorkingDirectory</key>
	<string>${SEE_DIR}</string>
	<key>RunAtLoad</key>
	<true/>
	<key>KeepAlive</key>
	<true/>
	<key>StandardOutPath</key>
	<string>${SEE_DIR}/see.log</string>
	<key>StandardErrorPath</key>
	<string>${SEE_DIR}/see.log</string>
</dict>
</plist>
EOF

cat > "$AGENT_DIR/com.user.see.sync.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>com.user.see.sync</string>
	<key>ProgramArguments</key>
	<array>
		<string>${SEE_DIR}/sync.sh</string>
	</array>
	<key>WorkingDirectory</key>
	<string>${SEE_DIR}</string>
	<key>RunAtLoad</key>
	<true/>
	<key>StartInterval</key>
	<integer>900</integer>
	<key>StandardOutPath</key>
	<string>${SEE_DIR}/sync.log</string>
	<key>StandardErrorPath</key>
	<string>${SEE_DIR}/sync.log</string>
</dict>
</plist>
EOF

for label in com.user.see com.user.see.sync; do
    launchctl bootout "$DOMAIN/$label" 2>/dev/null || true
done
# a `see` started by hand would write the same CSVs as the launchd one
pkill -x see || true

for label in com.user.see com.user.see.sync; do
    launchctl bootstrap "$DOMAIN" "$AGENT_DIR/$label.plist"
done

echo "installed. restart: launchctl kickstart -k $DOMAIN/com.user.see"
echo "remove: launchctl bootout $DOMAIN/com.user.see; launchctl bootout $DOMAIN/com.user.see.sync"
