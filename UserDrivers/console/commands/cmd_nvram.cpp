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
 * @file    cmd_nvram.cpp
 * @brief   NVRAM diagnostic console commands
 */

extern "C" {
#include "console_internal.h"
#include "cmsis_os.h"
}

#include "driver_manager.hpp"
#include "nvram.hpp"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Command Handlers
// ============================================================================

static bool cmd_nvram_write(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Usage: NVRAM_WRITE:0x0000:0xDEADBEEF
    // Writes 32-bit value to NVRAM at specified address

    const char* address_str = console_get_arg(cmd_buffer, "NVRAM_WRITE");
    if (!address_str) {
        console_send(huart, "\r\n[ERROR] Usage: NVRAM_WRITE:0xAAAA:0xVVVVVVVV (address:value)\r\n");
        return true;
    }

    // Parse address and value from string like "0x0000:0xDEADBEEF"
    char address_part[16] = {0};
    char value_part[16] = {0};

    const char* colon = strchr(address_str, ':');
    if (!colon) {
        console_send(huart, "\r\n[ERROR] Format: NVRAM_WRITE:0xAAAA:0xVVVVVVVV\r\n");
        return true;
    }

    // Copy address part
    size_t addr_len = colon - address_str;
    if (addr_len >= sizeof(address_part)) addr_len = sizeof(address_part) - 1;
    strncpy(address_part, address_str, addr_len);
    address_part[addr_len] = '\0';

    // Copy value part
    strncpy(value_part, colon + 1, sizeof(value_part) - 1);
    value_part[sizeof(value_part) - 1] = '\0';

    // Parse hex values
    uint16_t addr = (uint16_t)strtol(address_part, NULL, 16);
    uint32_t value = (uint32_t)strtol(value_part, NULL, 16);

    // Get NVRAM driver via DriverManager
    auto& dm = DriverManager::getInstance();
    NVRAM* nvram = dm.getNVRAM();

    if (!nvram) {
        console_send(huart, "\r\n[ERROR] NVRAM driver not available\r\n");
        return true;
    }

    // Write 4 bytes to NVRAM
    uint8_t data[4];
    data[0] = (value >> 0) & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;

    nvram->writeArray(addr, data, 4);

    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n[SUCCESS] Wrote 0x%08lX to NVRAM address 0x%04X\r\n", value, addr);
    console_send(huart, msg);

    return true;
}

static bool cmd_nvram_read(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Usage: NVRAM_READ:0x0000
    // Reads 32-bit value from NVRAM at specified address

    const char* address_str = console_get_arg(cmd_buffer, "NVRAM_READ");
    if (!address_str) {
        console_send(huart, "\r\n[ERROR] Usage: NVRAM_READ:0xAAAA\r\n");
        return true;
    }

    uint16_t addr = (uint16_t)strtol(address_str, NULL, 16);

    // Get NVRAM driver via DriverManager
    auto& dm = DriverManager::getInstance();
    NVRAM* nvram = dm.getNVRAM();

    if (!nvram) {
        console_send(huart, "\r\n[ERROR] NVRAM driver not available\r\n");
        return true;
    }

    // Read 4 bytes from NVRAM
    uint8_t data[4];
    nvram->readArray(addr, data, 4);

    uint32_t value = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];

    char msg[128];
    snprintf(msg, sizeof(msg),
             "\r\n[INFO] NVRAM[0x%04X] = 0x%08lX\r\n"
             "       Bytes: 0x%02X 0x%02X 0x%02X 0x%02X\r\n",
             addr, value, data[0], data[1], data[2], data[3]);
    console_send(huart, msg);

    return true;
}

static bool cmd_nvram_status(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Read and display NVRAM status register and device ID

    console_send(huart, "\r\n[INFO] NVRAM Hardware Status\r\n");
    console_send(huart, "========================================\r\n");

    auto& dm = DriverManager::getInstance();
    NVRAM* nvram = dm.getNVRAM();

    if (!nvram) {
        console_send(huart, "[ERROR] NVRAM driver not available\r\n");
        return true;
    }

    // Read device ID
    uint32_t device_id = nvram->readDeviceID();
    char msg[256];
    snprintf(msg, sizeof(msg),
             "[INFO] Device ID: 0x%08lX (Expected: 0x06818820)\r\n",
             device_id);
    console_send(huart, msg);

    // Read status register
    NVRAM_StatusRegister status = nvram->readStatusRegister();
    snprintf(msg, sizeof(msg),
             "[INFO] Status Register: 0x%02X\r\n"
             "       RDY=%u WEN=%u BP0=%u BP1=%u SNL=%u WPEN=%u\r\n",
             (status.RDY | (status.WEN << 1) | (status.BP0 << 2) | (status.BP1 << 3) | (status.SNL << 6) | (status.WPEN << 7)),
             status.RDY, status.WEN, status.BP0, status.BP1, status.SNL, status.WPEN);
    console_send(huart, msg);

    console_send(huart, "========================================\r\n");
    return true;
}

static bool cmd_nvram_verify(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Write test pattern, read it back, verify match

    console_send(huart, "\r\n[INFO] NVRAM Read/Write Verification Test\r\n");
    console_send(huart, "========================================\r\n");

    auto& dm = DriverManager::getInstance();
    NVRAM* nvram = dm.getNVRAM();

    if (!nvram) {
        console_send(huart, "[ERROR] NVRAM driver not available\r\n");
        return true;
    }

    // Test address: 0x0000 (user space)
    uint16_t test_addr = 0x0000;
    uint32_t test_pattern = 0xDEADBEEF;

    // Write test pattern
    uint8_t write_data[4];
    write_data[0] = (test_pattern >> 0) & 0xFF;
    write_data[1] = (test_pattern >> 8) & 0xFF;
    write_data[2] = (test_pattern >> 16) & 0xFF;
    write_data[3] = (test_pattern >> 24) & 0xFF;

    nvram->writeArray(test_addr, write_data, 4);
    console_send(huart, "[INFO] Wrote 0xDEADBEEF to address 0x0000\r\n");

    // Small delay to ensure write completes
    osDelay(100);

    // Read back
    uint8_t read_data[4];
    nvram->readArray(test_addr, read_data, 4);

    uint32_t read_value = (read_data[3] << 24) | (read_data[2] << 16) | (read_data[1] << 8) | read_data[0];

    char msg[256];
    snprintf(msg, sizeof(msg),
             "[INFO] Read 0x%08lX from address 0x0000\r\n"
             "[INFO] Bytes read: 0x%02X 0x%02X 0x%02X 0x%02X\r\n",
             read_value, read_data[0], read_data[1], read_data[2], read_data[3]);
    console_send(huart, msg);

    if (read_value == test_pattern) {
        console_send(huart, "\r\n[SUCCESS] NVRAM read/write verification PASSED!\r\n");
    } else {
        snprintf(msg, sizeof(msg),
                 "\r\n[ERROR] Verification FAILED!\r\n"
                 "        Expected: 0x%08lX\r\n"
                 "        Got:      0x%08lX\r\n",
                 test_pattern, read_value);
        console_send(huart, msg);
    }

    console_send(huart, "========================================\r\n");

    return true;
}

// ============================================================================
// Command Registration
// ============================================================================

extern "C" {

const console_command_t nvram_commands[] = {
    { .command = "NVRAM_WRITE", .handler = cmd_nvram_write, .help_text = "Write to NVRAM: NVRAM_WRITE:0xAAAA:0xVVVVVVVV" },
    { .command = "NVRAM_READ", .handler = cmd_nvram_read, .help_text = "Read from NVRAM: NVRAM_READ:0xAAAA" },
    { .command = "NVRAM_STATUS", .handler = cmd_nvram_status, .help_text = "Check NVRAM device ID and status register" },
    { .command = "NVRAM_VERIFY", .handler = cmd_nvram_verify, .help_text = "Test NVRAM read/write round-trip" }
};

const size_t nvram_commands_count = sizeof(nvram_commands) / sizeof(nvram_commands[0]);

}
