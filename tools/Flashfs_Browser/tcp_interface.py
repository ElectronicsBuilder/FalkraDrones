#!/usr/bin/env python3
"""
TCP Interface wrapper to match SerialInterface API used by Flashfs_Browser
"""
import socket
import threading

class SocketWrapper:
    def __init__(self, sock: socket.socket):
        self._sock = sock
        self._sock.settimeout(0.5)

    def read(self, size=1):
        try:
            return self._sock.recv(size)
        except socket.timeout:
            return b''

    def write(self, data: bytes):
        return self._sock.send(data)

    def flush(self):
        return None

    def close(self):
        try:
            self._sock.shutdown(socket.SHUT_RDWR)
        except Exception:
            pass
        self._sock.close()

class TCPInterface:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.lock = threading.Lock()
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.settimeout(5.0)
        self.socket.connect((host, port))
        # Provide `ser` attribute similar to SerialInterface
        self.ser = SocketWrapper(self.socket)

    def send_bytes(self, data: bytes):
        with self.lock:
            return self.ser.write(data)

    def send_raw_command(self, cmd: str):
        with self.lock:
            return self.ser.write(cmd.encode())

    def read_bytes(self, size=1):
        with self.lock:
            return self.ser.read(size)

    def close(self):
        try:
            self.ser.close()
        except Exception:
            pass
