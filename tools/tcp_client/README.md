# TCP Client for STM32 FalkraDrones

Python TCP client to test communication with the STM32 TCP server.

## Usage

### Basic Usage
```bash
cd tools/tcp_client
python tcp_client.py
```

### Specify STM32 IP Address
```bash
python tcp_client.py 192.168.1.250
```

### Run Test Mode
```bash
python tcp_client.py 192.168.1.250 8080 test
```

## Modes

### Interactive Mode (default)
- Type messages and press Enter to send to STM32
- Type `quit` or `exit` to disconnect
- Real-time communication with STM32 server

### Test Mode
- Automatically sends predefined test messages
- Includes various message types:
  - Simple text
  - JSON data
  - Multi-line messages
  - Long messages
  - Special characters
  - Unicode characters

## Finding STM32 IP Address

1. Check STM32 logs - IP is printed when connecting to Wi-Fi
2. Use ARP table: `arp -a | findstr "40-82-7b"` (STM32 MAC)
3. Scan network: `nmap -p 8080 192.168.1.1-254`

## Examples

```bash
# Interactive mode with default IP
python tcp_client.py

# Connect to specific STM32 IP
python tcp_client.py 192.168.1.200

# Run automated tests
python tcp_client.py 192.168.1.200 8080 test

# Help
python tcp_client.py --help
```

## Expected Behavior

1. Client connects to STM32 TCP server
2. STM32 sends welcome message: "Hello from STM32 TCP Server!"
3. Client sends messages
4. STM32 echoes back: "Echo from STM32: [your message]"