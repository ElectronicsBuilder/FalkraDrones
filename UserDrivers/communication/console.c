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
 * @file    console.c
 * @brief   Modular console command dispatcher
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "console.h"
#include "console_internal.h"
#include "stm32f7xx_hal.h"
#include "ffs.h"
#include "log.hpp"

extern UART_HandleTypeDef huart1;
const char* prompt = "\r\n\033[36m[Falkra >>] \033[0m";

// Command buffer
static char log_buffer[128];
static size_t log_pos = 0;

// External file table for log file reading (from FlashFS)
extern ffs_file_entry_t file_table[];

// ============================================================================
// Command Registration Table
// ============================================================================

// Aggregate all command modules into a single registration table
typedef struct {
    const console_command_t* commands;
    const size_t* count;
} command_module_t;

static const command_module_t command_modules[] = {
    { battery_commands, &battery_commands_count },
    { system_commands, &system_commands_count },
    { test_commands, &test_commands_count },
    { filesystem_commands, &filesystem_commands_count },
    { wifi_commands, &wifi_commands_count },
    { time_commands, &time_commands_count },
    { network_commands, &network_commands_count },
    { nvram_commands, &nvram_commands_count },
    { environmental_commands, &environmental_commands_count },
    { memory_commands, &memory_commands_count },
    { motor_commands, &motor_commands_count },
    { tof_commands, &tof_commands_count },
    { NULL, NULL }  // Sentinel
};

static const size_t command_modules_count = sizeof(command_modules) / sizeof(command_modules[0]) - 1;

// ============================================================================
// Command Dispatcher
// ============================================================================

/**
 * @brief Dispatch a command to the appropriate handler
 * @param cmd_buffer Command buffer containing the command string
 * @param huart UART handle for output
 * @return true if command was handled, false otherwise
 */
static bool console_dispatch_command(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (!cmd_buffer || !huart) return false;

    // Iterate through all command modules
    for (size_t i = 0; i < command_modules_count; i++) {
        const command_module_t* module = &command_modules[i];
        size_t cmd_count = *(module->count);

        // Iterate through commands in this module
        for (size_t j = 0; j < cmd_count; j++) {
            const console_command_t* cmd = &module->commands[j];
            size_t cmd_len = strlen(cmd->command);

            // Check if command matches
            if (strncasecmp(cmd_buffer, cmd->command, cmd_len) == 0) {
                // Ensure full match (next char must be space, colon, or end of string)
                char next_char = cmd_buffer[cmd_len];
                if (next_char == '\0' || next_char == ' ' || next_char == ':') {
                    return cmd->handler(cmd_buffer, huart);
                }
            }
        }
    }

    return false;  // Command not found
}

// ============================================================================
// Main Console Input Handler
// ============================================================================

void uart_handle_CONSOLE(uint8_t byte) {
    switch (byte) {
        case '\r':
        case '\n':
            log_buffer[log_pos] = '\0';
            if (log_pos > 0) {
                // Dispatch command to modular handlers
                if (!console_dispatch_command(log_buffer, &huart1)) {
                    // Command not recognized
                    const char* error_msg = "\r\n[ERROR] Unknown command. Type HELP for available commands.\r\n";
                    HAL_UART_Transmit(&huart1, (uint8_t*)error_msg, strlen(error_msg), HAL_MAX_DELAY);
                }


            }
                           // Print prompt for next command
            HAL_UART_Transmit(&huart1, (uint8_t*)prompt, strlen(prompt), HAL_MAX_DELAY);
       
            log_pos = 0;
            memset(log_buffer, 0, sizeof(log_buffer));
            break;

        case '\b':  // Backspace
        case 127:   // Delete
            if (log_pos > 0) {
                log_pos--;
                log_buffer[log_pos] = '\0';
                const char* backspace_seq = "\b \b";  // Move back, space, move back
                HAL_UART_Transmit(&huart1, (uint8_t*)backspace_seq, 3, HAL_MAX_DELAY);
            }
            break;

        default:
            // Echo printable characters
            if (byte >= 32 && byte < 127 && log_pos < sizeof(log_buffer) - 1) {
                log_buffer[log_pos++] = byte;
                HAL_UART_Transmit(&huart1, &byte, 1, HAL_MAX_DELAY);
            }
            break;
    }
}

// ============================================================================
// LOG FILE DISPLAY FUNCTIONS (used by cmd_filesystem.c)
// ============================================================================

void uart_print_current_log(void) {
    const char* current_filename = log_get_current_filename();
    if (!current_filename || strlen(current_filename) == 0) {
        const char* err = "\r\n[ERROR] No current log file available\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t*)err, strlen(err), HAL_MAX_DELAY);
        return;
    }

    char header[128];
    snprintf(header, sizeof(header), "\r\n=== CURRENT LOG FILE: %s ===\r\n", current_filename);
    HAL_UART_Transmit(&huart1, (uint8_t*)header, strlen(header), HAL_MAX_DELAY);

    uart_print_log_file(current_filename);
}


void uart_list_log_files(void) {
    char log_files[20][32];
    int log_count = log_list_files(log_files, 20);

    const char* header = "\r\n========================================\r\n=== LOG FILES ===\r\n========================================\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)header, strlen(header), HAL_MAX_DELAY);

    if (log_count == 0) {
        const char* no_files = "No log files found\r\n========================================\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t*)no_files, strlen(no_files), HAL_MAX_DELAY);
        return;
    }

    extern ffs_file_entry_t file_table[];
    uint32_t total_size = 0;

    // Calculate total size
    for (int i = 0; i < log_count; i++) {
        int file_id = ffs_open_log(log_files[i]);
        if (file_id >= 0) {
            total_size += file_table[file_id].size;
        }
    }

    char info_line[128];
    snprintf(info_line, sizeof(info_line), "Total: %d logs | Size: %lu bytes\r\n----------------------------------------\r\n",
             log_count, total_size);
    HAL_UART_Transmit(&huart1, (uint8_t*)info_line, strlen(info_line), HAL_MAX_DELAY);

    // Display with sizes
    for (int i = 0; i < log_count; i++) {
        uint32_t file_size = 0;
        int file_id = ffs_open_log(log_files[i]);
        if (file_id >= 0) {
            file_size = file_table[file_id].size;
        }

        uint32_t percent = (total_size > 0) ? (file_size * 100) / total_size : 0;
        char file_info[96];
        snprintf(file_info, sizeof(file_info), "%d. %-24s %8lu B  %3lu%%\r\n",
                 i + 1, log_files[i], file_size, percent);
        HAL_UART_Transmit(&huart1, (uint8_t*)file_info, strlen(file_info), HAL_MAX_DELAY);
    }

    const char* usage = "----------------------------------------\r\nUsage: READLOG:filename.log\r\n========================================\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)usage, strlen(usage), HAL_MAX_DELAY);
}
void uart_print_log_file(const char* filename) {
    if (!filename) {
        const char* err = "\r\n[ERROR] Invalid filename\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t*)err, strlen(err), HAL_MAX_DELAY);
        return;
    }

    // Open the log file for reading
    int file_id = ffs_open(filename);
    if (file_id < 0) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "\r\n[ERROR] Cannot open log file: %s\r\n", filename);
        HAL_UART_Transmit(&huart1, (uint8_t*)err_msg, strlen(err_msg), HAL_MAX_DELAY);
        return;
    }

    // Print file header with size info
    uint32_t file_size = file_table[file_id].size;
    char file_header[128];
    snprintf(file_header, sizeof(file_header), "\r\n=== LOG FILE: %s (%lu bytes) ===\r\n",
             filename, (unsigned long)file_size);
    HAL_UART_Transmit(&huart1, (uint8_t*)file_header, strlen(file_header), HAL_MAX_DELAY);

    ffs_seek(file_id, 0, FFS_SEEK_SET);

    char line_buffer[256];
    int line_count = 0;

    while (ffs_read_line(file_id, line_buffer, sizeof(line_buffer)) > 0) {
        char numbered_line[300];
        snprintf(numbered_line, sizeof(numbered_line), "%04d: %s\r\n", ++line_count, line_buffer);
        HAL_UART_Transmit(&huart1, (uint8_t*)numbered_line, strlen(numbered_line), HAL_MAX_DELAY);

        if (line_count >= 1000) {
            const char* truncated = "\r\n[INFO] Output truncated at 1000 lines...\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t*)truncated, strlen(truncated), HAL_MAX_DELAY);
            break;
        }
    }

    char footer[64];
    snprintf(footer, sizeof(footer), "\r\n=== END OF LOG (%d lines) ===\r\n", line_count);
    HAL_UART_Transmit(&huart1, (uint8_t*)footer, strlen(footer), HAL_MAX_DELAY);
}
