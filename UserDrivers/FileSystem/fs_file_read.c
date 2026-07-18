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
 * @file    fs_file_read.c
 * @brief   File read command handler implementation
 */

#include "fs_file_read.h"
#include "fs_file_config.h"
#include "app_reply.h"
#include "ffs.h"
#include "spi_flash_block_device.hpp"
#include "log.hpp"
#include "stm32f7xx_hal.h"
#include <string.h>

extern ffs_config_t spi_flash_fs_config;

void fs_cmd_handle_read(const uint8_t *args, uint8_t len) {
    if (!args || len == 0 || args[0] == 0) {
        app_reply_send_str(RESP_TYPE_ERROR, "Missing filename");
        return;
    }

    char filename[64] = {0};
    size_t safe_len = (len < sizeof(filename) - 1) ? len : (sizeof(filename) - 1);
    memcpy(filename, args, safe_len);
    filename[safe_len] = '\0';

    // Initialize FFS if needed
    if (!ffs_mount(&spi_flash_fs_config)) {
        LOG_WARN("FFS mount failed, attempting format...");
        if (ffs_format() != 0 || !ffs_mount(&spi_flash_fs_config)) {
            app_reply_send_str(RESP_TYPE_ERROR, "FS MOUNT FAILED");
            return;
        }
    }

    // Detect file type based on extension - treat .log and .txt as text files
    bool is_text_file = (strstr(filename, ".log") != NULL || strstr(filename, ".txt") != NULL);
    int file_id;
    
    if (is_text_file) {
        // Use appropriate open function based on file type
        bool is_log = (strstr(filename, ".log") != NULL);
        if (is_log) {
            // Use ffs_open_log for .log files (handles recovery and proper positioning)
            file_id = ffs_open_log(filename);
        } else {
            // Use regular ffs_open for .txt files
            file_id = ffs_open(filename);
        }
        
        if (file_id < 0) {
            char error_msg[96];
            snprintf(error_msg, sizeof(error_msg), "TEXT FILE OPEN FAILED: %s", filename);
            app_reply_send_str(RESP_TYPE_ERROR, error_msg);
            return;
        }
        
        // Read text file line by line
        ffs_seek(file_id, 0, FFS_SEEK_SET);
        char line_buf[256];
        int line_count = 0;
        
        while (ffs_read_line(file_id, line_buf, sizeof(line_buf)) > 0) {
            app_reply_send_str(RESP_TYPE_INFO, line_buf);
            line_count++;
            
            // Check configurable line limit for text files
            if (FS_TEXT_FILE_READ_LIMIT_LINES > 0 && line_count >= FS_TEXT_FILE_READ_LIMIT_LINES) {
                char limit_msg[96];
                snprintf(limit_msg, sizeof(limit_msg), "[TRUNCATED] Output limited to %d lines...", FS_TEXT_FILE_READ_LIMIT_LINES);
                app_reply_send_str(RESP_TYPE_INFO, limit_msg);
                break;
            }
        }
        
        if (line_count == 0) {
            app_reply_send_str(RESP_TYPE_INFO, "[EMPTY] Text file is empty");
        }
        
        LOG_INFO("[FS_READ] Sent %d lines from text file '%s' to GUI", line_count, filename);
        
    } else {
        // Use ffs_open for binary files
        file_id = ffs_open(filename);
        if (file_id < 0) {
            char error_msg[96];
            snprintf(error_msg, sizeof(error_msg), "BINARY FILE OPEN FAILED: %s", filename);
            app_reply_send_str(RESP_TYPE_ERROR, error_msg);
            return;
        }
        
        // Read binary file as hex dump for GUI display
        // Ensure cursor is at beginning
        int seek_result = ffs_seek(file_id, 0, FFS_SEEK_SET);
        if (seek_result != 0) {
            char seek_error[64];
            snprintf(seek_error, sizeof(seek_error), "[ERROR] Seek to start failed: %d", seek_result);
            app_reply_send_str(RESP_TYPE_ERROR, seek_error);
            return;
        }
        
        uint8_t binary_buf[FS_BINARY_HEX_DUMP_BUFFER_SIZE];
        char hex_line[128];
        int bytes_read;
        int total_bytes = 0;
        uint32_t offset = 0;
        
        
        while ((bytes_read = ffs_read(file_id, binary_buf, sizeof(binary_buf))) > 0) {
            // Process the buffer in 16-byte chunks for hex dump formatting
            for (int chunk_start = 0; chunk_start < bytes_read; chunk_start += 16) {
                int chunk_size = (chunk_start + 16 <= bytes_read) ? 16 : (bytes_read - chunk_start);
                uint32_t line_offset = offset + chunk_start;
                
                // Format as hex dump: "0000: 01 02 03 04 05 06 07 08  09 0A 0B 0C 0D 0E 0F 10"
                int pos = snprintf(hex_line, sizeof(hex_line), "%04X: ", line_offset);
                
                for (int i = 0; i < chunk_size; i++) {
                    pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "%02X ", binary_buf[chunk_start + i]);
                    if (i == 7 && chunk_size > 8) {
                        pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, " ");
                    }
                }
                
                // Pad the hex part if less than 16 bytes
                while (pos < 50) {
                    hex_line[pos++] = ' ';
                }
                
                // Add ASCII representation
                pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, " |");
                for (int i = 0; i < chunk_size; i++) {
                    char c = (binary_buf[chunk_start + i] >= 32 && binary_buf[chunk_start + i] <= 126) ? binary_buf[chunk_start + i] : '.';
                    pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "%c", c);
                }
                pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "|");
                
                app_reply_send_str(RESP_TYPE_INFO, hex_line);
            }
            
            total_bytes += bytes_read;
            offset += bytes_read;
            
            // Minimal delay to prevent UART buffer overflow
            if ((offset % 256) == 0 && offset > 0) {
                HAL_Delay(1);  // 5ms delay every 256 bytes
            }
            
            // Check configurable byte limit for binary files
            if (FS_BINARY_FILE_READ_LIMIT_BYTES > 0 && offset >= FS_BINARY_FILE_READ_LIMIT_BYTES) {
                char limit_msg[96];
                snprintf(limit_msg, sizeof(limit_msg), "[TRUNCATED] Binary output limited to %d bytes...", FS_BINARY_FILE_READ_LIMIT_BYTES);
                app_reply_send_str(RESP_TYPE_INFO, limit_msg);
                break;
            }
        }
        
        
        if (total_bytes == 0) {
            app_reply_send_str(RESP_TYPE_INFO, "[EMPTY] Binary file is empty");
        } else {
            char summary[96];
            snprintf(summary, sizeof(summary), "[SUMMARY] Binary file: %d bytes displayed", total_bytes);
            app_reply_send_str(RESP_TYPE_INFO, summary);
        }
        
        LOG_INFO("[FS_READ] Sent %d bytes from binary file '%s' to GUI as hex dump", total_bytes, filename);
    }
}