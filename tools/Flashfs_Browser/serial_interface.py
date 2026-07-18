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
 * @file    serial_interface.py
 * @brief   Serial Interface for Flashfs Browser
 */
"""
import serial
import threading

class SerialInterface:
    def __init__(self, port, baudrate):
        self.ser = serial.Serial(port, baudrate, timeout=0.3)
        self.lock = threading.Lock()

    def send_bytes(self, data: bytes):
        with self.lock:
            self.ser.write(data)

    def send_raw_command(self, cmd: str):
        with self.lock:
            self.ser.write(cmd.encode())

    def read_lines(self):
        lines = []
        while self.ser.in_waiting:
            line = self.ser.readline().decode(errors='ignore').strip()
            if line:
                lines.append(line)
        return lines

    def read_bytes(self, size=1):
        with self.lock:
            return self.ser.read(size)

    def close(self):
        self.ser.close()
