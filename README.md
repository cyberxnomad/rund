# rund

A lightweight daemonizer and process supervisor.

## Features

- Daemonize any process
- Process respawn with configurable conditions
- Custom working directory
- Environment variable support
- Standard output/error redirection
- Graceful shutdown handling
- Configurable respawn delay and maximum respawn attempts

## Requirements

- C99 compatible compiler
- CMake 3.15.0 or higher
- POSIX compliant operating system

## Building

```bash
# Clone the repository
git clone https://github.com/cyberxnomad/rund.git
cd rund

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
make

# Install the program (optional, requires sudo)
sudo make install
```

## Usage

```bash
rund [options...] <target> [target_args...]
```

### Options

| Short | Long                  | Description                                       |
|-------|-----------------------|---------------------------------------------------|
| `-o`  | `--stdout=FILE`       | Redirect stdout to FILE (default: /dev/null)      |
| `-e`  | `--stderr=FILE`       | Redirect stderr to FILE (default: /dev/null)      |
| `-c`  | `--chdir=DIR`         | Change working directory to DIR                   |
| `-E`  | `--env=NAME=VALUE`    | Set environment variable                          |
|       |                       | Can be used multiple times                        |
| `-p`  | `--pidfile=FILE`      | Write PID to FILE                                 |
| `-r`  | `--respawn`           | Enable auto-respawn on exit                       |
|       | `--respawn-code=CODE` | Respawn only if exit code equals CODE             |
|       |                       | Can be used multiple times                        |
|       |                       | Use -1 for any codes                              |
|       |                       | Default: any non-zero codes (if -r is set)        |
|       | `--respawn-delay=N`   | Wait N seconds before respawning (default: 3)     |
|       | `--max-respawns=N`    | Maximum respawn attempts (default: 0 = unlimited) |
| `-h`  | `--help`              | Display this help message and exit                |
| `-V`  | `--version`           | Show version information and exit                 |

### Examples

1. **Run a program as a daemon:**
   ```bash
   rund /path/to/your/program arg1 arg2
   ```

2. **Redirect stdout and stderr to files:**
   ```bash
   rund -o /var/log/app.log -e /var/log/app.err /path/to/your/program
   ```

3. **Set working directory and environment variables:**
   ```bash
   rund -c /app -E DEBUG=1 -E LOG_LEVEL=info /path/to/your/program
   ```

4. **Enable respawning with custom conditions:**
   ```bash
   rund -r --respawn-code=1 --respawn-delay=5 --max-respawns=10 /path/to/your/program
   ```

## License

This project is licensed under the GPL-3.0 License. See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
