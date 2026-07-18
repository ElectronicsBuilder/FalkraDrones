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
 * @file    cmd_battery.c
 * @brief   Battery monitor console commands (BATMON_*)
 */

#include "console_internal.h"
#include "BatteryMonitor.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Command Handlers
// ============================================================================

BatteryMonitor* monitor = BatteryMonitor::getInstance();


static bool cmd_batmon_calibrate(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (!monitor) {
        console_send(huart, "\r\n[ERROR] BatteryMonitor instance not found!\r\n");
        return true;
    }

    if (!monitor->isRunning()) {
        console_send(huart, "\r\n[ERROR] BatteryMonitor not running! Start DMA first.\r\n");
        return true;
    }

    const char* warn =
        "\r\n========================================\r\n"
        "STARTING CALIBRATION\r\n"
        "========================================\r\n"
        "CRITICAL: Ensure NO current flowing!\r\n"
        "Disconnect all loads from battery.\r\n"
        "Calibration starts in 3 seconds...\r\n";
    console_send(huart, warn);
    HAL_Delay(3000);

    if (monitor->calibrate(100)) {
        const userconfig_batmon_t* cal = monitor->getCalibration();
        char msg[512];
        snprintf(msg, sizeof(msg),
                "\r\n[SUCCESS] Calibration complete!\r\n"
                "  VREF: %.4fV\r\n"
                "  Zero (raw): %.4fV\r\n"
                "  Zero (buffered): %.4fV\r\n"
                "  Sensitivity: %.3fV/A\r\n"
                "Calibration saved to NVRAM.\r\n",
                cal->vref, cal->zero_current_raw, cal->zero_current_buffered, cal->sensitivity_raw);
        console_send(huart, msg);
    } else {
        console_send(huart, "\r\n[ERROR] Calibration failed!\r\n");
    }

    return true;
}

static bool cmd_batmon_status(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (!monitor) {
        console_send(huart, "\r\n[ERROR] BatteryMonitor instance not found!\r\n");
        return true;
    }

    if (!monitor->isRunning()) {
        console_send(huart, "\r\n[INFO] BatteryMonitor not running - attempting to start...\r\n");
        if (!monitor->start()) {
            console_send(huart, "\r\n[ERROR] Failed to start BatteryMonitor!\r\n");
            return true;
        }
    }

    char msg[1024];
    int offset = 0;
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "BATTERY MONITOR STATUS\r\n"
            "========================================\r\n"
            "Running: %s\r\n"
            "Calibrated: %s\r\n",
            monitor->isRunning() ? "YES" : "NO",
            monitor->isCalibrated() ? "YES" : "NO");

    if (monitor->isCalibrated()) {
        const userconfig_batmon_t* cal = monitor->getCalibration();
        BatteryMonitorData data = monitor->getData();

        offset += snprintf(msg + offset, sizeof(msg) - offset,
                "VREF: %.4fV\r\n"
                "Zero (raw): %.4fV\r\n"
                "Zero (buffered): %.4fV\r\n"
                "Sensitivity: %.3fV/A\r\n"
                "Polarity: %s\r\n"
                "Channels: Raw=%s, Buffered=%s\r\n"
                "Calculation uses: %s\r\n"
                "----------------------------------------\r\n",
                cal->vref, cal->zero_current_raw, cal->zero_current_buffered, cal->sensitivity_raw,
                cal->invert_polarity ? "INVERTED (GND/return)" : "NORMAL (VCC+)",
                monitor->isRawChannelEnabled() ? "ENABLED" : "DISABLED",
                monitor->isBufferedChannelEnabled() ? "ENABLED" : "DISABLED",
                (monitor->isRawChannelEnabled() && monitor->isBufferedChannelEnabled()) ? "BOTH channels" :
                monitor->isRawChannelEnabled() ? "RAW channel only" :
                monitor->isBufferedChannelEnabled() ? "BUFFERED channel only" : "NO channels (ERROR)");
        float v_raw = data.voltage_raw;
        float v_buf = data.voltage_buffered;
        float v_avg = (v_raw + v_buf) / 2.0f;
        float delta_v = v_avg - cal->zero_current_raw;
        BatteryMonitorData status_data = monitor->getData();
        BatteryMonitorStats status_stats = monitor->getStats();

        offset += snprintf(msg + offset, sizeof(msg) - offset,
                "Current Readings:\r\n"
                "  ADC Raw:      %4d (%.4fV) -> %.3fA\r\n"
                "  ADC Buffered: %4d (%.4fV) -> %.3fA\r\n"
                "  Average:           %.4fV -> %.3fA\r\n"
                "  Voltage delta: %.4fV from zero (%.4fV)\r\n"
                "----------------------------------------\r\n"
                "Statistics:\r\n"
                "  Min: %.3fA\r\n"
                "  Max: %.3fA\r\n"
                "  Avg: %.3fA\r\n"
                "  Samples: %lu\r\n"
                "  DMA callbacks: %lu\r\n",
                status_data.adc_raw, v_raw, status_data.current_raw,
                status_data.adc_buffered, v_buf, status_data.current_buffered,
                v_avg, status_data.current_average,
                delta_v, cal->zero_current_raw,
                status_stats.current_min, status_stats.current_max, status_stats.current_avg,
                status_stats.sample_count, status_stats.dma_complete_count);
    }

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "========================================\r\n");

    console_send(huart, msg);
    return true;
}

static bool cmd_batmon_reset(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (!monitor) {
        console_send(huart, "\r\n[ERROR] BatteryMonitor instance not found!\r\n");
        return true;
    }

    if (monitor->eraseCalibration()) {
        console_send(huart, "\r\n[INFO] Calibration reset. Driver will return 0.0A until re-calibrated.\r\n");
    } else {
        console_send(huart, "\r\n[ERROR] Failed to reset calibration!\r\n");
    }
    return true;
}

static bool cmd_batmon_set_polarity(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (!monitor) {
        console_send(huart, "\r\n[ERROR] BatteryMonitor instance not found!\r\n");
        return true;
    }

    if (!monitor->isCalibrated()) {
        console_send(huart, "\r\n[ERROR] Must calibrate first! Use BATMON_CALIBRATE\r\n");
        return true;
    }

    const char* arg = console_get_arg(cmd_buffer, "BATMON_SET_POLARITY");
    if (!arg) {
        const char* usage =
            "\r\n[ERROR] Usage: BATMON_SET_POLARITY <0|1>\r\n"
            "  0 = NORMAL (sensor on VCC+ path)\r\n"
            "  1 = INVERTED (sensor on GND/return path)\r\n";
        console_send(huart, usage);
        return true;
    }

    uint8_t invert = (uint8_t)atoi(arg);
    const userconfig_batmon_t* cal = monitor->getCalibration();


    extern userconfig_t* g_userConfig;
    if (!g_userConfig) {
        console_send(huart, "[BATMON] UserConfig not available");
        return false;
    }

    if (cal && userconfig_set_batmon_calibration(
        g_userConfig,
        cal->vref,
        cal->zero_current_raw,
        cal->zero_current_buffered,
        cal->sensitivity_raw,
        cal->sensitivity_buffered,
        invert)) {

        char msg[256];
        snprintf(msg, sizeof(msg),
                "\r\n[INFO] Polarity set to: %s\r\n"
                "[INFO] Current readings will now be %s\r\n"
                "[INFO] Restart monitoring task or wait for next sample\r\n",
                invert ? "INVERTED (sensor on GND/return)" : "NORMAL (sensor on VCC+)",
                invert ? "POSITIVE (absolute value)" : "NORMAL (can be positive/negative)");
        console_send(huart, msg);
    } else {
        console_send(huart, "\r\n[ERROR] Failed to set polarity!\r\n");
    }

    return true;
}

static bool cmd_batmon_set_vref(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (!monitor) {
        console_send(huart, "\r\n[ERROR] BatteryMonitor instance not found!\r\n");
        return true;
    }

    const char* arg = console_get_arg(cmd_buffer, "BATMON_SET_VREF");
    if (!arg) {
        const char* err = "\r\n[ERROR] Missing voltage value\r\n"
                         "[INFO] Usage: BATMON_SET_VREF <voltage>\r\n"
                         "[INFO] Example: BATMON_SET_VREF 3.28\r\n";
        console_send(huart, err);
        return true;
    }

    // Copy argument to clean buffer for atof
    char vref_str[16];
    memset(vref_str, 0, sizeof(vref_str));
    strncpy(vref_str, arg, sizeof(vref_str) - 1);

    double vref = atof(vref_str);

    if (vref < 2.5f || vref > 3.6f) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                "\r\n[ERROR] Invalid voltage value: %.4fV\r\n"
                "[INFO] Usage: BATMON_SET_VREF <voltage>\r\n"
                "[INFO] Example: BATMON_SET_VREF 3.28\r\n"
                "[INFO] Valid range: 2.5V - 3.6V\r\n",
                 vref);
        console_send(huart, msg);
    } else if (monitor->setVref(vref)) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                "\r\n[SUCCESS] VREF updated to %.4fV\r\n"
                "[INFO] Saved to NVRAM. Current readings will now use this reference.\r\n",
                vref);
        console_send(huart, msg);
    } else {
        console_send(huart, "\r\n[ERROR] Failed to set VREF. Check logs for details.\r\n");
    }

    return true;
}

static bool cmd_batmon_enable_raw(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (!monitor) {
        console_send(huart, "\r\n[ERROR] BatteryMonitor instance not found!\r\n");
        return true;
    }

    const char* arg = console_get_arg(cmd_buffer, "BATMON_ENABLE_RAW");
    if (!arg) {
        console_send(huart, "\r\n[ERROR] Usage: BATMON_ENABLE_RAW <0|1>\r\n");
        return true;
    }

    int enable = atoi(arg);
    monitor->setRawChannelEnabled(enable);

    char msg[128];
    snprintf(msg, sizeof(msg),
            "\r\n[INFO] Raw channel %s and saved to NVRAM\r\n"
            "[INFO] Current calculation will use: %s\r\n",
            enable ? "ENABLED" : "DISABLED",
            (monitor->isRawChannelEnabled() && monitor->isBufferedChannelEnabled()) ? "BOTH channels" :
            monitor->isRawChannelEnabled() ? "RAW channel only" :
            monitor->isBufferedChannelEnabled() ? "BUFFERED channel only" : "NO channels (ERROR)");
    console_send(huart, msg);

    return true;
}

static bool cmd_batmon_enable_buffered(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (!monitor) {
        console_send(huart, "\r\n[ERROR] BatteryMonitor instance not found!\r\n");
        return true;
    }

    const char* arg = console_get_arg(cmd_buffer, "BATMON_ENABLE_BUFFERED");
    if (!arg) {
        console_send(huart, "\r\n[ERROR] Usage: BATMON_ENABLE_BUFFERED <0|1>\r\n");
        return true;
    }

    int enable = atoi(arg);
    monitor->setBufferedChannelEnabled(enable);

    char msg[128];
    snprintf(msg, sizeof(msg),
            "\r\n[INFO] Buffered channel %s and saved to NVRAM\r\n"
            "[INFO] Current calculation will use: %s\r\n",
            enable ? "ENABLED" : "DISABLED",
            (monitor->isRawChannelEnabled() && monitor->isBufferedChannelEnabled()) ? "BOTH channels" :
            monitor->isRawChannelEnabled() ? "RAW channel only" :
            monitor->isBufferedChannelEnabled() ? "BUFFERED channel only" : "NO channels (ERROR)");
    console_send(huart, msg);

    return true;
}

// ============================================================================
// Command Registration
// ============================================================================

const console_command_t battery_commands[] = {
    {
        .command = "BATMON_CALIBRATE",
        .handler = cmd_batmon_calibrate,
        .help_text = "Calibrate battery current sensor (ensure 0A current!)"
    },
    {
        .command = "BATMON_STATUS",
        .handler = cmd_batmon_status,
        .help_text = "Show battery monitor status and current readings"
    },
    {
        .command = "BATMON_RESET",
        .handler = cmd_batmon_reset,
        .help_text = "Reset battery monitor calibration"
    },
    {
        .command = "BATMON_SET_POLARITY",
        .handler = cmd_batmon_set_polarity,
        .help_text = "Set polarity (0=VCC+, 1=GND/return)"
    },
    {
        .command = "BATMON_SET_VREF",
        .handler = cmd_batmon_set_vref,
        .help_text = "Set ADC reference voltage (e.g., 3.28)"
    },
    {
        .command = "BATMON_ENABLE_RAW",
        .handler = cmd_batmon_enable_raw,
        .help_text = "Enable/disable raw channel in calculation"
    },
    {
        .command = "BATMON_ENABLE_BUFFERED",
        .handler = cmd_batmon_enable_buffered,
        .help_text = "Enable/disable buffered channel in calculation"
    }
};

const size_t battery_commands_count = sizeof(battery_commands) / sizeof(battery_commands[0]);
