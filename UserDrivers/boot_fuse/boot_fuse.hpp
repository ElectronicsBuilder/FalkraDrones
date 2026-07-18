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
 * @file    boot_fuse.hpp
 * @brief   Boot Mode Selection System - Application vs Bootloader Control
 * @details Boot fuse management system that determines whether the STM32F767
 *          system should boot into application mode or bootloader mode on startup.
 *          Uses persistent storage markers in QSPI flash to control boot behavior,
 *          enabling firmware updates and recovery operations.
 * 
 * Boot Mode Control Features:
 * - Persistent boot mode selection stored in QSPI flash
 * - Application mode: Normal drone controller operation
 * - Bootloader mode: Firmware update and recovery operations  
 * - Command-triggered mode switching via UART ("jumpTobootloader")
 * - CRC-protected boot configuration with metadata validation
 * - Atomic boot mode transitions with system reset
 * 
 * Boot Mode Operation:
 * - Default: System boots into application mode (main_cpp.cpp)
 * - When fuse is SET: System should boot into bootloader mode
 * - When fuse is CLEAR: System boots into application mode
 * - Mode switching triggered via UART command or programmatic control
 * 
 * Usage Example:
 * @code
 * // Trigger bootloader mode via UART command
 * // Send "jumpTobootloader" command over UART1
 * // System will set boot fuse and reset into bootloader
 * 
 * // Programmatic bootloader entry
 * qspiFlash_set_fuse();    // Set bootloader boot mode
 * NVIC_SystemReset();      // Reset into bootloader
 * 
 * // Boot sequence checks fuse state during startup
 * // and branches to appropriate mode (app vs bootloader)
 * @endcode
 */

#ifndef __BOOT_FUSE_HPP
#define __BOOT_FUSE_HPP

#include <stdint.h>


#define BOOT_FUSE_SET_BYTE1     0x42
#define BOOT_FUSE_SET_BYTE2     0x54
#define BOOT_FUSE_SET_BYTE3     0x4C

#define BOOT_FUSE_CLEAR_BYTE1   0x43
#define BOOT_FUSE_CLEAR_BYTE2   0x4C
#define BOOT_FUSE_CLEAR_BYTE3   0x52

#define BOOT_FUSE_SET        1
#define BOOT_FUSE_CLEAR      0
#define BOOT_FUSE_SIZE       3

#endif // __BOOT_FUSE_HPP
