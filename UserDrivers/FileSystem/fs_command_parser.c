/**
 * @file    fs_command_parser.c
 * @brief   Filesystem Command Parser Implementation
 * @details Implements the command parser for filesystem operations,
 *          handling command packets, CRC validation, and routing
 *          to appropriate command handlers.
 * 
 * Part of FalkraController - STM32F767-based drone controller firmware
 * 
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
 */

#include "fs_command_parser.h"
#include "transport_uart.h"
#include "app_defs.hpp"         
#include "app_reply.h"
#include "log.hpp"
#include "ffs.h"


#include "fs_file_listing.h"
#include "fs_file_read.h"
#include "fs_file_write.h"
#include "fs_file_delete.h"
#include "fs_format.h"
#include "fs_time_sync.h"
#include "fs_binary_upload.h"
#include "fs_exit.h"

typedef enum {
    FS_CMD_WAIT_STX,
    FS_CMD_LEN,
    FS_CMD_PAYLOAD,
    FS_CMD_WAIT_ETX
} fs_cmd_state_t;


typedef enum {
    FS_CMD_LIST   = 0x10,
    FS_CMD_READ   = 0x11,
    FS_CMD_WRITE  = 0x12,
    FS_CMD_DELETE = 0x13,
    FS_CMD_FORMAT = 0x14,
    FS_CMD_SYNC_TIME = 0x15,  // New command for time synchronization
    FS_CMD_UPLOAD_START = 0x16,  // Start binary file upload
    FS_CMD_UPLOAD_CHUNK = 0x17,  // Upload file chunk
    FS_CMD_UPLOAD_END = 0x18,    // End binary file upload
    FS_CMD_EXIT = 0x19,          // Exit filesystem mode

    FS_LOG_CMD_LIST   = 0x20,
    FS_LOG_CMD_READ   = 0x21,
    FS_LOG_CMD_DELETE = 0x22,
    FS_LOG_CMD_FORMAT = 0x23
} FsCommandCode;






static fs_cmd_state_t state = FS_CMD_WAIT_STX;
static uint8_t buffer[4096];
static uint8_t idx = 0;
static uint8_t expected_len = 0;



void fs_process_packet_byte(uint8_t byte) {
    // Removed excessive per-byte logging - only log packet-level events (STX, complete packets)

    switch (state) {
        case FS_CMD_WAIT_STX:
            if (byte == APP_CMD_STX) {
                LOG_INFO("[FS_CMD] STX received, starting packet");
                idx = 0;
                state = FS_CMD_LEN;
            }
            break;

        case FS_CMD_LEN:
            expected_len = byte;
            if (expected_len < 2 || expected_len >= sizeof(buffer)) {
                state = FS_CMD_WAIT_STX;
                break;
            }
            buffer[idx++] = byte;
            state = FS_CMD_PAYLOAD;
            break;

        case FS_CMD_PAYLOAD:
            buffer[idx++] = byte;
            if (idx >= expected_len + 1) {
                state = FS_CMD_WAIT_ETX;
            }
            break;

        case FS_CMD_WAIT_ETX:
            if (byte != APP_CMD_ETX) {
                LOG_WARN("[FS_CMD] Invalid ETX");
                state = FS_CMD_WAIT_STX;
                return;
            }

            uint8_t received_crc = buffer[expected_len];
            uint8_t calc_crc = app_crc8(&buffer[1], expected_len - 1);
            if (received_crc != calc_crc) {
                send_framed_response(RESP_TYPE_ERROR, "CRC_FAIL");
            } else {
                uint8_t cmd = buffer[1];
                const uint8_t *args = &buffer[2];
                uint8_t arg_len = expected_len - 2;
                switch (cmd) {
                    case FS_CMD_LIST:
                    case FS_LOG_CMD_LIST:
                        fs_cmd_handle_list(args, arg_len);
                        break;
                        
                    case FS_CMD_READ:
                    case FS_LOG_CMD_READ:
                        fs_cmd_handle_read(args, arg_len);
                        break;
                        
                    case FS_CMD_WRITE:
                        fs_cmd_handle_write(args, arg_len);
                        break;
                        
                    case FS_CMD_DELETE:
                    case FS_LOG_CMD_DELETE:
                        fs_cmd_handle_delete(args, arg_len);
                        break;
                        
                    case FS_CMD_FORMAT:
                    case FS_LOG_CMD_FORMAT:
                        fs_cmd_handle_format(args, arg_len);
                        break;
                        
                    case FS_CMD_SYNC_TIME:
                        fs_cmd_handle_sync_time(args, arg_len);
                        break;
                        
                    case FS_CMD_UPLOAD_START:
                        LOG_INFO("[FS_CMD] Received FS_CMD_UPLOAD_START, arg_len=%d", arg_len);
                        send_framed_response(RESP_TYPE_INFO, "DEBUG: Command parser received upload start");
                        fs_cmd_handle_upload_start(args, arg_len);
                        break;
                        
                    case FS_CMD_UPLOAD_CHUNK:
                        fs_cmd_handle_upload_chunk(args, arg_len);
                        break;
                        
                    case FS_CMD_UPLOAD_END:
                        fs_cmd_handle_upload_end(args, arg_len);
                        break;

                    case FS_CMD_EXIT:
                        fs_cmd_handle_exit(args, arg_len);
                        break;

                    default:
                        send_framed_response(RESP_TYPE_ERROR, "UNKNOWN_CMD");
                        break;
                }
            }
            state = FS_CMD_WAIT_STX;
            break;
    }
}
