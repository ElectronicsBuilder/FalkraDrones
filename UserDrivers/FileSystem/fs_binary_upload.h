/**
 * @file    fs_binary_upload.h
 * @brief   Binary file upload command handlers interface
 * @details Defines the interface for file upload operations over UART,
 *          supporting reliable file transfer with chunk validation and
 *          progress tracking.
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

#ifndef __FS_BINARY_UPLOAD_H
#define __FS_BINARY_UPLOAD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Upload state structure */
typedef struct {
    bool active;
    char filename[64];
    int file_id;
    uint32_t total_bytes_received;
    uint32_t expected_file_size;
} upload_state_t;

extern upload_state_t upload_state;

/**
 * @brief Handle upload start command
 * @param args Command arguments containing filename and file size
 * @param len Argument length
 */
void fs_cmd_handle_upload_start(const uint8_t* args, uint8_t len);

/**
 * @brief Handle upload chunk command
 * @param args Command arguments containing chunk data
 * @param len Argument length
 */
void fs_cmd_handle_upload_chunk(const uint8_t* args, uint8_t len);

/**
 * @brief Handle raw upload chunk data (from data mode)
 * @param data Raw chunk data
 * @param len Data length
 */
void fs_cmd_handle_upload_chunk_raw(const uint8_t* data, size_t len);

/**
 * @brief Handle upload end command
 * @param args Command arguments (none expected)
 * @param len Argument length
 */
void fs_cmd_handle_upload_end(const uint8_t* args, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __FS_BINARY_UPLOAD_H */