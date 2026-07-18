/**
 * MIT License
 * Copyright (c) 2025 ElectronicsBuilder
 *
 * @file    cmd_system.c
 * @brief   System control console commands
 */

#include "console_internal.h"
#include "boot_fuse_qspiFlash.hpp"
#include "data_transport.h"
#include "cmsis_os2.h"
#include <string.h>

extern void uart_send_string(const char* str);

static bool cmd_restart(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Restarting Controller...\r\n");
    HAL_Delay(100);
    NVIC_SystemReset();
    return true;
}

static bool cmd_jump_to_bootloader(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Jumping to bootloader...\r\n");
    osDelay(100);
    qspiFlash_set_fuse();
    NVIC_SystemReset();
    return true;
}

static bool cmd_jump_to_app(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Jumping to application...\r\n");
    osDelay(100);
    NVIC_SystemReset();
    return true;
}

static bool cmd_jump_to_filesystem(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Starting File System...\r\n");
    data_transport_set_mode(APP_TRANSPORT_MODE_FILESYSTEM_COMMAND);
    return true;
}

static bool cmd_exit_filesystem(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Exiting filesystem mode, returning to shell...\r\n");
    data_transport_set_mode(APP_TRANSPORT_MODE_CONSOLE);
    return true;
}

static bool cmd_set_uart_command_mode(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Setting UART to COMMAND mode...\r\n");
    osDelay(100);
    data_transport_set_mode(APP_TRANSPORT_MODE_COMMAND);
    return true;
}

static bool cmd_help(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* help_msg =
        "\r\n=== AVAILABLE COMMANDS ===\r\n"
        "SHOWLOG               - Display current log file\r\n"
        "LISTLOGS             - List all log files\r\n"
        "READLOG:filename.log  - Read specified log file\r\n"
        "FORMATLOGDISK        - Format log storage area\r\n"
        "FORMATDISK           - Format entire filesystem\r\n"
        "DUMPBLOCKS           - Dump flash block status\r\n"
        "GETBLOCK:<num>       - Get detailed block info with chain\r\n"
        "RESET_ERASE_COUNTS   - Reset all block erase counts to zero\r\n"
        "DEBUGMOUNT          - Test filesystem mount status\r\n"
        "LISTFILES           - List all files in filesystem\r\n"
        "READFILE:filename    - Read specified file\r\n"
        "DELETEFILE:filename  - Delete specified file\r\n"
        "SEND_TCP:message     - Send message via TCP client\r\n"
        "tcpServerStart       - Start TCP server\r\n"
        "tcpServerStop        - Stop TCP server\r\n"
        "tcpServerStatus      - Get TCP server status\r\n"
        "BROADCAST_TCP:message- Broadcast message to all TCP clients\r\n"
        "WIFI_SHOW            - Show stored WiFi credentials\r\n"
        "WIFI_SET_SSID:name   - Set WiFi SSID (max 32 chars)\r\n"
        "WIFI_SET_PASSWORD:pw - Set WiFi password (max 64 chars)\r\n"
        "WIFI_SET_AUTOCONNECT:on|off - Enable/disable auto-connect\r\n"
        "WIFI_SAVE            - Save WiFi credentials to NVRAM\r\n"
        "WIFI_LOAD            - Load WiFi credentials from NVRAM\r\n"
        "WIFI_RESET           - Reset WiFi credentials to defaults\r\n"
        "GETTIME              - Get current RTC time\r\n"
        "SETTIME:YYYY-MM-DDTHH:MM:SS - Set RTC time\r\n"
        "TEST_PPM [timeout]   - Test PPM decoder (timeout in ms, default: 5000)\r\n"
        "spiTest              - Run SPI4 hardware test\r\n"
        "jumpToFilesystem     - Enter filesystem mode\r\n"
        "exitFilesystem      - Exit filesystem mode\r\n"
        "TEST_BATMON [duration] - Test battery current monitor (duration in ms, default: 10000)\r\n"
        "BATMON_CALIBRATE     - Calibrate battery current sensor (ensure 0A current!)\r\n"
        "BATMON_STATUS        - Show battery monitor status and current readings\r\n"
        "BATMON_SET_VREF <voltage> - Set ADC reference voltage (e.g., 3.28)\r\n"
        "BATMON_SET_POLARITY <0|1> - Set polarity (0=VCC+, 1=GND/return)\r\n"
        "BATMON_ENABLE_RAW <0|1> - Enable/disable raw channel in calculation\r\n"
        "BATMON_ENABLE_BUFFERED <0|1> - Enable/disable buffered channel in calculation\r\n"
        "BATMON_RESET         - Reset battery monitor calibration\r\n"
        "\r\n--- ENVIRONMENTAL SENSORS ---\r\n"
        "ENV_STATUS           - Show all environmental sensor status and readings\r\n"
        "ENV_TEMP_HUMIDITY    - Show temperature and humidity from SHT4x sensor\r\n"
        "ENV_PRESSURE_ALTITUDE - Show pressure and altitude from BMP581 sensor\r\n"
        "ENV_ALL              - Show all environmental data in compact format\r\n"
        "\r\n--- MEMORY DEVICES ---\r\n"
        "MEM_STATUS           - Show all memory device status and usage\r\n"
        "MEM_NVRAM            - Show NVRAM (CY14B101Q2) detailed information\r\n"
        "MEM_FLASH            - Show SPI and QSPI Flash detailed information\r\n"
        "MEM_HEALTH           - Show memory system health score and status\r\n"
        "FFS_STATUS           - Show FFS block allocation and wear statistics\r\n"
        "FFS_FILES            - List all files in Flash File System with sizes\r\n"
        "\r\n--- SYSTEM ---\r\n"
        "restart              - Restart the controller\r\n"
        "jumpTobootloader     - Jump to bootloader mode\r\n"
        "jumpToApp            - Jump to application\r\n"
        "SET_UART_COMMAND_MODE- Set UART to command mode\r\n"
        "HELP                 - Show this help message\r\n";
    console_send(huart, help_msg);
    return true;
}

const console_command_t system_commands[] = {
    { .command = "restart", .handler = cmd_restart, .help_text = "Restart the controller" },
    { .command = "jumpTobootloader", .handler = cmd_jump_to_bootloader, .help_text = "Jump to bootloader mode" },
    { .command = "jumpToApp", .handler = cmd_jump_to_app, .help_text = "Jump to application" },
    { .command = "jumpToFilesystem", .handler = cmd_jump_to_filesystem, .help_text = "Enter filesystem mode" },
    { .command = "exitFilesystem", .handler = cmd_exit_filesystem, .help_text = "Exit filesystem mode" },
    { .command = "SET_UART_COMMAND_MODE", .handler = cmd_set_uart_command_mode, .help_text = "Set UART to command mode" },
    { .command = "HELP", .handler = cmd_help, .help_text = "Show available commands" }
};

const size_t system_commands_count = sizeof(system_commands) / sizeof(system_commands[0]);
