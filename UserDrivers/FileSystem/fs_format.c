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
 * @file    fs_format.c
 * @brief   Filesystem format command handler implementation
 */

#include "fs_format.h"
#include "app_reply.h"
#include "ffs.h"
#include "spi_flash_block_device.hpp"
#include "ffs_block_alloc.h"
#include "log.hpp"

extern ffs_config_t spi_flash_fs_config;

void fs_cmd_handle_format(const uint8_t* args, uint8_t len) {
    LOG_WARN("[FS_FORMAT] Format command received - this will erase all files!");

    /* ffs_format() now preserves erase counts internally and persists both
       tables itself — the old RAM save/restore dance is no longer needed. */
    int result = ffs_format();

    if (result == 0) {
        // Try to remount after format
        if (ffs_mount(&spi_flash_fs_config)) {
            send_framed_response(RESP_TYPE_SUCCESS, "Filesystem formatted successfully");
            LOG_INFO("[FS_FORMAT] Format completed successfully (wear data preserved)");
        } else {
            send_framed_response(RESP_TYPE_WARN, "Format completed but remount failed");
            LOG_WARN("[FS_FORMAT] Format completed but remount failed");
        }
    } else {
        send_framed_response(RESP_TYPE_ERROR, "FORMAT FAILED");
        LOG_ERROR("[FS_FORMAT] Format failed (%d)", result);
    }
}


