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
 * @file    app_reply.h
 * @brief   Application reply interface
 */
#ifndef __APP_REPLY_H
#define __APP_REPLY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>



#define APP_REPLY_CRC_FAIL    0x12
#define APP_REPLY_VERIFIED    0x20  


typedef enum {
    RESP_TYPE_INFO    = 0x01,
    RESP_TYPE_WARN    = 0x02,
    RESP_TYPE_ERROR   = 0x03,
    RESP_TYPE_REPLY   = 0x04,
    RESP_TYPE_SUCCESS = 0x05
} AppResponseType;


typedef enum {
    APP_REPLY_CHUNK_OK = 0x00,
    APP_REPLY_CHUNK_ERR = 0x01,
    APP_REPLY_CHUNK_INVALID = 0x02,
    APP_REPLY_CHUNK_DONE = 0x03
} app_reply_chunk_t;

/**
 * @brief Sends a framed UART response with response type and message.
 * 
 * Frame format: [STX][LEN][TYPE][DATA...][CRC][ETX]
 * - STX: 0x7E same as APP_CMD_STX in App_defs
 * - ETX: 0x7F same as APP_CMD_ETX in App_defs    
 * - LEN: bytes after LEN up to ETX (TYPE + DATA + CRC)
 *
 * @param type AppResponseType enum (INFO, WARN, ERROR, REPLY)
 * @param msg  Null-terminated UTF-8 string (max 250 chars used)
 */
void send_framed_response(AppResponseType type, const char* msg);
void app_reply_send(uint8_t code);
void app_reply_send_chunk_ack(uint8_t code, uint32_t offset);

void app_reply_send_str(uint8_t responseType, const char* str) ;

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_REPLY_H */
