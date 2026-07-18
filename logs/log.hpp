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
 * @file    log.hpp
 * @brief   Logging functions
 */
#ifndef __LOG_HPP
#define __LOG_HPP

#ifdef __cplusplus
extern "C" {
#endif



#include <stdint.h>
#include <stdarg.h> 

/* Undefine any conflicting macros from ST67W6X */
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_WARN
#undef LOG_WARN
#endif
#ifdef LOG_ERROR
#undef LOG_ERROR
#endif
#ifdef LOG_NONE
#undef LOG_NONE
#endif

typedef enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_SYSSTATUS,
    LOG_WARN,
    LOG_ERROR,
    LOG_NONE
} LogLevel;

void log_set_level(LogLevel level);
void log_debug(const char* format, ...);
void log_info(const char* format, ...);
void log_sysstatus(const char* format, ...);
void log_warn(const char* format, ...);
void log_error(const char* format, ...);
void log_cmd(const char* format, ...);

// Internal functions with file/line support
void log_warn_impl(const char* file, int line, const char* format, ...);
void log_error_impl(const char* file, int line, const char* format, ...);
void log_sysstatus_impl(const char* file, int line, const char* format, ...);


int log_init(void);
void log_deinit(void);
void clear_error(void);

void ffs_init_if_needed(void);

// Enhanced log management functions
const char* log_get_current_filename(void);
uint32_t log_get_current_file_size(void);
int log_list_files(char file_list[][32], int max_files);
void log_force_rotation(void);

#define LOG_DEBUG(...) log_debug(__VA_ARGS__)
#define LOG_INFO(...)  log_info(__VA_ARGS__)
#define LOG_WARN(...)  log_warn_impl(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_SYSSTATUS(...)  log_sysstatus_impl(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_error_impl(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_CMD(...) log_cmd(__VA_ARGS__)
#ifdef __cplusplus
}
#endif

#endif /* __LOG_HPP */