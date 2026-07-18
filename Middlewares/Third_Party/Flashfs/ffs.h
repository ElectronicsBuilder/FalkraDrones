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
 * @file    ffs.h
 * @brief   FlashFs - A lightweight filesystem for embedded flash storage
 * @details Custom-designed filesystem optimized for embedded flash storage providing
 *          persistent file storage with block-based allocation, wear leveling, and
 *          power-failure recovery capabilities for drone controller applications.
 * 
 * FlashFs provides persistent file storage with features like:
 * - Block-based allocation with wear leveling
 * - Text and binary file support
 * - Persistent logging capabilities
 * - Power-failure recovery
 * - Optimized for SPI/QSPI flash memory
 */

#ifndef __FFS_H
#define __FFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "ffs_config.h"

#if FFS_LOG_LEVEL > FFS_LOG_LEVEL_NONE
#include "log.hpp"
#endif

#if FF_USE_ERROR_LOG
#include "log.hpp"
#endif
/*
  Memory Usage Breakdown based on Max Files of 64

  | Component            | Size Calculation       | Flash Usage | RAM Usage |
  |----------------------|------------------------|-------------|-----------|
  | File Table Entries   | 60 bytes × 64 files    | 3.84 KB     | 3.84 KB   |
  | File Table Header    | Magic + Count + CRC    | 0.01 KB     | -         |
  | Block Table          | 9 bytes × 4096 blocks  | 40.0 KB     | 36.0 KB   |
  | Reserved Flash Areas | Header + Table blocks  | ~10.0 KB    | -         |
  | Runtime Config       | ffs_config_t structure | -           | 0.05 KB   |
  | TOTAL OVERHEAD       |                        | 53.85 KB    | 39.89 KB  |

  Storage Efficiency Analysis

  | Flash Size      | Overhead | Usable Space | Efficiency |
  |-----------------|----------|--------------|------------|
  | 16 MB (W25Q128) | 53.85 KB | 15.95 MB     | 99.66%     |
  | 8 MB (W25Q64)   | 53.85 KB | 7.95 MB      | 99.33%     |
  | 4 MB (W25Q32)   | 53.85 KB | 3.95 MB      | 98.65%     |
  | 2 MB (W25Q16)   | 53.85 KB | 1.95 MB      | 97.31%     |

  RAM Impact Analysis

  | System RAM | Overhead | Available RAM | Impact |
  |------------|----------|---------------|--------|
  | 512 KB     | 39.89 KB | 472.11 KB     | 7.8%   |
  | 256 KB     | 39.89 KB | 216.11 KB     | 15.6%  |
  | 128 KB     | 39.89 KB | 88.11 KB      | 31.2%  |
  | 64 KB      | 39.89 KB | 24.11 KB      | 62.3%  |
*/

// =============================================================================
// CONFIGURATION (Centralized in ffs_config.h)
// =============================================================================
// All configurable constants have been moved to ffs_config.h for centralized management

// Seek origins
#define FFS_SEEK_SET                0
#define FFS_SEEK_CUR                1
#define FFS_SEEK_END                2

// Write semantics
#define FFS_ERR_NOT_APPEND         -5

// =============================================================================
// LOGGING MACROS (Based on ffs_config.h settings)
// =============================================================================

// Conditional logging macros based on configuration
#if FFS_LOG_LEVEL >= FFS_LOG_LEVEL_INFO && FFS_LOG_FILE_OPS
    #define FFS_LOG_FILE_INFO(...) LOG_INFO(__VA_ARGS__)
#else
    #define FFS_LOG_FILE_INFO(...)
#endif

#if FFS_LOG_LEVEL >= FFS_LOG_LEVEL_DEBUG && FFS_LOG_WRITE_OPS  
    #define FFS_LOG_WRITE_DEBUG(...) LOG_INFO(__VA_ARGS__)
#else
    #define FFS_LOG_WRITE_DEBUG(...)
#endif

#if FFS_LOG_LEVEL >= FFS_LOG_LEVEL_DEBUG && FFS_LOG_READ_OPS
    #define FFS_LOG_READ_DEBUG(...) LOG_INFO(__VA_ARGS__)
#else  
    #define FFS_LOG_READ_DEBUG(...)
#endif

#if FFS_LOG_LEVEL >= FFS_LOG_LEVEL_DEBUG && FFS_LOG_DEBUG_DUMP
    #define FFS_LOG_DEBUG_DUMP_INFO(...) LOG_INFO(__VA_ARGS__)
#else
    #define FFS_LOG_DEBUG_DUMP_INFO(...)
#endif

#if FFS_LOG_LEVEL >= FFS_LOG_LEVEL_INFO && FFS_LOG_CHAIN_OPS
    #define FFS_LOG_CHAIN_INFO(...) LOG_INFO(__VA_ARGS__)  
#else
    #define FFS_LOG_CHAIN_INFO(...)
#endif

#if FFS_LOG_LEVEL >= FFS_LOG_LEVEL_INFO && FFS_LOG_SYSTEM
    #define FFS_LOG_SYSTEM_INFO(...) LOG_INFO(__VA_ARGS__)
    #define FFS_LOG_SYSTEM_WARN(...) LOG_WARN(__VA_ARGS__)
    #define FFS_LOG_SYSTEM_ERROR(...) LOG_ERROR(__VA_ARGS__)
#elif FFS_LOG_LEVEL >= FFS_LOG_LEVEL_WARN && FFS_LOG_SYSTEM
    #define FFS_LOG_SYSTEM_INFO(...)
    #define FFS_LOG_SYSTEM_WARN(...) LOG_WARN(__VA_ARGS__)
    #define FFS_LOG_SYSTEM_ERROR(...) LOG_ERROR(__VA_ARGS__)
#elif FFS_LOG_LEVEL >= FFS_LOG_LEVEL_ERROR && FFS_LOG_SYSTEM
    #define FFS_LOG_SYSTEM_INFO(...)
    #define FFS_LOG_SYSTEM_WARN(...)
    #define FFS_LOG_SYSTEM_ERROR(...) LOG_ERROR(__VA_ARGS__)
#else
    #define FFS_LOG_SYSTEM_INFO(...)
    #define FFS_LOG_SYSTEM_WARN(...)
    #define FFS_LOG_SYSTEM_ERROR(...)
#endif

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief File entry structure - tracks individual file metadata
 */
typedef struct {
    char name[FFS_MAX_FILENAME];    ///< Filename (null-terminated)
    uint32_t head_block;            ///< First block in file chain
    uint32_t tail_block;            ///< Last block in file chain  
    uint32_t size;                  ///< Current file size in bytes
    uint32_t reserved;              ///< Max allowed size (0 = unlimited)
    uint32_t cursor;                ///< Current read/write position
    uint32_t offset;                ///< Physical offset of first block
    bool in_use;                    ///< File slot is active
} ffs_file_entry_t;

/** @brief Size of ffs_file_entry_t structure in bytes */
#define FFS_FILE_ENTRY_SIZE         60  // 32+4+4+4+4+4+4+1 + 7 padding = 60 bytes

/**
 * @brief Block entry structure - tracks individual block metadata
 */
typedef struct {
    uint32_t next;                  ///< Next block in chain (FFS_BLOCK_EOF = end)
    uint8_t  in_use;                ///< Block is allocated
    uint8_t  padding;               ///< Alignment padding
    uint16_t valid_bytes;           ///< Valid data bytes in block
    uint32_t erase_count;           ///< Wear leveling counter
} __attribute__((packed)) ffs_block_entry_t;

/** @brief Size of ffs_block_entry_t structure in bytes */
#define FFS_BLOCK_ENTRY_SIZE        12  // 4+1+1+2+4 = 12 bytes (packed)

/** @brief Number of blocks needed to store block metadata table */
#define FFS_BLOCK_META_BLOCKS       ((FFS_DEFAULT_BLOCK_COUNT * FFS_BLOCK_ENTRY_SIZE + FFS_DEFAULT_BLOCK_SIZE - 1) / FFS_DEFAULT_BLOCK_SIZE)

/**
 * @brief Block device configuration - defines hardware interface
 */
typedef struct {
    uint32_t block_size;            ///< Block size in bytes
    uint32_t block_count;           ///< Total blocks available
    void* context;                  ///< Hardware driver context

    // Hardware driver function pointers
    int (*read)(void* context, uint32_t addr, void* buffer, uint32_t size);
    int (*prog)(void* context, uint32_t addr, const void* data, uint32_t size);
    int (*erase)(void* context, uint32_t block);
    int (*eraseChip)(void* context);
    int (*sync)(void* context);
    int (*init)(void* context);

    // Block allocation layout
    uint32_t blockmeta_start_block;
    uint32_t blockmeta_block_count;
    uint32_t header_block;
    uint32_t table_block;
    uint32_t reserved_blocks;
    uint32_t reserved_start_block;

    // Address mappings
    uint32_t header_addr;
    uint32_t table_addr;
    uint32_t file_data_addr;

    // Limits
    uint32_t max_files;
    uint32_t max_filename_len;
} ffs_config_t;

/**
 * @brief File information for directory listings
 */
typedef struct {
    char name[FFS_MAX_FILENAME];    ///< Filename
    uint32_t size;                  ///< File size in bytes
} ffs_file_info_t;


extern ffs_file_entry_t file_table[FFS_MAX_FILES];


// =============================================================================
// THREAD SAFETY
// =============================================================================

/**
 * @brief Acquire/release the filesystem lock (recursive; safe to nest).
 * @details All public API functions take this lock internally. Call these
 *          directly only when composing multiple FFS calls that must be
 *          atomic as a group. No-ops before the scheduler starts or when
 *          FFS_USE_LOCKING is 0.
 */
void ffs_lock(void);
void ffs_unlock(void);

/**
 * @brief True if the calling task currently holds the FFS lock.
 * @details Lets external sinks (e.g. a persistent logger) avoid re-entering
 *          the filesystem from log statements emitted INSIDE filesystem
 *          operations — nested writes during a metadata update corrupt state.
 */
bool ffs_lock_held_by_current_task(void);

// =============================================================================
// CORE FILESYSTEM API
// =============================================================================

/**
 * @brief Mount the filesystem with given configuration
 * @param cfg Block device configuration
 * @return true on success, false on failure
 */
bool ffs_mount(const ffs_config_t* cfg);

/**
 * @brief Check whether the filesystem is currently mounted.
 * @return true after a successful mount, false before mount or after format failure
 */
bool ffs_is_mounted(void);

/**
 * @brief Format the entire filesystem (erases all data)
 * @return 0 on success, negative on error
 */
int ffs_format(void);

/**
 * @brief Initialize filesystem structures
 * @return 0 on success, negative on error
 */
int ffs_init(void);

/**
 * @brief Format a single block (debug/recovery function)
 * @return 0 on success, negative on error
 */
int ffs_format_block(void);

/**
 * @brief Create a new file
 * @param name Filename (max FFS_MAX_FILENAME chars)
 * @param size Reserved size in bytes (0 = unlimited growth)
 * @return File ID on success, negative on error
 */
int ffs_create(const char* name, uint32_t size);

/**
 * @brief Open an existing file for read/write
 * @param name Filename to open
 * @return File ID on success, negative if not found
 */
int ffs_open(const char* name);

/**
 * @brief Write binary data to file
 * @param file_id File ID from ffs_create/ffs_open
 * @param data Data buffer to write
 * @param len Number of bytes to write
 * @return Bytes written on success, negative on error
 */
int ffs_write(int file_id, const void* data, uint32_t len);

/**
 * @brief Read binary data from file
 * @param file_id File ID from ffs_open
 * @param data Buffer to read into
 * @param len Maximum bytes to read
 * @return Bytes read on success, negative on error
 */
int ffs_read(int file_id, void* data, uint32_t len);

/**
 * @brief Seek to position in file
 * @param file_id File ID
 * @param offset Offset value
 * @param start Origin (FFS_SEEK_SET, FFS_SEEK_CUR, FFS_SEEK_END)
 * @return 0 on success, negative on error
 */
int ffs_seek(int file_id, int32_t offset, int start);

/**
 * @brief Get current file position
 * @param file_id File ID from ffs_open
 * @return Current position on success, negative on error
 */
int32_t ffs_tell(int file_id);

/**
 * @brief Delete a file
 * @param name Filename to delete
 * @return 0 on success, negative on error
 */
int ffs_delete(const char* name);

// =============================================================================
// CONVENIENCE API
// =============================================================================

/**
 * @brief Open file or create if it doesn't exist, then reset (truncate)
 * @param name Filename
 * @param reserve_size Reserved size for new file
 * @return File ID on success, negative on error
 */
int ffs_open_or_create_reset(const char* name, uint32_t reserve_size);

/**
 * @brief Open persistent log file (creates if needed, seeks to end)
 * @param name Log filename
 * @return File ID on success, negative on error
 */
int ffs_open_log(const char* name);

/**
 * @brief Open persistent binary file (creates if needed, seeks to end)
 * @param name Binary filename  
 * @return File ID on success, negative on error
 */
int ffs_open_binary(const char* name);

/**
 * @brief Write text line with automatic newline
 * @param file_id File ID
 * @param line Text line to write (newline added if missing)
 * @return Bytes written on success, negative on error
 */
int ffs_write_line(int file_id, const char* line);

/**
 * @brief Read text line (stops at newline)
 * @param file_id File ID
 * @param buf Buffer for line text
 * @param max_len Maximum buffer size
 * @return Line length on success, 0 on EOF, negative on error
 */
int ffs_read_line(int file_id, char* buf, uint32_t max_len);

/**
 * @brief Check if file exists
 * @param name Filename to check
 * @return true if file exists, false otherwise
 */
bool ffs_file_exists(const char* name);

/**
 * @brief Find file by name (internal function)
 * @param name Filename to find
 * @return File table index on success, negative if not found
 */
int ffs_find_file(const char* name);

/**
 * @brief List all files in filesystem
 * @param out Array of file info structures
 * @param max_count Maximum entries to return
 * @return Number of files found, negative on error
 */
int ffs_list_files(ffs_file_info_t* out, int max_count);

/**
 * @brief Printf-style append to log file
 * @param file_id File ID
 * @param fmt Format string
 * @return Bytes written on success, negative on error
 */
int ffs_log_appendf(int file_id, const char* fmt, ...);

// =============================================================================
// INTERNAL/ADVANCED API
// =============================================================================

/**
 * @brief Serialize file table to flash storage
 * @return 0 on success, negative on error
 */
int ffs_serialize_table(void);

/**
 * @brief Load file table from flash storage
 * @return 0 on success, negative on error
 */
int ffs_load_table(void);

/**
 * @brief Initialize empty file table
 * @return 0 on success, negative on error
 */
int ffs_init_table(void);

/**
 * @brief Resolve block number for file offset
 * @param entry File entry pointer
 * @param offset Byte offset within file
 * @return Block number containing offset, negative on error
 */
int ffs_resolve_block_for_offset(ffs_file_entry_t* entry, uint32_t offset);

/**
 * @brief Get current write block for file
 * @param file_id File ID
 * @return Block number, negative on error
 */
int ffs_get_current_write_block(int file_id);

/**
 * @brief Calculate actual file size by scanning blocks
 * @param file_id File ID
 * @return Actual size in bytes
 */
uint32_t ffs_calculate_actual_file_size(int file_id);

/**
 * @brief Recover file state after mount
 * @param file_id File ID
 */
void ffs_recover_file_state(int file_id);

/**
 * @brief Scan block for used bytes
 * @param block Block number
 * @return Number of used bytes
 */
uint32_t ffs_scan_block_used_bytes(uint32_t block);

/**
 * @brief Resolve cursor position to block and offset
 * @param entry File entry
 * @param cursor Cursor position
 * @param out_block Output block number
 * @param out_offset Output offset within block
 * @return 0 on success, negative on error
 */
int ffs_resolve_block_and_offset(const ffs_file_entry_t* entry, uint32_t cursor, int* out_block, uint32_t* out_offset);

/**
 * @brief Check if block address needs erasing
 * @param addr Address to check
 * @param len Length of data
 * @return true if erase needed
 */
bool block_needs_erase(uint32_t addr, uint32_t len);

/**
 * @brief Check if block index is reserved or metadata
 * @param idx Block index
 * @return true if reserved
 */
bool is_block_reserved_or_meta(uint32_t idx);

/**
 * @brief Calculate remaining space in file
 * @param file_id File ID
 * @return Remaining bytes available
 */
uint32_t ffs_remaining_space(int file_id);

// =============================================================================
// DEBUG & DIAGNOSTICS API  
// =============================================================================

/**
 * @brief Dump raw block data to console
 * @param block Block number to dump
 */
void debug_dump_block_raw(uint32_t block);

/**
 * @brief Dump block range to console
 * @param block Starting block number
 * @param offset Offset within block
 * @param length Number of bytes to dump
 */
void debug_dump_block_range(uint32_t block, uint32_t offset, uint32_t length);

/**
 * @brief Print file's block chain information
 * @param file_id File ID to analyze
 */
void ffs_debug_print_file_blocks(int file_id);

/**
 * @brief Check block table consistency
 */
void ffs_debug_check_block_table(void);

/**
 * @brief Rebuild block usage tracking (recovery)
 */
void ffs_rebuild_block_usage(void);

/**
 * @brief Reset file's block chain (recovery)
 * @param file_id File ID to reset
 */
void ffs_reset_file_chain(int file_id);

/**
 * @brief Rebuild all block links (recovery)
 */
void ffs_rebuild_block_links(void);


/**
 * @brief Get file's block chain (debug/internal use)
 * @param file_id File ID
 * @param out_blocks Output array for block numbers
 * @param max_blocks Maximum blocks to return
 * @return Number of blocks found, negative on error
 */
int ffs_get_file_blocks(int file_id, uint32_t* out_blocks, size_t max_blocks);

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

extern ffs_config_t ffs_config;
extern ffs_block_entry_t block_table[];

#ifdef __cplusplus
}
#endif

#endif /* __FFS_H */
