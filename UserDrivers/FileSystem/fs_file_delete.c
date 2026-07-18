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
 * @file    fs_file_delete.c
 * @brief   File delete command handler implementation
 */

#include "fs_file_delete.h"
#include "app_reply.h"
#include "ffs.h"
#include "spi_flash_block_device.hpp"
#include "log.hpp"
#include <string.h>

extern ffs_config_t spi_flash_fs_config;

void fs_cmd_handle_delete(const uint8_t* args, uint8_t len) {
    if (!args || len == 0 || args[0] == 0) {
        send_framed_response(RESP_TYPE_ERROR, "Missing filename");
        return;
    }

    char filename[64] = {0};
    size_t safe_len = (len < sizeof(filename) - 1) ? len : (sizeof(filename) - 1);
    memcpy(filename, args, safe_len);
    filename[safe_len] = '\0';

    if (!ffs_mount(&spi_flash_fs_config)) {
        LOG_WARN("FFS mount failed, attempting format...");
        if (ffs_format() != 0 || !ffs_mount(&spi_flash_fs_config)) {
            send_framed_response(RESP_TYPE_ERROR, "FS MOUNT FAILED");
            return;
        }
    }

    LOG_INFO("[FS_DELETE] Deleting file: %s", filename);

    if (!ffs_file_exists(filename)) {
        send_framed_response(RESP_TYPE_ERROR, "File not found");
        LOG_WARN("[FS_DELETE] File not found: %s", filename);
        return;
    }

    int result = ffs_delete(filename);
    if (result == 0) {
        char response[96];
        snprintf(response, sizeof(response), "File deleted: %s", filename);
        send_framed_response(RESP_TYPE_SUCCESS, response);
        LOG_INFO("[FS_DELETE] Successfully deleted: %s", filename);
    } else {
        send_framed_response(RESP_TYPE_ERROR, "Delete failed");
        LOG_ERROR("[FS_DELETE] Failed to delete: %s", filename);
    }
}