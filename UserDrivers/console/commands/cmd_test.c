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
 * @file    cmd_test.c
 * @brief   Hardware test console commands
 */

#include "console_internal.h"
#include "test_spi.hpp"
#include "test_ppm.hpp"
#include "test_batteryMonitor.hpp"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Command Handlers
// ============================================================================

static bool cmd_spi_test(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Running SPI4 hardware test...\r\n");

    SpiTestResult_t result = test_spi_basic();
    if (result == SPI_TEST_PASSED) {
        console_send(huart, "[INFO] SPI4 test PASSED! Hardware is working correctly.\r\n");
    } else {
        console_send(huart, "[ERROR] SPI4 test FAILED! Check hardware configuration.\r\n");
    }

    return true;
}

static bool cmd_test_ppm(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    uint32_t timeout_ms = 5000; // Default 5 seconds

    const char* arg = console_get_arg(cmd_buffer, "TEST_PPM");
    if (arg) {
        int parsed_timeout = atoi(arg);
        if (parsed_timeout > 0 && parsed_timeout <= 300000) { // Max 5 minutes
            timeout_ms = (uint32_t)parsed_timeout;
        } else {
            console_send(huart, "\r\n[ERROR] Invalid timeout. Use 1-300000 ms (max 5 minutes)\r\n");
            timeout_ms = 5000; // Fallback to default
        }
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n[INFO] Starting PPM signal test (timeout: %lums)...\r\n", timeout_ms);
    console_send(huart, msg);

    test_ppm(timeout_ms);
    return true;
}

static bool cmd_test_batmon(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    uint32_t duration_ms = 10000; // Default 10 seconds

    const char* arg = console_get_arg(cmd_buffer, "TEST_BATMON");
    if (arg) {
        int parsed_duration = atoi(arg);
        if (parsed_duration > 0 && parsed_duration <= 300000) { // Max 5 minutes
            duration_ms = (uint32_t)parsed_duration;
        } else {
            console_send(huart, "\r\n[ERROR] Invalid duration. Use 1-300000 ms (max 5 minutes)\r\n");
            duration_ms = 10000; // Fallback to default
        }
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n[INFO] Starting Battery Monitor test (duration: %lums)...\r\n", duration_ms);
    console_send(huart, msg);

    test_batteryMonitor(duration_ms);
    return true;
}

// ============================================================================
// Command Registration
// ============================================================================

const console_command_t test_commands[] = {
    {
        .command = "spiTest",
        .handler = cmd_spi_test,
        .help_text = "Run SPI4 hardware test"
    },
    {
        .command = "TEST_PPM",
        .handler = cmd_test_ppm,
        .help_text = "Test PPM decoder (timeout in ms, default: 5000)"
    },
    {
        .command = "TEST_BATMON",
        .handler = cmd_test_batmon,
        .help_text = "Test battery current monitor (duration in ms, default: 10000)"
    }
};

const size_t test_commands_count = sizeof(test_commands) / sizeof(test_commands[0]);
