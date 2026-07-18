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
 * @file    parser.py
 * @brief   Parser for Flashfs Browser
 */
"""
APP_CMD_STX = 0x7E
APP_CMD_ETX = 0x7F

def crc8(data: bytes) -> int:
    crc = 0x00
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc << 1) ^ 0x31 if (crc & 0x80) else (crc << 1)
            crc &= 0xFF
    return crc

def build_framed_command(cmd_id: int, args: bytes = b'') -> bytes:
    payload = bytes([cmd_id]) + args
    length = len(payload) + 1  # include CRC
    crc = crc8(payload)
    return bytes([APP_CMD_STX, length]) + payload + bytes([crc, APP_CMD_ETX])

def extract_framed_responses(data: bytes) -> list[str]:
    responses = []
    idx = 0
    while idx < len(data):
        if data[idx] == APP_CMD_STX and (idx + 2) < len(data):
            length = data[idx + 1]
            end_idx = idx + 2 + length  # STX + LEN + (TYPE + DATA + CRC)
            if end_idx >= len(data):
                break  # Incomplete frame
            if data[end_idx] != APP_CMD_ETX:
                idx += 1
                continue

            payload = data[idx + 2 : end_idx - 1]  # TYPE + DATA
            crc_recv = data[end_idx - 1]
            crc_calc = crc8(payload)

            if crc_recv != crc_calc:
                idx += 1
                continue

            # Extract string after type
            if len(payload) >= 1:
                str_data = payload[1:].decode(errors='ignore')
                responses.append(str_data)

            idx = end_idx + 1  # Move past ETX
        else:
            idx += 1
    return responses