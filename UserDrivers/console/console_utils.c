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
 * @file    console_utils.c
 * @brief   Console utility functions for command handlers
 */

#include "console_internal.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void console_printf(UART_HandleTypeDef* huart, const char* fmt, ...) {
    if (!huart || !fmt) return;

    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
}

const char* console_get_arg(const char* cmd_buffer, const char* command) {
    if (!cmd_buffer || !command) return NULL;

    // Find either space or colon after the command name (COMMAND: or COMMAND <arg>)
    const char* arg = NULL;
    const char* space_pos = strchr(cmd_buffer, ' ');
    const char* colon_pos = strchr(cmd_buffer, ':');

    // Determine which delimiter comes first (or if either exists)
    if (space_pos && colon_pos) {
        arg = (space_pos < colon_pos) ? space_pos : colon_pos;
    } else if (space_pos) {
        arg = space_pos;
    } else if (colon_pos) {
        arg = colon_pos;
    } else {
        return NULL;  // No delimiter found
    }

    // Skip the delimiter (space or colon)
    arg++;

    // Skip any additional leading spaces and colons
    while (*arg == ' ' || *arg == ':') arg++;

    // Return NULL if nothing after delimiters
    if (*arg == '\0') return NULL;

    return arg;
}
