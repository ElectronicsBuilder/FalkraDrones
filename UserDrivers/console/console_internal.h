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
 * @file    console_internal.h
 * @brief   Internal console command handler types and declarations
 */

#ifndef __CONSOLE_INTERNAL_H
#define __CONSOLE_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f7xx_hal.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Command Handler Types
// ============================================================================

/**
 * @brief Command handler function pointer
 * @param cmd_buffer Full command buffer (e.g., "BATMON_STATUS")
 * @param huart UART handle for output
 * @return true if command was handled, false otherwise
 */
typedef bool (*console_cmd_handler_t)(const char* cmd_buffer, UART_HandleTypeDef* huart);

/**
 * @brief Command handler registration structure
 */
typedef struct {
    const char* command;                // Command string (e.g., "BATMON_STATUS")
    console_cmd_handler_t handler;      // Handler function
    const char* help_text;              // Help text for this command
} console_command_t;

// ============================================================================
// Utility Functions for Command Handlers
// ============================================================================

/**
 * @brief Send string to UART (helper for command handlers)
 * @param huart UART handle
 * @param str String to send
 */
static inline void console_send(UART_HandleTypeDef* huart, const char* str) {
    if (str && huart) {
        HAL_UART_Transmit(huart, (const uint8_t*)str, strlen(str), HAL_MAX_DELAY);
    }
}

/**
 * @brief Send formatted string to UART
 * @param huart UART handle
 * @param fmt Format string (printf-style)
 * @param ... Arguments
 */
void console_printf(UART_HandleTypeDef* huart, const char* fmt, ...);

/**
 * @brief Extract argument after command name
 * @param cmd_buffer Full command buffer
 * @param command Command name
 * @return Pointer to argument string (after space), or NULL if no argument
 */
const char* console_get_arg(const char* cmd_buffer, const char* command);

// ============================================================================
// Command Module Declarations
// ============================================================================

// Each module provides an array of commands and a count
extern const console_command_t battery_commands[];
extern const size_t battery_commands_count;

extern const console_command_t system_commands[];
extern const size_t system_commands_count;

extern const console_command_t test_commands[];
extern const size_t test_commands_count;

extern const console_command_t filesystem_commands[];
extern const size_t filesystem_commands_count;

extern const console_command_t wifi_commands[];
extern const size_t wifi_commands_count;

extern const console_command_t time_commands[];
extern const size_t time_commands_count;

extern const console_command_t network_commands[];
extern const size_t network_commands_count;

extern const console_command_t nvram_commands[];
extern const size_t nvram_commands_count;

extern const console_command_t environmental_commands[];
extern const size_t environmental_commands_count;

extern const console_command_t memory_commands[];
extern const size_t memory_commands_count;

extern const console_command_t motor_commands[];
extern const size_t motor_commands_count;

extern const console_command_t tof_commands[];
extern const size_t tof_commands_count;

#ifdef __cplusplus
}
#endif

#endif /* __CONSOLE_INTERNAL_H */
