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
 * @file    boot_fuse_qspiFlash.hpp
 * @brief   QSPI Flash Backend for Boot Mode Selection System
 * @details QSPI flash-based implementation of boot fuse storage that determines
 *          whether the STM32F767 system boots into application mode or bootloader
 *          mode. Provides persistent, non-volatile storage of boot configuration
 *          using the high-speed QSPI flash interface.
 * 
 * QSPI Flash Boot Mode Features:
 * - High-speed QSPI flash storage for boot mode selection
 * - Non-volatile storage survives power cycles and system resets
 * - CRC8-protected metadata structure for data integrity
 * - Magic number validation for configuration verification
 * - Integrated with main QSPI flash storage system
 * - Atomic write operations to dedicated flash sector
 * 
 * Storage Implementation Details:
 * - Uses boot_fuse_metadata_t structure with CRC8 protection
 * - Stored at QSPI_FUSE_ADDR in dedicated flash sector
 * - Magic number (BOOT_FUSE_MAGIC) for validation
 * - Version control for future compatibility
 * - 3-byte fuse data pattern (BTL = 0x42,0x54,0x4C for SET)
 * - Sector erase before write to ensure clean state
 * 
 * Boot Mode Integration:
 * - Application mode: Normal drone controller operation
 * - Bootloader mode: Firmware update and recovery operations
 * - Triggered by UART "jumpTobootloader" command
 * - Enables remote firmware updates over communication interface
 */

#ifndef __BOOT_FUSE_QSPI_FLASH_HPP
#define __BOOT_FUSE_QSPI_FLASH_HPP

#include <stdint.h>
#include "boot_fuse.hpp"

#ifdef __cplusplus
extern "C" {
#endif

 bool qspiFlash_set_fuse();

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_FUSE_QSPI_FLASH_HPP */
