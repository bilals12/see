# see - Personal Computer Activity Monitor

A lightweight tool to track and visualize your computer usage patterns, including keyboard and mouse activity.

## Features
- Tracks keypresses, mouse clicks, and mouse movement
- Converts mouse movement to meters
- Maintains 24-hour rolling history
- Auto-syncs data to GitHub
- Web visualization interface

## Installation

### Prerequisites
- macOS (requires Accessibility permissions)
- gcc compiler
- curl library
- GitHub Personal Access Token (for data sync)

### Setup
1. Clone the repository:
```bash
git clone https://github.com/bilals12/see.git
cd see
```

2. Create environment file:
```bash
echo "GITHUB_TOKEN=your_token_here" > .env
echo "GITHUB_REPO=your_username/your_repo" >> .env
```

3. Compile the program:
```bash
gcc -o see see.c -framework ApplicationServices -pthread -lcurl
```

4. Grant Accessibility permissions:
- Go to System Settings > Privacy & Security > Accessibility
- Add the compiled 'see' executable

## Usage

### Running the Program
```bash
./see
```

### Running in Background
```bash
nohup ./see > output.log 2>&1 &
```

### Process Management
Check if running:
```bash
ps -ef | grep see
```

Stop the program:
```bash
kill $(pgrep see)
```

## Data Files
- `cumulative_data.csv`: Lifetime statistics
- `past_24_hours_data.csv`: Rolling 24-hour data in 10-minute intervals

## Debugging

Using LLDB:
```bash
lldb ./see
run
```

## Web Interface
Open `index.html` in a browser to view the visualization dashboard.

## Contributing
Pull requests are welcome. For major changes, please open an issue first.

## License
[MIT](https://choosealicense.com/licenses/mit/)