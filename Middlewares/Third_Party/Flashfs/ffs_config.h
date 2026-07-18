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
 * @file    ffs_config.h
 * @brief   FlashFs - Central Configuration
 * @details Configuration constants and parameters for FlashFs,
 *          allowing customization of filesystem behavior, memory allocation,
 *          wear leveling parameters, and hardware-specific settings.
 * 
 * This file contains all configurable constants for FlashFs.
 * Modify values here to customize filesystem behavior.
 */

#ifndef __FFS_CONFIG_H
#define __FFS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

// =============================================================================
// FILESYSTEM LIMITS CONFIGURATION
// =============================================================================

/** @brief Maximum number of concurrent files in filesystem */
#define FFS_MAX_FILES               64

/** @brief Maximum filename length (including null terminator) */
#define FFS_MAX_FILENAME            32

// =============================================================================
// BLOCK CONFIGURATION
// =============================================================================

/** @brief End-of-file block marker value */
#define FFS_BLOCK_EOF               0xFFFFFFFFU

/** @brief Default block size in bytes */
#define FFS_DEFAULT_BLOCK_SIZE      4096

/** @brief Default block count (calculated for 16MB flash) */
#define FFS_DEFAULT_BLOCK_COUNT     (16 * 1024 * 1024 / FFS_DEFAULT_BLOCK_SIZE)

// =============================================================================
// HARDWARE/PLATFORM CONFIGURATION
// =============================================================================

/** @brief Enable D-Cache cleaning for STM32F7/H7 (1=enable, 0=disable) */
#define FFS_USE_CLEAN_DCACHE        1U

/** @brief Use hardware RNG for wear leveling (1=hardware, 0=software PRNG) */
#define BLOCK_ALLOC_USES_RNG        1U

/** @brief Include main.h for hardware definitions when using D-Cache or RNG */
#if FFS_USE_CLEAN_DCACHE || BLOCK_ALLOC_USES_RNG
    #include "main.h"
#endif

// =============================================================================
// THREAD SAFETY CONFIGURATION
// =============================================================================

/** @brief Serialize all public FFS API calls with a recursive mutex
 *         (FreeRTOS). Required when multiple tasks use the filesystem —
 *         e.g. the persistent logger, console commands, tests and TCP
 *         transfers all run concurrently. 0 = no locking (single-task use). */
#ifndef FFS_USE_LOCKING
#define FFS_USE_LOCKING             1U
#endif

// =============================================================================
// BLOCK ALLOCATION CONFIGURATION
// =============================================================================

// =============================================================================
// DEBUG CONFIGURATION
// =============================================================================

/** @brief Block size for debug hex dumps */
#define DEBUG_BLOCK_SIZE            256U

/** @brief Enable FFS internal logging (1=enable, 0=disable) */
#define USE_FFS_LOG                 0U

// =============================================================================
// LOGGING CONFIGURATION
// =============================================================================

#define FFS_LOG_LEVEL_NONE          0U
#define FFS_LOG_LEVEL_ERROR         1U  
#define FFS_LOG_LEVEL_WARN          2U
#define FFS_LOG_LEVEL_INFO          3U
#define FFS_LOG_LEVEL_DEBUG         4U

/** @brief Master logging level (set to desired maximum level) */
#define FFS_LOG_LEVEL               FFS_LOG_LEVEL_INFO

/** @brief Individual logging category controls (1=enable, 0=disable) */
#define FFS_LOG_FILE_OPS            1U  // File create/delete/open operations
#define FFS_LOG_WRITE_OPS           0U  // Write loop debugging (verbose)
#define FFS_LOG_READ_OPS            0U  // Read operations debugging  
#define FFS_LOG_DEBUG_DUMP          0U  // Block hex dumps
#define FFS_LOG_CHAIN_OPS           1U  // Chain rebuild/reset operations
#define FFS_LOG_SYSTEM              1U  // System verification/integrity checks
#define FF_USE_ERROR_LOG            1U  // Keep error logging separate

// =============================================================================
// FILESYSTEM SIGNATURE
// =============================================================================

/** @brief FFS filesystem magic signature */
#define FFS_MAGIC                   0x46465321UL  // 'FFS!'

#ifdef __cplusplus
}
#endif

#endif /* __FFS_CONFIG_H */
