# see

A minimal macOS activity tracker for keyboard and mouse use. The chart is on [my website](https://github.com/bilals12/website/tree/main/static/viz).

## How it works

- `see` counts keypresses, mouse movement (in meters), and left, right, and middle clicks.
- Every 5 minutes, `see` writes `past_24_hours_data.csv` (5-minute intervals for the last 24 hours) and `cumulative_data.csv` (totals since the first run).
- Every 15 minutes, a LaunchAgent runs `sync.sh`. The script adds the new rows to `history.csv` (5-minute intervals for the last 7 days), then commits the CSV files and pushes them.
- The website reads the CSV files from `raw.githubusercontent.com` when the page loads. A new data commit does not start a site build.

## Install

```bash
make
./install_agent.sh
```

`install_agent.sh` installs two LaunchAgents. `com.user.see` keeps `see` running and starts it again after a reboot. `com.user.see.sync` runs `sync.sh`.

Add the `see` binary in System Settings > Privacy & Security > Input Monitoring. The entry matches only one build. After each `make`, delete the old `see` entry and add the binary again. If you only turn on the old entry, macOS still denies the permission. Then restart `see`:

```bash
launchctl kickstart -k gui/$(id -u)/com.user.see
```

## Logs

- `see.log`: the output of `see`. It shows a warning when `see` gets no events for an hour.
- `sync.log`: errors from `git commit` and `git push`.

## Requirements

- macOS
- `gcc` (Xcode Command Line Tools)
- An SSH key for GitHub that works with no prompt (in the SSH agent or the macOS keychain)

## License

MIT
