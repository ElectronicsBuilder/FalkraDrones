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
 * @file    cmd_time.c
 * @brief   RTC time management console commands
 */

#include "console_internal.h"
#include "test_RTC.hpp"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Command Handlers
// ============================================================================

static bool cmd_get_time(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* timeStr = rtc_get_time_str();
    console_printf(huart, "\r\n[RTC] Time: %s\r\n", timeStr);
    return true;
}

static bool cmd_set_time(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* arg = console_get_arg(cmd_buffer, "SETTIME");
    if (!arg) {
        console_send(huart, "\r\n[ERROR] Missing time value\r\n");
        console_send(huart, "[INFO] Usage: SETTIME:YYYY-MM-DDTHH:MM:SS\r\n");
        console_send(huart, "[INFO] Example: SETTIME:2025-12-24T14:30:00\r\n");
        return true;
    }

    int y, M, d, H, m, s;
    if (sscanf(arg, "%d-%d-%dT%d:%d:%d", &y, &M, &d, &H, &m, &s) == 6) {
        rtc_set_date(1, M, d, y - 2000);
        rtc_set_time(H, m, s);
        console_send(huart, "\r\n[RTC] Time updated.\r\n");
    } else {
        console_send(huart, "\r\n[RTC] Invalid format. Use YYYY-MM-DDTHH:MM:SS\r\n");
    }

    return true;
}

// ============================================================================
// Command Registration
// ============================================================================

const console_command_t time_commands[] = {
    {
        .command = "GETTIME",
        .handler = cmd_get_time,
        .help_text = "Get current RTC time"
    },
    {
        .command = "SETTIME",
        .handler = cmd_set_time,
        .help_text = "Set RTC time (YYYY-MM-DDTHH:MM:SS)"
    }
};

const size_t time_commands_count = sizeof(time_commands) / sizeof(time_commands[0]);
