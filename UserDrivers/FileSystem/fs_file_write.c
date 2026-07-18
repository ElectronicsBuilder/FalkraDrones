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
 * @file    fs_file_write.c
 * @brief   File write command handler implementation
 */

#include "fs_file_write.h"
#include "app_reply.h"
#include "ffs.h"
#include "spi_flash_block_device.hpp"
#include "log.hpp"
#include <string.h>

extern ffs_config_t spi_flash_fs_config;

void fs_cmd_handle_write(const uint8_t* args, uint8_t len) {
    if (!args || len < 2) {
        send_framed_response(RESP_TYPE_ERROR, "Invalid write format");
        return;
    }

    // Initialize FFS if needed
    if (!ffs_mount(&spi_flash_fs_config)) {
        LOG_WARN("FFS mount failed, attempting format...");
        if (ffs_format() != 0 || !ffs_mount(&spi_flash_fs_config)) {
            send_framed_response(RESP_TYPE_ERROR, "FS MOUNT FAILED");
            return;
        }
    }

    // Parse filename (null-terminated string at start of args)
    const char* filename = (const char*)args;
    size_t filename_len = strnlen(filename, len);
    
    if (filename_len == 0 || filename_len >= len) {
        send_framed_response(RESP_TYPE_ERROR, "Invalid filename");
        return;
    }

    // Binary data starts after filename + null terminator
    const uint8_t* binary_data = args + filename_len + 1;
    size_t data_len = len - filename_len - 1;

    if (data_len == 0) {
        send_framed_response(RESP_TYPE_ERROR, "No data to write");
        return;
    }

    LOG_INFO("[FS_WRITE] Writing %zu bytes to file: %s", data_len, filename);

    // Create or overwrite file
    int file_id = ffs_open_or_create_reset(filename, 0);
    if (file_id < 0) {
        send_framed_response(RESP_TYPE_ERROR, "Failed to create file");
        LOG_ERROR("[FS_WRITE] Failed to create file: %s", filename);
        return;
    }

    // Write binary data
    int bytes_written = ffs_write(file_id, binary_data, data_len);
    if (bytes_written != (int)data_len) {
        send_framed_response(RESP_TYPE_ERROR, "Write failed");
        LOG_ERROR("[FS_WRITE] Write failed: wrote %d of %zu bytes", bytes_written, data_len);
        return;
    }

    // Save changes to flash
    ffs_serialize_table();

    char response[64];
    snprintf(response, sizeof(response), "File written: %zu bytes", data_len);
    send_framed_response(RESP_TYPE_SUCCESS, response);
    LOG_INFO("[FS_WRITE] Successfully wrote %zu bytes to %s", data_len, filename);
}