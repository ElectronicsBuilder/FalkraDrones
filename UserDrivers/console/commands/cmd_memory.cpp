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
 * @file    cmd_memory.cpp
 * @brief   Memory device console commands (MEM_*, FFS_*)
 *          NVRAM, SPI Flash, and QSPI Flash, and Flash File System status monitoring
 */

#include "console_internal.h"
#include "driver_status.hpp"
#include "status.hpp"
#include "ffs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Command Handlers
// ============================================================================

static bool cmd_mem_status(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Get latest memory status
    auto mem_data = DriverStatus::getSnapshot().memory;
    auto status = DriverStatus::getSnapshot();

    char msg[512];
    int offset = 0;
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "MEMORY DEVICE STATUS\r\n"
            "========================================\r\n");

    // NVRAM Status
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\rNVRAM (CY14B101Q2)\r\n"
            "  Status:      %s\r\n"
            "  Capacity:    %.2f KB (128KB total)\r\n"
            "  Protection:  %s\r\n",
            status.nvramOk ? "OK" : "OFFLINE",
            mem_data.nvram_capacity_bytes / 1024.0f,
            mem_data.nvram_write_protected ? "ENABLED" : "DISABLED");

    // SPI Flash Status
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\rSPI Flash (W25Q128J)\r\n"
            "  Status:      %s\r\n"
            "  Capacity:    %.2f MB (16MB total)\r\n"
            "  Used:        %.2f MB\r\n",
            status.flashOk ? "OK" : "OFFLINE",
            mem_data.spi_flash_capacity_bytes / (1024.0f * 1024.0f),
            mem_data.spi_flash_used_bytes / (1024.0f * 1024.0f));

    // QSPI Flash Status
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\rQSPI Flash (W25Q128J)\r\n"
            "  Status:      %s\r\n"
            "  Capacity:    %.2f MB (16MB total)\r\n"
            "  Used:        %.2f MB\r\n",
            status.qspiOk ? "OK" : "OFFLINE",
            mem_data.qspi_flash_capacity_bytes / (1024.0f * 1024.0f),
            mem_data.qspi_flash_used_bytes / (1024.0f * 1024.0f));

    // Memory Health
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r----------------------------------------\r\n"
            "Memory Health Score: %u/100\r\n"
            "Last Update: %lu ms\r\n"
            "========================================\r\n",
            mem_data.memory_health_score,
            mem_data.timestamp_ms);

    console_send(huart, msg);
    return true;
}

static bool cmd_mem_nvram(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Get latest memory status
    auto mem_data = DriverStatus::getSnapshot().memory;
    auto status = DriverStatus::getSnapshot();

    char msg[256];
    int offset = 0;
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "NVRAM (CY14B101Q2) DETAILS\r\n"
            "========================================\r\n"
            "Status:      %s\r\n"
            "Capacity:    128 KB (131072 bytes)\r\n"
            "Write Protect: %s\r\n"
            "Characteristics:\r\n"
            "  - Instant-Write SRAM\r\n"
            "  - Unlimited write endurance\r\n"
            "  - Hardware autostore backup\r\n"
            "  - Serial: 8 bytes unique ID\r\n"
            "========================================\r\n",
            status.nvramOk ? "OK - READY" : "OFFLINE",
            mem_data.nvram_write_protected ? "ENABLED (Read-Only)" : "DISABLED (Read-Write)");

    console_send(huart, msg);
    return true;
}

static bool cmd_mem_flash(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Get latest memory status
    auto mem_data = DriverStatus::getSnapshot().memory;
    auto status = DriverStatus::getSnapshot();

    char msg[512];
    int offset = 0;
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "FLASH MEMORY STATUS\r\n"
            "========================================\r\n"
            "SPI Flash (W25Q128J)\r\n"
            "  Status:     %s\r\n"
            "  Capacity:   16 MB (16777216 bytes)\r\n"
            "  Interface:  Standard SPI\r\n"
            "  Page Size:  256 bytes\r\n"
            "  Sector Size: 4 KB (4096 bytes)\r\n"
            "\rQSPI Flash (W25Q128J)\r\n"
            "  Status:     %s\r\n"
            "  Capacity:   16 MB (16777216 bytes)\r\n"
            "  Interface:  Quad SPI (QSPI)\r\n"
            "  Page Size:  256 bytes\r\n"
            "  Sector Size: 4 KB (4096 bytes)\r\n"
            "  Purpose:    TouchGFX Assets (memory-mapped)\r\n"
            "========================================\r\n",
            status.flashOk ? "OK - READY" : "OFFLINE",
            status.qspiOk ? "OK - READY" : "OFFLINE");

    console_send(huart, msg);
    return true;
}

static bool cmd_mem_health(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Get latest memory status
    auto mem_data = DriverStatus::getSnapshot().memory;
    auto status = DriverStatus::getSnapshot();

    uint32_t health = mem_data.memory_health_score;
    const char* health_status;
    if (health >= 90) {
        health_status = "EXCELLENT";
    } else if (health >= 75) {
        health_status = "GOOD";
    } else if (health >= 50) {
        health_status = "FAIR";
    } else if (health >= 25) {
        health_status = "POOR";
    } else {
        health_status = "CRITICAL";
    }

    char msg[256];
    int offset = 0;
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "MEMORY SYSTEM HEALTH\r\n"
            "========================================\r\n"
            "Overall Health:  %u/100 (%s)\r\n"
            "\rDevice Status:\r\n"
            "  NVRAM:        %s\r\n"
            "  SPI Flash:    %s\r\n"
            "  QSPI Flash:   %s\r\n"
            "\rStatus Indicators:\r\n"
            "  NVRAM Write Protected: %s\r\n"
            "  Last Update: %lu ms\r\n"
            "========================================\r\n",
            health,
            health_status,
            status.nvramOk ? "✓ OK" : "✗ OFFLINE",
            status.flashOk ? "✓ OK" : "✗ OFFLINE",
            status.qspiOk ? "✓ OK" : "✗ OFFLINE",
            mem_data.nvram_write_protected ? "YES" : "NO",
            mem_data.timestamp_ms);

    console_send(huart, msg);
    return true;
}

static bool cmd_ffs_status(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    auto mem_data = DriverStatus::getSnapshot().memory;

    uint32_t ffs_used_mb = (mem_data.ffs_used_blocks * ffs_config.block_size) / (1024 * 1024);
    uint32_t ffs_free_mb = (mem_data.ffs_free_blocks * ffs_config.block_size) / (1024 * 1024);
    uint32_t ffs_total_mb = (mem_data.ffs_total_blocks * ffs_config.block_size) / (1024 * 1024);
    uint32_t ffs_usage_pct = (mem_data.ffs_total_blocks > 0) ?
                             (mem_data.ffs_used_blocks * 100) / mem_data.ffs_total_blocks : 0;

    char msg[512];
    int offset = 0;
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "FLASH FILE SYSTEM (FFS) STATUS\r\n"
            "========================================\r\n"
            "Block Allocation:\r\n"
            "  Total Blocks:    %lu\r\n"
            "  Used Blocks:     %lu\r\n"
            "  Free Blocks:     %lu\r\n"
            "  Usage:           %lu%%\r\n"
            "\rStorage Capacity:\r\n"
            "  Total:           %lu MB\r\n"
            "  Used:            %lu MB\r\n"
            "  Free:            %lu MB\r\n"
            "\rWear Leveling Statistics:\r\n"
            "  Max Erase Count: %lu\r\n"
            "  Avg Erase Count: %lu\r\n"
            "  Last Updated:    %lu ms\r\n"
            "========================================\r\n",
            mem_data.ffs_total_blocks,
            mem_data.ffs_used_blocks,
            mem_data.ffs_free_blocks,
            ffs_usage_pct,
            ffs_total_mb,
            ffs_used_mb,
            ffs_free_mb,
            mem_data.ffs_max_erase_count,
            mem_data.ffs_avg_erase_count,
            mem_data.timestamp_ms);

    console_send(huart, msg);
    return true;
}

static bool cmd_ffs_files(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    ffs_file_info_t file_list[FFS_MAX_FILES];
    int file_count = ffs_list_files(file_list, FFS_MAX_FILES);

    char msg[1024];
    int offset = 0;
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "FFS FILE LISTING\r\n"
            "========================================\r\n");

    if (file_count <= 0) {
        offset += snprintf(msg + offset, sizeof(msg) - offset,
                "No files found in FFS\r\n");
    } else {
        offset += snprintf(msg + offset, sizeof(msg) - offset,
                "Total Files: %d\r\n\r\n"
                "Filename                          Size (bytes)\r\n"
                "--------------------------------  -----------\r\n",
                file_count);

        uint32_t total_size = 0;
        for (int i = 0; i < file_count && offset < (int)sizeof(msg) - 100; i++) {
            offset += snprintf(msg + offset, sizeof(msg) - offset,
                    "%-32s  %u\r\n",
                    file_list[i].name,
                    file_list[i].size);
            total_size += file_list[i].size;
        }

        offset += snprintf(msg + offset, sizeof(msg) - offset,
                "\r\nTotal Files: %d\r\n"
                "Total Size:  %lu bytes (%.2f MB)\r\n",
                file_count,
                total_size,
                total_size / (1024.0f * 1024.0f));
    }

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "========================================\r\n");

    console_send(huart, msg);
    return true;

}
// ============================================================================
// Command Registration
// ============================================================================

const console_command_t memory_commands[] = {
    {
        .command = "MEM_STATUS",
        .handler = cmd_mem_status,
        .help_text = "Show all memory device status and usage"
    },
    {
        .command = "MEM_NVRAM",
        .handler = cmd_mem_nvram,
        .help_text = "Show NVRAM (CY14B101Q2) detailed information"
    },
    {
        .command = "MEM_FLASH",
        .handler = cmd_mem_flash,
        .help_text = "Show SPI and QSPI Flash detailed information"
    },
    {
    },
    {
        .command = "FFS_STATUS",
        .handler = cmd_ffs_status,
        .help_text = "Show FFS block allocation and wear statistics"
    },
    {
        .command = "FFS_FILES",
        .handler = cmd_ffs_files,
        .help_text = "List all files in Flash File System with sizes"
    }
};

const size_t memory_commands_count = sizeof(memory_commands) / sizeof(memory_commands[0]);
