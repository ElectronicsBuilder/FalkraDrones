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
 * @file    fs_file_listing.c
 * @brief   File listing command implementation
 * @details Implementation of the file listing functionality for the proprietary
 *          ElectronicsBuilder filesystem. Handles filesystem mounting, file
 *          enumeration, and special handling for log files with size recovery.
 */

#include "fs_file_listing.h"
#include "app_reply.h"
#include "ffs.h"
#include "spi_flash_block_device.hpp"
#include "log.hpp"
#include <stdio.h>
#include <string.h>

extern ffs_config_t spi_flash_fs_config;

void fs_cmd_handle_list(const uint8_t *args, uint8_t len) {
    // Initialize FFS if needed
    if (!ffs_mount(&spi_flash_fs_config)) {
        LOG_WARN("FFS mount failed, attempting format...");
        if (ffs_format() != 0 || !ffs_mount(&spi_flash_fs_config)) {
            app_reply_send_str(RESP_TYPE_ERROR, "FS MOUNT FAILED");
            return;
        }
    }
    
    // List all files in FFS (no directories supported)
    ffs_file_info_t files[FFS_MAX_FILES];
    int file_count = ffs_list_files(files, FFS_MAX_FILES);
    
    if (file_count == 0) {
        app_reply_send_str(RESP_TYPE_INFO, "[INFO] No files found");
        return;
    }
    
    // Send each file as a response
    char line[128];
    for (int i = 0; i < file_count; i++) {
        // For .log files, recalculate size using ffs_open_log to get proper recovery
        uint32_t display_size = files[i].size;
        bool is_log_file = (strstr(files[i].name, ".log") != NULL);
        
        if (is_log_file && display_size == 0) {
            // Try to get actual size by opening the log file
            int temp_id = ffs_open_log(files[i].name);
            if (temp_id >= 0) {
                display_size = file_table[temp_id].size;
               // LOG_INFO("[FS_LIST] Log file '%s' actual size: %lu bytes", files[i].name, (unsigned long)display_size);
            }
        }
        
        // Format: [FILE] filename (size bytes)
        snprintf(line, sizeof(line), "[FILE] %s (%lu bytes)", 
                 files[i].name, (unsigned long)display_size);
        app_reply_send_str(RESP_TYPE_INFO, line);
    }
    
    LOG_INFO("[FS_LIST] Sent %d file(s) to GUI", file_count);
}