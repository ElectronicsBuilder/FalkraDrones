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
 * @file    log.cpp
 * @brief   Logging functions
 */
#include "log.hpp"
#include "main.h"
#include "rtc.hpp"

extern "C" {
#include "ffs.h"
#include "ffs_block_alloc.h"
#include "spi_flash_block_device.hpp"
}

#include <stdio.h>
#include <string.h>
#include "app_defs.hpp"

#define COLOR_RESET      "\033[0m"
#define COLOR_PURPLE     "\033[35m"
#define LOG_FILENAME_LEN 32
#define LOG_LINE_BUFFER_SIZE 512

static LogLevel current_log_level = LOG_DEBUG;

bool log_initialized = false;

static char current_log_filename[LOG_FILENAME_LEN];
static char current_log_date[16];
static int current_log_file_id = -1;
static void strip_trailing_newlines(char* str);

extern ffs_file_entry_t file_table[FFS_MAX_FILES];


bool ffs_initialized = false;


void ffs_init_if_needed(void) {
    if (!ffs_initialized) {
        ffs_config = spi_flash_fs_config;  
        ffs_initialized = true;
    }
}

static int log_open_daily_file(void) {
    const char* date_str = rtc_get_date_str();
    if (!date_str) return -1;
    
  
    snprintf(current_log_filename, sizeof(current_log_filename), "%s.log", date_str);
    strncpy(current_log_date, date_str, sizeof(current_log_date));
    
    ffs_init_if_needed();

    printf("\n\rStarting filesystem initialization for date: %s\n\r", date_str);


    bool mount_successful = false;
    const int max_retries = 10;

    for (int retry = 0; retry < max_retries && !mount_successful; retry++) {
        if (ffs_mount(&spi_flash_fs_config)) {
            mount_successful = true;
            printf("Filesystem mounted successfully on attempt %d\n\r", retry + 1);
            break;
        }

        if (retry < max_retries - 1) {
            printf("Mount failed (attempt %d/%d), retrying in 10ms...\n\r", retry + 1, max_retries);
            HAL_Delay(1000);
        }
    }


    if (!mount_successful) {
        printf("Mount failed after %d retries, formatting (erase counts preserved by ffs_format)...\n\r", max_retries);

        /* ffs_format() now preserves erase counts internally — the old
           heap-based save/restore dance is no longer needed. */
        if (ffs_format() != 0) {
            printf("Format operation failed\n\r");
            return -1;
        }

        if (!ffs_mount(&spi_flash_fs_config)) {
            printf("Mount failed even after format");
            return -1;
        }

        printf("Format completed and filesystem mounted successfully");
    }

    int file_id = ffs_open_log(current_log_filename);
    if (file_id >= 0) {
        uint32_t file_size = file_table[file_id].size;
        log_info("Opened daily log file '%s' with size %lu bytes", current_log_filename, (unsigned long)file_size);
        return file_id;
    }
    
    printf("Failed to open/create log file '%s'", current_log_filename);
    return -1;
}

int log_init(void) {
    #if APP_LOG_FILE_WRITE_ENABLED
        current_log_file_id = log_open_daily_file();
        if (current_log_file_id >= 0) {
            // Write initialization message
            char init_msg[LOG_LINE_BUFFER_SIZE];
            const char* time_str = rtc_get_time_str();
            snprintf(init_msg, sizeof(init_msg), "[%s] [SYSTEM] Log system initialized - %s", 
                     time_str, current_log_filename);
            ffs_write_line(current_log_file_id, init_msg);
        }
    #endif

    log_initialized = true;
    return (current_log_file_id >= 0) ? 0 : -1;
}

void log_deinit(void) {
    #if APP_LOG_FILE_WRITE_ENABLED
        if (current_log_file_id >= 0) {
            // Write shutdown message
            char shutdown_msg[LOG_LINE_BUFFER_SIZE];
            const char* time_str = rtc_get_time_str();
            snprintf(shutdown_msg, sizeof(shutdown_msg), "[%s] [SYSTEM] Log system shutdown", time_str);
            ffs_write_line(current_log_file_id, shutdown_msg);
            current_log_file_id = -1;
        }
    #endif
    log_initialized = false;
}

static void log_check_date_change(void) {
    #if APP_LOG_FILE_WRITE_ENABLED
        const char* current_date = rtc_get_date_str();
        if (!current_date) return;
        
        // Check if date has changed
        if (strncmp(current_log_date, current_date, sizeof(current_log_date)) != 0) {
            // Close current log file
            if (current_log_file_id >= 0) {
                char date_change_msg[LOG_LINE_BUFFER_SIZE];
                const char* time_str = rtc_get_time_str();
                snprintf(date_change_msg, sizeof(date_change_msg), "[%s] [SYSTEM] Date changed, switching to new log file", time_str);
                ffs_write_line(current_log_file_id, date_change_msg);
            }
            
            // Open new daily log file
            current_log_file_id = log_open_daily_file();
            
            if (current_log_file_id >= 0) {
                char new_file_msg[LOG_LINE_BUFFER_SIZE];
                const char* time_str = rtc_get_time_str();
                snprintf(new_file_msg, sizeof(new_file_msg), "[%s] [SYSTEM] New daily log file created - %s", 
                         time_str, current_log_filename);
                ffs_write_line(current_log_file_id, new_file_msg);
            }
        }
    #endif
}

static void log_write_to_file(const char* level, const char* format, va_list args) {
    #if APP_LOG_FILE_WRITE_ENABLED
        /* Re-entrancy guard: log statements emitted from INSIDE a filesystem
           operation (FFS's own LOG_* calls) must not recurse back into FFS —
           a nested write during a metadata update corrupts the tables.
           Those lines still reach the UART sink, just not the flash file. */
        if (ffs_lock_held_by_current_task())
            return;
        if (current_log_file_id >= 0) {
            // Check if date has changed
            log_check_date_change();
            
            char log_buffer[LOG_LINE_BUFFER_SIZE];
            const char* ts = rtc_get_time_str();
            
            // Format: [timestamp] [level] message
            int prefix_len = snprintf(log_buffer, sizeof(log_buffer), "[%s] [%s] ", ts, level);
            if (prefix_len > 0 && prefix_len < (int)sizeof(log_buffer)) {
                vsnprintf(log_buffer + prefix_len, sizeof(log_buffer) - prefix_len, format, args);
                strip_trailing_newlines(log_buffer); // Strip newlines from complete message
                ffs_write_line(current_log_file_id, log_buffer);
            }
        }
    #endif
}

static void log_write_to_file_with_location(const char* level, const char* file, int line, const char* format, va_list args) {
    #if APP_LOG_FILE_WRITE_ENABLED
        /* Re-entrancy guard — see log_write_to_file() */
        if (ffs_lock_held_by_current_task())
            return;
        if (current_log_file_id >= 0) {
            // Check if date has changed
            log_check_date_change();
            
            char log_buffer[LOG_LINE_BUFFER_SIZE];
            const char* ts = rtc_get_time_str();
            
            // Extract just the filename from the full path
            const char* filename = strrchr(file, '/');
            if (!filename) filename = strrchr(file, '\\');
            if (!filename) filename = file;
            else filename++; // Skip the slash
            
            // Format: [timestamp] [level] filename:line message
            int prefix_len = snprintf(log_buffer, sizeof(log_buffer), "[%s] [%s] %s:%d ", ts, level, filename, line);
            if (prefix_len > 0 && prefix_len < (int)sizeof(log_buffer)) {
                vsnprintf(log_buffer + prefix_len, sizeof(log_buffer) - prefix_len, format, args);
                strip_trailing_newlines(log_buffer); // Strip newlines from complete message
                ffs_write_line(current_log_file_id, log_buffer);
            }
        }
    #endif
}

static void strip_trailing_newlines(char* str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[--len] = '\0';
    }
}

static void log_output(const char* color_code, const char* level, const char* format, va_list args)
{
    const char* ts = rtc_get_time_str();
    printf("\r" COLOR_PURPLE "[%s] " COLOR_RESET "%s[%s] ", ts, color_code, level);
    vprintf(format, args);
    printf(COLOR_RESET "\n");
}

static void log_output_with_location(const char* color_code, const char* level, const char* file, int line, const char* format, va_list args)
{
    const char* ts = rtc_get_time_str();
    
    // Extract just the filename from the full path
    const char* filename = strrchr(file, '/');
    if (!filename) filename = strrchr(file, '\\');
    if (!filename) filename = file;
    else filename++; // Skip the slash
    
    printf("\r" COLOR_PURPLE "[%s] " COLOR_RESET "%s[%s] %s:%d ", ts, color_code, level, filename, line);
    vprintf(format, args);
    printf(COLOR_RESET "\n");
}

static void console(const char* color_code, const char* level, const char* format, va_list args)
{
    const char* ts = rtc_get_time_str();
    printf("\r" COLOR_PURPLE "[%s] " COLOR_RESET "%s[%s] ", ts, color_code, level);
    vprintf(format, args);
    printf(COLOR_RESET "\n");
}

void log_debug(const char* format, ...)
{
    if (current_log_level <= LOG_DEBUG)
    {
        va_list args;
        va_start(args, format);
        log_output("\033[36m", "Falkra >>", format, args);
        va_end(args);
    }
}

void log_info(const char* format, ...)
{
    if (current_log_level <= LOG_INFO)
    {
        va_list args, args_copy;
        va_start(args, format);
        va_copy(args_copy, args);
        log_output("\033[32m", "INFO ", format, args);
      //  log_write_to_file("INFO", format, args_copy);
        va_end(args_copy);
        va_end(args);
    }
}


void log_sysstatus(const char* format, ...)
{
    if (current_log_level <= LOG_SYSSTATUS)
    {
        va_list args, args_copy;
        va_start(args, format);
        va_copy(args_copy, args);
        log_output("\033[36m", "SYSSTATUS", format, args);
        log_write_to_file("SYSSTATUS", format, args_copy);
        va_end(args_copy);
        va_end(args);
    }
}

void log_warn(const char* format, ...)
{
    if (current_log_level <= LOG_WARN)
    {
        va_list args, args_copy;
        va_start(args, format);
        va_copy(args_copy, args);
        log_output("\033[33m", "WARN ", format, args);
        log_write_to_file("WARN", format, args_copy);
        va_end(args_copy);
        va_end(args);
    }
}

void log_error(const char* format, ...)
{
    if (current_log_level <= LOG_ERROR)
    {
        if (HAL_GPIO_ReadPin(LED_ERROR_GPIO_Port, LED_ERROR_Pin) != GPIO_PIN_RESET) {
            HAL_GPIO_WritePin(LED_ERROR_GPIO_Port, LED_ERROR_Pin, GPIO_PIN_RESET);
        }
        va_list args, args_copy;
        va_start(args, format);
        va_copy(args_copy, args);
        log_output("\033[31m", "ERROR", format, args);
        log_write_to_file("ERROR", format, args_copy);
        va_end(args_copy);
        va_end(args);
    }
}

void clear_error(void)
{
        if (HAL_GPIO_ReadPin(LED_ERROR_GPIO_Port, LED_ERROR_Pin) != GPIO_PIN_SET) {
            HAL_GPIO_WritePin(LED_ERROR_GPIO_Port, LED_ERROR_Pin, GPIO_PIN_SET);

        }
        log_warn("Clearing Last Error");
}

void log_set_level(LogLevel level) {
    current_log_level = level;
}

void log_cmd(const char* format, ...)
{
    if (current_log_level <= LOG_DEBUG)
    {
        va_list args;
        va_start(args, format);
        console("\033[36m", "Falkra >>", format, args);
        va_end(args);
    }
}

// =============================================================================
// ENHANCED LOG MANAGEMENT FUNCTIONS
// =============================================================================

const char* log_get_current_filename(void) {
    return current_log_filename;
}

uint32_t log_get_current_file_size(void) {
    #if APP_LOG_FILE_WRITE_ENABLED
        if (current_log_file_id >= 0) {
            return file_table[current_log_file_id].size;
        }
    #endif
    return 0;
}

int log_list_files(char file_list[][32], int max_files) {
    #if APP_LOG_FILE_WRITE_ENABLED
        ffs_file_info_t files[20];  // Max files in FFS
        int file_count = ffs_list_files(files, 20);
        int log_file_count = 0;
        
        for (int i = 0; i < file_count && log_file_count < max_files; i++) {
            // Only include .log files
            if (strstr(files[i].name, ".log") != NULL) {
                strncpy(file_list[log_file_count], files[i].name, 31);
                file_list[log_file_count][31] = '\0';
                log_file_count++;
            }
        }
        return log_file_count;
    #else
        return 0;
    #endif
}

void log_force_rotation(void) {
    #if APP_LOG_FILE_WRITE_ENABLED
        if (current_log_file_id >= 0) {
            // Write rotation message
            char rotate_msg[LOG_LINE_BUFFER_SIZE];
            const char* time_str = rtc_get_time_str();
            snprintf(rotate_msg, sizeof(rotate_msg), "[%s] [SYSTEM] Forced log rotation", time_str);
            ffs_write_line(current_log_file_id, rotate_msg);
            
            // Force date change check to create new file
            memset(current_log_date, 0, sizeof(current_log_date));
            log_check_date_change();
        }
    #endif
}

void log_warn_impl(const char* file, int line, const char* format, ...)
{
    if (current_log_level <= LOG_WARN)
    {
        va_list args, args_copy;
        va_start(args, format);
        va_copy(args_copy, args);
        log_output_with_location("\033[33m", "WARN ", file, line, format, args);
        log_write_to_file_with_location("WARN", file, line, format, args_copy);
        va_end(args_copy);
        va_end(args);
    }
}

void log_error_impl(const char* file, int line, const char* format, ...)
{
    if (current_log_level <= LOG_ERROR)
    {
        if (HAL_GPIO_ReadPin(LED_ERROR_GPIO_Port, LED_ERROR_Pin) != GPIO_PIN_RESET) {
            HAL_GPIO_WritePin(LED_ERROR_GPIO_Port, LED_ERROR_Pin, GPIO_PIN_RESET);
        }
        va_list args, args_copy;
        va_start(args, format);
        va_copy(args_copy, args);
        log_output_with_location("\033[31m", "ERROR", file, line, format, args);
        log_write_to_file_with_location("ERROR", file, line, format, args_copy);
        va_end(args_copy);
        va_end(args);
    }
}



void log_sysstatus_impl(const char* file, int line, const char* format, ...)
{
    if (current_log_level <= LOG_SYSSTATUS)
    {
        va_list args, args_copy;
        va_start(args, format);
        va_copy(args_copy, args);
        log_output_with_location("\033[36m", "SYSSTATUS", file, line, format, args);
        log_write_to_file_with_location("SYSSTATUS", file, line, format, args_copy);
        va_end(args_copy);
        va_end(args);
    }
}

