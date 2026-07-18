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
 * @file    app_reply.c
 * @brief   Application reply implementation
 */
#include "uart.hpp"
#include "string.h"
#include "app_reply.h"
#include "log.hpp"


#define STX 0x7E
#define ETX 0x7F



void send_framed_response(AppResponseType type, const char* msg)
{
    size_t len = strlen(msg);
    if (len > 250) len = 250;

    uint8_t buf[260] = {0};
    buf[0] = STX;
    buf[1] = (uint8_t)(1 + len + 1); // TYPE + DATA + CRC
    buf[2] = type;
    memcpy(&buf[3], msg, len);
    buf[3 + len] = app_crc8(&buf[2], 1 + len);  // CRC over TYPE+DATA
    buf[4 + len] = ETX;

    data_transport_write(buf, 5 + len);
    data_transport_flush();

    // Removed excessive hex dump logging - floods logs and interferes with GUI
    // Debug logging can be re-enabled by uncommenting this block if needed for troubleshooting
    #if 0
    {
        char hexbuf[128];
        size_t h = 0;
        size_t max_print = (5 + len < 24) ? (5 + len) : 24;
        for (size_t i = 0; i < max_print && h + 3 < sizeof(hexbuf); ++i) {
            int n = snprintf(&hexbuf[h], sizeof(hexbuf) - h, "%02X", buf[i]);
            if (n < 0) break;
            h += (size_t)n;
            if (i + 1 < max_print) {
                if (h + 1 < sizeof(hexbuf)) hexbuf[h++] = ' ';
            }
        }
        if (h < sizeof(hexbuf)) hexbuf[h] = '\0'; else hexbuf[sizeof(hexbuf)-1] = '\0';
        LOG_INFO("[APP_REPLY] Sent framed response (type=%d, len=%d) bytes: %s",
                (int)type, (int)(5 + len), hexbuf);
    }
    #endif
}

void app_reply_send(uint8_t code)
{
    uint8_t frame[5];

    frame[0] = STX;
    frame[1] = 0x02;          // LEN (1 byte code + 1 byte CRC)
    frame[2] = code;
    frame[3] = app_crc8(&frame[2], 1);  // CRC over reply code only
    frame[4] = ETX;

    data_transport_write(frame, sizeof(frame));
    data_transport_flush();
}

void app_reply_send_chunk_ack(uint8_t code, uint32_t offset)
{
    uint8_t frame[9];

    frame[0] = STX;
    frame[1] = 1 /*code*/ + 4 /*offset*/ + 1 /*crc*/;
    frame[2] = code;
    frame[3] = (offset)       & 0xFF;
    frame[4] = (offset >> 8)  & 0xFF;
    frame[5] = (offset >> 16) & 0xFF;
    frame[6] = (offset >> 24) & 0xFF;
    frame[7] = app_crc8(&frame[2], 5);  // CRC over code + offset
    frame[8] = ETX;

    data_transport_write(frame, 9);
    data_transport_flush();
}


void app_reply_send_str(uint8_t responseType, const char* str) {
    if (!str) return;

    size_t dataLen = strlen(str);
    if (dataLen > 250) dataLen = 250;

    uint8_t buffer[256];
    size_t idx = 0;

    buffer[idx++] = STX;
    buffer[idx++] = (uint8_t)(1 + dataLen + 1);  // LEN = TYPE + DATA + CRC
    buffer[idx++] = responseType;
    memcpy(&buffer[idx], str, dataLen); idx += dataLen;

    uint8_t crc = app_crc8(&buffer[2], 1 + dataLen);  // CRC over TYPE+DATA
    buffer[idx++] = crc;
    buffer[idx++] = ETX;

    data_transport_write(buffer, idx);
    data_transport_flush();
}
