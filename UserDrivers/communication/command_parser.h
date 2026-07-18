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
 * @file    command_parser.h
 * @brief   Bootloader Interface
 */

#ifndef __COMMAND_PARSER_H
#define __COMMAND_PARSER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STX 0x7E
#define ETX 0x7F

typedef enum {
    APP_CMD_GET_VERSION             = 0x01,
    APP_CMD_JUMP_TO_APP             = 0x02,
    APP_CMD_FS_LIST                 = 0x10,
    APP_CMD_RESET                   = 0x06,
    APP_CMD_TRANSPORT_SWITCH        = 0x07,
    APP_CMD_TRANSPORT_MODE_SWITCH   = 0x08,
    APP_CMD_JUMP_TO_BOOTLOADER      = 0x09,
    APP_CMD_WIFI_DISCONNECT         = 0x0A,
    APP_CMD_SERIAL_DISCONNECT       = 0x0B
  
} AppCommand;


void process_packet_byte(uint8_t byte);
void reset_command_parser(void);

#ifdef __cplusplus
}
#endif

#endif /* __COMMAND_PARSER_H */
