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
 * @file    cmd_wifi.c
 * @brief   WiFi credential management console commands
 */

#include "console_internal.h"
#include "user_config.h"
#include "wifi_bootloader.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

extern userconfig_t* g_userConfig;

// ============================================================================
// Command Handlers
// ============================================================================

static bool cmd_wifi_show(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (g_userConfig && userconfig_has_wifi_credentials(g_userConfig)) {
        char wifi_msg[256];
        snprintf(wifi_msg, sizeof(wifi_msg),
                "\r\n=== Stored WiFi Credentials ===\r\n"
                "SSID: %s\r\n"
                "Auto-connect: %s\r\n"
                "================================\r\n",
                userconfig_get_wifi_ssid(g_userConfig),
                userconfig_get_wifi_autoconnect(g_userConfig) ? "Enabled" : "Disabled");
        console_send(huart, wifi_msg);
    } else {
        console_send(huart, "\r\n[INFO] No WiFi credentials stored in NVRAM\r\n");
    }
    return true;
}

static bool cmd_wifi_set_ssid(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* ssid = console_get_arg(cmd_buffer, "WIFI_SET_SSID");
    if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 32) {
        console_send(huart, "\r\n[ERROR] Usage: WIFI_SET_SSID:YourNetworkName (max 32 chars)\r\n");
        return true;
    }

    if (g_userConfig && userconfig_set_wifi_ssid(g_userConfig, ssid)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "\r\n[INFO] WiFi SSID set to: %s (not saved to NVRAM yet)\r\n", ssid);
        console_send(huart, msg);
    } else {
        console_send(huart, "\r\n[ERROR] Failed to set WiFi SSID\r\n");
    }

    return true;
}

static bool cmd_wifi_set_password(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* password = console_get_arg(cmd_buffer, "WIFI_SET_PASSWORD");
    if (!password || strlen(password) == 0 || strlen(password) > 64) {
        console_send(huart, "\r\n[ERROR] Usage: WIFI_SET_PASSWORD:YourPassword (max 64 chars)\r\n");
        return true;
    }

    if (g_userConfig && userconfig_set_wifi_password(g_userConfig, password)) {
        console_send(huart, "\r\n[INFO] WiFi password set (not saved to NVRAM yet)\r\n");
    } else {
        console_send(huart, "\r\n[ERROR] Failed to set WiFi password\r\n");
    }

    return true;
}

static bool cmd_wifi_set_autoconnect(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* value = console_get_arg(cmd_buffer, "WIFI_SET_AUTOCONNECT");
    if (!value) {
        console_send(huart, "\r\n[ERROR] Usage: WIFI_SET_AUTOCONNECT:on|off\r\n");
        return true;
    }

    if (!g_userConfig) {
        console_send(huart, "\r\n[ERROR] User config not available\r\n");
        return true;
    }

    if (strcasecmp(value, "on") == 0 || strcasecmp(value, "1") == 0 || strcasecmp(value, "true") == 0) {
        userconfig_set_wifi_autoconnect(g_userConfig, true);
        console_send(huart, "\r\n[INFO] WiFi auto-connect enabled (not saved to NVRAM yet)\r\n");
    } else if (strcasecmp(value, "off") == 0 || strcasecmp(value, "0") == 0 || strcasecmp(value, "false") == 0) {
        userconfig_set_wifi_autoconnect(g_userConfig, false);
        console_send(huart, "\r\n[INFO] WiFi auto-connect disabled (not saved to NVRAM yet)\r\n");
    } else {
        console_send(huart, "\r\n[ERROR] Usage: WIFI_SET_AUTOCONNECT:on|off\r\n");
    }

    return true;
}

static bool cmd_wifi_save(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (g_userConfig && userconfig_save_wifi(g_userConfig)) {
        console_send(huart, "\r\n[SUCCESS] WiFi credentials saved to NVRAM\r\n");
    } else {
        console_send(huart, "\r\n[ERROR] Failed to save WiFi credentials to NVRAM\r\n");
    }
    return true;
}

static bool cmd_wifi_load(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (g_userConfig && userconfig_load_wifi(g_userConfig)) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                "\r\n[SUCCESS] WiFi credentials loaded from NVRAM\r\n"
                "SSID: %s\r\n",
                userconfig_get_wifi_ssid(g_userConfig));
        console_send(huart, msg);
    } else {
        console_send(huart, "\r\n[ERROR] Failed to load WiFi credentials from NVRAM (or no valid credentials stored)\r\n");
    }
    return true;
}

static bool cmd_wifi_reset(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (g_userConfig) {
        userconfig_reset_wifi(g_userConfig);
        console_send(huart, "\r\n[INFO] WiFi credentials reset to defaults (empty credentials)\r\n");
    } else {
        console_send(huart, "\r\n[ERROR] User config not available\r\n");
    }
    return true;
}

static bool cmd_wifi_bootloader(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Setting Wi-Fi module to bootloader mode...\r\n");

    if (wifi_enter_bootloader_mode()) {
        console_send(huart, "[INFO] Wi-Fi module is now in bootloader mode.\r\n");
        console_send(huart, "[INFO] Module ready for UART firmware flashing.\r\n");
        console_send(huart, "[INFO] Use 'wifiExitBootloader' to exit bootloader mode.\r\n");
    } else {
        console_send(huart, "[ERROR] Failed to enter Wi-Fi bootloader mode.\r\n");
    }

    return true;
}

static bool cmd_wifi_exit_bootloader(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Exiting Wi-Fi bootloader mode...\r\n");

    if (wifi_exit_bootloader_mode()) {
        console_send(huart, "[INFO] Wi-Fi module reset and returned to normal operation.\r\n");
    } else {
        console_send(huart, "[ERROR] Failed to exit Wi-Fi bootloader mode.\r\n");
    }

    return true;
}

static bool cmd_wifi_diagnose(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    if (!g_userConfig) {
        console_send(huart, "\r\n[ERROR] User config not available\r\n");
        return true;
    }

    char msg[512];

    // Check WiFi credentials using public API only
    bool has_wifi = userconfig_has_wifi_credentials(g_userConfig);
    const char* ssid = userconfig_get_wifi_ssid(g_userConfig);
    bool autoconnect = userconfig_get_wifi_autoconnect(g_userConfig);

    snprintf(msg, sizeof(msg),
            "\r\n=== WiFi Configuration Diagnostic ===\r\n"
            "Credentials in NVRAM: %s\r\n"
            "SSID: %s\r\n"
            "Auto-connect: %s\r\n"
            "======================================\r\n",
            has_wifi ? "YES" : "NO",
            ssid ? ssid : "(not set)",
            autoconnect ? "Enabled" : "Disabled");
    console_send(huart, msg);

    // Check BatteryMonitor block
    if (userconfig_has_batmon_calibration(g_userConfig)) {
        const userconfig_batmon_t* batmon_block = userconfig_get_batmon_block(g_userConfig);
        if (batmon_block) {
            snprintf(msg, sizeof(msg),
                    "\r\n=== BatteryMonitor Calibration Status ===\r\n"
                    "Status: CALIBRATED\r\n"
                    "Version: %u\r\n"
                    "Vref: %.3f V\r\n"
                    "Zero current (raw): %.3f A\r\n"
                    "Zero current (buffered): %.3f A\r\n"
                    "==========================================\r\n",
                    batmon_block->version,
                    batmon_block->vref,
                    batmon_block->zero_current_raw,
                    batmon_block->zero_current_buffered);
        } else {
            console_send(huart, "\r\n[ERROR] Could not read BatteryMonitor block\r\n");
        }
    } else {
        snprintf(msg, sizeof(msg),
                "\r\n=== BatteryMonitor Calibration Status ===\r\n"
                "Status: NOT CALIBRATED\r\n"
                "Action: Run battery calibration procedure\r\n"
                "         with 0 current flowing\r\n"
                "==========================================\r\n");
    }
    console_send(huart, msg);

    return true;
}

// ============================================================================
// Command Registration
// ============================================================================

const console_command_t wifi_commands[] = {
    { .command = "WIFI_SHOW", .handler = cmd_wifi_show, .help_text = "Show stored WiFi credentials" },
    { .command = "WIFI_SET_SSID", .handler = cmd_wifi_set_ssid, .help_text = "Set WiFi SSID (max 32 chars)" },
    { .command = "WIFI_SET_PASSWORD", .handler = cmd_wifi_set_password, .help_text = "Set WiFi password (max 64 chars)" },
    { .command = "WIFI_SET_AUTOCONNECT", .handler = cmd_wifi_set_autoconnect, .help_text = "Enable/disable auto-connect" },
    { .command = "WIFI_SAVE", .handler = cmd_wifi_save, .help_text = "Save WiFi credentials to NVRAM" },
    { .command = "WIFI_LOAD", .handler = cmd_wifi_load, .help_text = "Load WiFi credentials from NVRAM" },
    { .command = "WIFI_RESET", .handler = cmd_wifi_reset, .help_text = "Reset WiFi credentials to defaults" },
    { .command = "WIFI_DIAGNOSE", .handler = cmd_wifi_diagnose, .help_text = "Diagnose WiFi and BatteryMonitor NVRAM status" },
    { .command = "wifiBootloader", .handler = cmd_wifi_bootloader, .help_text = "Enter WiFi module bootloader mode" },
    { .command = "wifiExitBootloader", .handler = cmd_wifi_exit_bootloader, .help_text = "Exit WiFi module bootloader mode" }
};

const size_t wifi_commands_count = sizeof(wifi_commands) / sizeof(wifi_commands[0]);
