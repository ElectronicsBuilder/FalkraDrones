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
 * @file    tcp_server.py
 * @brief   TCP Server for STM32 FalkraDrones Wi-Fi Testing
 */
"""

import socket
import threading
import time
import sys

class TCPServer:
    def __init__(self, host='0.0.0.0', port=8080):
        self.host = host
        self.port = port
        self.server_socket = None
        self.running = False

    def handle_client(self, client_socket, client_address):
        """Handle individual client connections"""
        print(f"[{time.strftime('%H:%M:%S')}] New connection from {client_address}")

        try:
            while True:
                # Receive data from client
                data = client_socket.recv(1024)
                if not data:
                    break

                message = data.decode('utf-8', errors='ignore')
                print(f"[{time.strftime('%H:%M:%S')}] Received: {repr(message)}")

                # Send response back to client
                response = f"Server received: {message}"
                client_socket.send(response.encode('utf-8'))
                print(f"[{time.strftime('%H:%M:%S')}] Sent response: {repr(response)}")

        except Exception as e:
            print(f"[{time.strftime('%H:%M:%S')}] Error handling client {client_address}: {e}")
        finally:
            client_socket.close()
            print(f"[{time.strftime('%H:%M:%S')}] Connection closed: {client_address}")

    def start(self):
        """Start the TCP server"""
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.settimeout(1.0)  # Add timeout to make accept() non-blocking
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(5)
            self.running = True

            print(f"[{time.strftime('%H:%M:%S')}] TCP Server started on {self.host}:{self.port}")
            print(f"[{time.strftime('%H:%M:%S')}] Waiting for STM32 connections...")
            print("Press Ctrl+C to stop the server")

            while self.running:
                try:
                    client_socket, client_address = self.server_socket.accept()

                    # Handle each client in a separate thread
                    client_thread = threading.Thread(
                        target=self.handle_client,
                        args=(client_socket, client_address)
                    )
                    client_thread.daemon = True
                    client_thread.start()

                except socket.timeout:
                    # Timeout allows checking self.running periodically
                    continue
                except socket.error as e:
                    if self.running:
                        print(f"[{time.strftime('%H:%M:%S')}] Socket error: {e}")
                        break

        except KeyboardInterrupt:
            print(f"\n[{time.strftime('%H:%M:%S')}] Received Ctrl+C, shutting down...")
        except Exception as e:
            print(f"[{time.strftime('%H:%M:%S')}] Server error: {e}")
        finally:
            self.stop()

    def stop(self):
        """Stop the TCP server"""
        self.running = False
        if self.server_socket:
            self.server_socket.close()
        print(f"\n[{time.strftime('%H:%M:%S')}] Server stopped")

def main():
    """Main function"""
    # Default settings
    host = '0.0.0.0'  # Listen on all interfaces
    port = 8080

    # Parse command line arguments
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
    if len(sys.argv) > 2:
        host = sys.argv[2]

    # Create and start server
    server = TCPServer(host, port)

    try:
        server.start()
    except KeyboardInterrupt:
        print(f"\n[{time.strftime('%H:%M:%S')}] Received Ctrl+C, shutting down...")
        server.stop()

if __name__ == '__main__':
    main()