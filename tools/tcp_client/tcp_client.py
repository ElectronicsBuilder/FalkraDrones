#!/usr/bin/env python3
"""
/**
 * MIT License
 * 
 * Copyright (c) 2025 ElectronicsBuilder
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * @file    tcp_client.py
 * @brief   TCP Client for STM32 FalkraDrones TCP Server Testing
 */
"""

import socket
import time
import sys
import threading

class TCPClient:
    def __init__(self, host='192.168.1.250', port=8081):
        self.host = host
        self.port = port
        self.socket = None
        self.connected = False

    def connect(self):
        """Connect to the STM32 TCP server"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10.0)  # 10 second timeout for slower STM32 processing

            print(f"[{time.strftime('%H:%M:%S')}] Connecting to STM32 server at {self.host}:{self.port}...")
            self.socket.connect((self.host, self.port))
            self.connected = True

            print(f"[{time.strftime('%H:%M:%S')}] Connected successfully!")

            # Receive welcome message if any
            try:
                welcome = self.socket.recv(1024).decode('utf-8', errors='ignore')
                if welcome:
                    print(f"[{time.strftime('%H:%M:%S')}] Server welcome: {repr(welcome)}")
            except socket.timeout:
                pass  # No welcome message

            return True

        except Exception as e:
            print(f"[{time.strftime('%H:%M:%S')}] Connection failed: {e}")
            if self.socket:
                self.socket.close()
                self.socket = None
            return False

    def send_message(self, message):
        """Send a message to the STM32 server"""
        if not self.connected or not self.socket:
            print("Error: Not connected to server")
            return False

        try:
            # Send message
            self.socket.send(message.encode('utf-8'))
            print(f"[{time.strftime('%H:%M:%S')}] Sent: {repr(message)}")

            # Wait for response
            response = self.socket.recv(1024).decode('utf-8', errors='ignore')
            print(f"[{time.strftime('%H:%M:%S')}] Received: {repr(response)}")

            return True

        except Exception as e:
            print(f"[{time.strftime('%H:%M:%S')}] Send failed: {e}")
            return False

    def interactive_mode(self):
        """Interactive mode - send messages from user input"""
        print("\n" + "="*50)
        print("Interactive mode - type messages to send to STM32")
        print("Type 'quit' or 'exit' to stop")
        print("="*50)

        while self.connected:
            try:
                message = input("\n> ").strip()
                if message.lower() in ['quit', 'exit', 'q']:
                    break
                if message:
                    if not self.send_message(message):
                        break
            except KeyboardInterrupt:
                print("\nReceived Ctrl+C, disconnecting...")
                break
            except EOFError:
                break

    def test_mode(self, messages=None):
        """Test mode - send predefined test messages"""
        if messages is None:
            messages = [
                "Hello STM32!",
                "Test message 1",
                "JSON: {'sensor': 'temp', 'value': 25.3}",
                "Multi-line\ntest message\nwith newlines",
                "Long message: " + "A" * 100,
                "Special chars: !@#$%^&*()_+-=[]{}|;':\",./<>?",
                "Unicode test: 🚁🛸🎮",
                "Goodbye STM32!"
            ]

        print(f"\n[{time.strftime('%H:%M:%S')}] Starting test mode with {len(messages)} messages...")

        for i, message in enumerate(messages, 1):
            print(f"\n--- Test {i}/{len(messages)} ---")
            if not self.send_message(message):
                print("Test failed, stopping...")
                break
            time.sleep(1)  # 1 second between messages

        print(f"\n[{time.strftime('%H:%M:%S')}] Test mode completed!")

    def disconnect(self):
        """Disconnect from the server"""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
        self.connected = False
        print(f"[{time.strftime('%H:%M:%S')}] Disconnected from server")

def main():
    """Main function"""
    # Default settings
    host = '192.168.1.250'  # Default STM32 IP - UPDATE THIS
    port = 8080
    mode = 'interactive'

    # Parse command line arguments
    if len(sys.argv) > 1:
        host = sys.argv[1]
    if len(sys.argv) > 2:
        port = int(sys.argv[2])
    if len(sys.argv) > 3:
        mode = sys.argv[3]

    print("STM32 TCP Client")
    print(f"Target: {host}:{port}")
    print(f"Mode: {mode}")
    print("-" * 30)

    # Create client
    client = TCPClient(host, port)

    try:
        # Connect to server
        if not client.connect():
            print("Failed to connect to STM32 server")
            return 1

        # Run in specified mode
        if mode == 'test':
            client.test_mode()
        else:
            client.interactive_mode()

    except KeyboardInterrupt:
        print(f"\n[{time.strftime('%H:%M:%S')}] Received Ctrl+C")
    except Exception as e:
        print(f"[{time.strftime('%H:%M:%S')}] Error: {e}")
    finally:
        client.disconnect()

    return 0

def print_usage():
    """Print usage information"""
    print("Usage: python tcp_client.py [host] [port] [mode]")
    print("")
    print("Arguments:")
    print("  host    STM32 IP address (default: 192.168.1.150)")
    print("  port    STM32 TCP server port (default: 8080)")
    print("  mode    'interactive' or 'test' (default: interactive)")
    print("")
    print("Examples:")
    print("  python tcp_client.py                    # Interactive mode with defaults")
    print("  python tcp_client.py 192.168.1.100     # Connect to specific IP")
    print("  python tcp_client.py 192.168.1.100 8080 test  # Run test mode")
    print("")
    print("Interactive Mode:")
    print("  - Type messages and press Enter to send")
    print("  - Type 'quit' or 'exit' to disconnect")
    print("")
    print("Test Mode:")
    print("  - Sends predefined test messages automatically")
    print("  - 1 second delay between messages")

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] in ['-h', '--help', 'help']:
        print_usage()
        sys.exit(0)

    sys.exit(main())