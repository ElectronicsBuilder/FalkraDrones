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
 * @file    cmd_filesystem.c
 * @brief   Filesystem and flash storage console commands
 */

#include "console_internal.h"
#include "ffs.h"
#include "ffs_config.h"
#include "ffs_block_alloc.h"
#include "spi_flash_block_device.hpp"
#include "log.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ffs_config_t spi_flash_fs_config;
extern ffs_config_t ffs_config;

// Helper function declarations (from console.h)
extern void uart_print_current_log(void);
extern void uart_list_log_files(void);
extern void uart_print_log_file(const char* filename);

static bool require_ffs_mounted(UART_HandleTypeDef* huart, const char* command) {
    if (ffs_is_mounted()) {
        return true;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n[ERROR] %s requires a mounted filesystem\r\n", command);
    console_send(huart, msg);
    return false;
}

// ============================================================================
// Command Handlers - Log Files
// ============================================================================

static bool cmd_showlog(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    uart_print_current_log();
    return true;
}

static bool cmd_listlogs(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    uart_list_log_files();
    return true;
}

static bool cmd_readlog(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* arg = console_get_arg(cmd_buffer, "READLOG");
    if (!arg) {
        console_send(huart, "\r\n[ERROR] Usage: READLOG:filename.log\r\n");
        return true;
    }

    uart_print_log_file(arg);
    return true;
}

static bool cmd_formatlogdisk(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] FORMATTING LOG AREA...\r\n");

    // Delete all .log files
    char log_files[SPI_FLASH_MAX_FILES][SPI_FLASH_MAX_FILE_NAME];
    int log_count = log_list_files(log_files, SPI_FLASH_MAX_FILES);
    for (int i = 0; i < log_count; i++) {
        ffs_delete(log_files[i]);
    }

    console_send(huart, "\r\n[INFO] LOG FORMAT DONE - All log files deleted\r\n");
    return true;
}

// ============================================================================
// Command Handlers - Flash Filesystem
// ============================================================================

static bool cmd_formatdisk(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    //LOG_WARN("[FS_FORMAT] Format command received - this will erase all files!");
    console_send(huart, "\r\n[INFO] FORMATTING FILESYSTEM (this can take a minute)...\r\n");

    /* ffs_format() now preserves erase counts internally and persists both
       tables itself — the old RAM save/restore dance is no longer needed. */
    int result = ffs_format();

    if (result != 0) {
        // LOG_ERROR("[FS_FORMAT] Format failed (%d)", result);
        console_send(huart, "\r\n[ERROR] FORMAT FAILED\r\n");
        return false;
    }

    // Try to remount after format
    if (ffs_mount(&spi_flash_fs_config)) {
     //   LOG_INFO("[FS_FORMAT] Format completed successfully (wear data preserved)");
        console_send(huart, "\r\n[INFO] FORMAT DONE\r\n");
    } else {
    //    LOG_WARN("[FS_FORMAT] Format completed but remount failed");
        console_send(huart, "\r\n[WARN] Format done but remount failed\r\n");
    }
    return true;
}


static bool cmd_dumpblocks(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (!require_ffs_mounted(huart, "DUMPBLOCKS")) {
        return true;
    }

    console_send(huart, "\r\n[INFO] FlashFs Block Status:\r\n");
    ffs_dump_blocks();
    return true;
}

static bool cmd_getblock(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* arg = console_get_arg(cmd_buffer, "GETBLOCK");
    if (!arg) {
        console_send(huart, "\r\n[ERROR] Usage: GETBLOCK:<block_number>\r\n");
        console_send(huart, "Example: GETBLOCK:0\r\n");
        return true;
    }
    if (!require_ffs_mounted(huart, "GETBLOCK")) {
        return true;
    }

    // Parse block number
    char* endptr;
    long block_num = strtol(arg, &endptr, 10);

    if (endptr == arg || block_num < 0) {
        console_send(huart, "\r\n[ERROR] Invalid block number\r\n");
        console_send(huart, "Example: GETBLOCK:0\r\n");
    } else {
        ffs_get_block_info((uint32_t)block_num);
    }

    return true;
}

static bool cmd_reset_erase_counts(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    LOG_WARN("[ERASE_COUNTS] Resetting all erase counts to zero - WARNING: wear leveling history will be lost!");

    if (ffs_reset_erase_counts()) {
        LOG_INFO("[ERASE_COUNTS] Successfully reset all erase counts to zero");
    } else {
        LOG_ERROR("[ERASE_COUNTS] Failed to reset erase counts");
    }

    return true;
}

static bool cmd_debugmount(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Testing filesystem mount status...\r\n");

    bool mount_result = ffs_is_mounted();

    char debug_msg[128];
    snprintf(debug_msg, sizeof(debug_msg), "[DEBUG] Current mount state: %s\r\n",
            mount_result ? "MOUNTED" : "NOT MOUNTED");
    console_send(huart, debug_msg);

    return true;
}


// Helper function to draw a progress bar
static void draw_progress_bar(char* buffer, int size, uint32_t used, uint32_t total) {
    if (total == 0) return;
    int filled = (int)((used * 20) / total);
    if (filled > 20) filled = 20;
    buffer[0] = '[';
    int i = 1;
    for (int j = 0; j < filled; j++) {
        buffer[i++] = '■';
    }
    for (int j = filled; j < 20; j++) {
        buffer[i++] = ' ';
    }
    buffer[i++] = ']';
    buffer[i] = '\0';
}




static bool cmd_listfiles(const char* cmd_buffer, UART_HandleTypeDef* huart)
{
    console_send(huart, "\r\n========================================\r\n");
    console_send(huart, "FILE LISTING\r\n");
    console_send(huart, "========================================\r\n");

    if (!require_ffs_mounted(huart, "LISTFILES")) {
        return true;
    }

    ffs_file_info_t file_list[FFS_MAX_FILES];
    int file_count = ffs_list_files(file_list, FFS_MAX_FILES);

    if (file_count < 0) {
        console_send(huart, "[ERROR] Failed to list files\r\n");
        return true;
    }

    if (file_count == 0) {
        console_send(huart, "No files found\r\n");
        console_send(huart, "========================================\r\n");
        return true;
    }

    // Calculate total file size and flash capacity
    uint32_t file_size[FFS_MAX_FILES];
    uint32_t total_file_size = 0;
    const uint32_t flash_capacity = 16777216;  // 16MB SPI flash

    for (int i = 0; i < file_count; i++) {
        // For .log files, recalculate size using ffs_open_log to get proper recovery
        uint32_t display_size = file_list[i].size;
        bool is_log_file = (strstr(file_list[i].name, ".log") != NULL);

        if (is_log_file && display_size == 0) {
            // Try to get actual size by opening the log file
            int temp_id = ffs_open_log(file_list[i].name);
            if (temp_id >= 0) {
                file_size[i] = file_table[temp_id].size;
            }
        } else {
            file_size[i] = display_size;
        }
        total_file_size += file_size[i];
    }

    // Display overall flash usage and file listing
    char msg[512];
    uint32_t flash_percent = (total_file_size * 100) / flash_capacity;
    char flash_bar[24];
    draw_progress_bar(flash_bar, sizeof(flash_bar), total_file_size, flash_capacity);

    snprintf(msg, sizeof(msg),
            "Flash Usage:  %s %3lu%% (%lu MB / 16 MB)\r\n"
            "\r\n"
            "Files: %d\r\n"
            "----------------------------------------\r\n"
            "Filename                      Size\r\n"
            "----------------------------------------\r\n",
            flash_bar,
            flash_percent,
            total_file_size / (1024 * 1024),
            file_count);
    console_send(huart, msg);

    for (int i = 0; i < file_count; i++) {
        snprintf(msg, sizeof(msg),
                "%-28s  %8lu B\r\n",
                file_list[i].name,
                file_size[i]);
        console_send(huart, msg);
    }

    console_send(huart, "========================================\r\n");
    return true;
}

static bool cmd_readfile(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* filename = console_get_arg(cmd_buffer, "READFILE");
    if (!filename) {
        console_send(huart, "\r\n[ERROR] Usage: READFILE:filename\r\n");
        return true;
    }

    char header[128];
    snprintf(header, sizeof(header), "\r\n[INFO] Reading file: %s\r\n", filename);
    console_send(huart, header);

    if (!require_ffs_mounted(huart, "READFILE")) {
        return true;
    }

    int file_id = ffs_open(filename);
    if (file_id < 0) {
        snprintf(header, sizeof(header), "[ERROR] Failed to open file (error %d)\r\n", file_id);
        console_send(huart, header);
        return true;
    }
    if (ffs_seek(file_id, 0, FFS_SEEK_SET) != 0) {
        console_send(huart, "[ERROR] Failed to seek file to start\r\n");
        return true;
    }

    char buffer[256];
    uint32_t total_read = 0;
    while (1) {
        int bytes_read = ffs_read(file_id, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) break;

        buffer[bytes_read] = '\0';
        console_send(huart, buffer);
        total_read += bytes_read;

        if (bytes_read < sizeof(buffer) - 1) break;
    }

    snprintf(header, sizeof(header), "\r\n[INFO] Read complete (%lu bytes)\r\n", total_read);
    console_send(huart, header);

    return true;
}

static bool cmd_deletefile(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* filename = console_get_arg(cmd_buffer, "DELETEFILE");
    if (!filename) {
        console_send(huart, "\r\n[ERROR] Usage: DELETEFILE:filename\r\n");
        return true;
    }

    char header[128];
    snprintf(header, sizeof(header), "\r\n[INFO] Deleting file: %s\r\n", filename);
    console_send(huart, header);

    if (!require_ffs_mounted(huart, "DELETEFILE")) {
        return true;
    }

    int result = ffs_delete(filename);
    if (result == 0) {
        console_send(huart, "[INFO] File deleted successfully\r\n");
    } else {
        snprintf(header, sizeof(header), "[ERROR] Failed to delete file (error %d)\r\n", result);
        console_send(huart, header);
    }

    return true;
}

// ============================================================================
// Command Registration
// ============================================================================

const console_command_t filesystem_commands[] = {
    { .command = "SHOWLOG", .handler = cmd_showlog, .help_text = "Display current log file" },
    { .command = "LISTLOGS", .handler = cmd_listlogs, .help_text = "List all log files" },
    { .command = "READLOG", .handler = cmd_readlog, .help_text = "Read specified log file" },
    { .command = "FORMATLOGDISK", .handler = cmd_formatlogdisk, .help_text = "Format log storage area" },
    { .command = "FORMATDISK", .handler = cmd_formatdisk, .help_text = "Format entire filesystem" },
    { .command = "DUMPBLOCKS", .handler = cmd_dumpblocks, .help_text = "Dump flash block status" },
    { .command = "GETBLOCK", .handler = cmd_getblock, .help_text = "Get detailed block info with chain" },
    { .command = "RESET_ERASE_COUNTS", .handler = cmd_reset_erase_counts, .help_text = "Reset all block erase counts to zero" },
    { .command = "DEBUGMOUNT", .handler = cmd_debugmount, .help_text = "Test filesystem mount status" },
    { .command = "LISTFILES", .handler = cmd_listfiles, .help_text = "List all files in filesystem" },
    { .command = "READFILE", .handler = cmd_readfile, .help_text = "Read specified file" },
    { .command = "DELETEFILE", .handler = cmd_deletefile, .help_text = "Delete specified file" }
};

const size_t filesystem_commands_count = sizeof(filesystem_commands) / sizeof(filesystem_commands[0]);
