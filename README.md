# see

A minimal macOS activity tracker that monitors keyboard and mouse usage.

## Overview

`see` runs silently in the background, collecting data about:
- Keystrokes
- Mouse movements (in meters)
- Mouse clicks (left, right)

Data is collected every minute and stored locally. An hourly snapshot is pushed to GitHub for visualization.

## Installation

1. Clone the repository:
```bash
git clone https://github.com/bilals12/see.git
cd see
```

2. Build:
```bash
make
```

3. Add these aliases to your shell config (`~/.zshrc` or `~/.bashrc`):
```bash
alias see-start="$HOME/code/see/launch.sh"
alias see-stop="$HOME/code/see/stop.sh"
alias see-status="[ -f $HOME/code/see/.see.pid ] && ps -p \$(cat $HOME/code/see/.see.pid) > /dev/null && echo 'see is running' || echo 'see is not running'"
```

## Usage

- Start tracking: `see-start`
- Check status: `see-status`
- Stop tracking: `see-stop`

## Data

Two CSV files are maintained:

1. `cumulative_data.csv`: Total activity since first run
2. `past_24_hours_data.csv`: Minute-by-minute activity for the last 24 hours

## Requirements

- macOS (tested on Sonoma 14.0+)
- gcc
- libcurl

## Privacy

All data is stored locally. Only hourly snapshots are pushed to GitHub if configured.

## License

MIT