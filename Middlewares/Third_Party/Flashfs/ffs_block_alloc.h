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
 * @file    ffs_block_alloc.h
 * @brief   FlashFs - Block Allocation Management
 * @details Block allocation and wear leveling subsystem for FlashFs,
 *          providing efficient flash memory management with
 *          wear distribution and power-failure safe operations.
 */

#ifndef __FFS_BLOCK_ALLOC_H
#define __FFS_BLOCK_ALLOC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t next;           // 4 bytes
    uint8_t  in_use;         // 1 byte
    uint8_t  padding;        // 1 byte (optional padding to align)
    uint16_t valid_bytes;    // 2 bytes — NEW: total valid bytes in this block
    uint32_t erase_count;    // 4 bytes
} __attribute__((packed)) ffs_block_meta_t; // Total: 12 bytes

// Allocate a free block and return its index, or -1 if full
int ffs_alloc_block(void);

// Free a block by index
void ffs_free_block(uint32_t block_index);

// Initialize block table and mark reserved blocks as in use
void ffs_clear_block_table(void);

// Same, but preserves per-block erase counts (wear-leveling history).
// Used by format operations so wear data survives a format.
void ffs_clear_block_table_preserve_wear(void);

// Load block allocation metadata from flash storage
bool ffs_block_alloc_load(void);

// Save block allocation metadata to flash storage
bool ffs_block_alloc_save(void);

// Dump all block information in a formatted table for debugging
void ffs_dump_blocks(void);

// Get detailed information about a specific block and its chain
void ffs_get_block_info(uint32_t block_num);

// Save erase counts to RAM before format operation
bool ffs_save_erase_counts(void);

// Increment and Save erase counts to RAM before format operation
bool ffs_incr_save_erase_counts(void);

// Restore erase counts from RAM after format operation
bool ffs_restore_erase_counts(void);

// Reset saved erase counts
bool ffs_reset_erase_counts(void);

// Free saved erase count memory
void ffs_free_saved_erase_counts(void);

#ifdef __cplusplus
}
#endif

#endif /* __FFS_BLOCK_ALLOC_H */
