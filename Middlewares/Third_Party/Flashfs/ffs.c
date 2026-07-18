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
 * @file    ffs.c
 * @brief   FlashFs Implementation
 * @details Custom-designed filesystem implementation optimized for embedded flash
 *          storage in drone controller applications. Provides reliable file operations
 *          with wear leveling, power-failure recovery, and efficient block management.
 * 
 * A lightweight filesystem for embedded flash storage providing:
 * - Block-based allocation with wear leveling
 * - Text and binary file support  
 * - Persistent logging capabilities
 * - Power-failure recovery
 * - Optimized for SPI/QSPI flash memory
 */

// =============================================================================
// INCLUDES & GLOBAL CONFIGURATION
// =============================================================================

#include "ffs.h"
#include "ffs_util.h"
#include "ffs_config.h"
#include "ffs_block_alloc.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#if FFS_USE_CLEAN_DCACHE
#include "main.h"
#endif 

// =============================================================================
// INTERNAL DATA STRUCTURES & CONSTANTS
// =============================================================================

#define FFS_BLOCK_EOF           0xFFFFFFFFU
#define FFS_INVALID_BLOCK       0xFFFFFFFF

/** @brief Header structure for file table serialization */
typedef struct __attribute__((packed)) {
    uint32_t magic;             ///< FFS_MAGIC signature
    uint32_t count;             ///< Number of file entries
    uint32_t crc;               ///< CRC32 of file table
} ffs_table_header_t;

/** @brief Block data header (currently unused but reserved) */
typedef struct __attribute__((packed)) {
    uint32_t next_block;        ///< Next block in chain
    uint16_t data_len;          ///< Data length in block
    uint16_t reserved;          ///< Reserved field
    uint8_t  data[];            ///< Data payload
} ffs_block_header_t;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

/** @brief File table - tracks all open files */
ffs_file_entry_t file_table[FFS_MAX_FILES];

/** @brief Block table - tracks all flash blocks */
ffs_block_entry_t block_table[FFS_DEFAULT_BLOCK_COUNT];

/** @brief Global filesystem configuration */
ffs_config_t ffs_config = {
    .block_size = FFS_DEFAULT_BLOCK_SIZE,
    .block_count = FFS_DEFAULT_BLOCK_COUNT,
    .context = NULL,
    .read = NULL,
    .prog = NULL,
    .erase = NULL,
    .eraseChip = NULL,
    .sync = NULL,
    .init = NULL,
};

static bool ffs_mounted = false;

#if FFS_LOG_LEVEL >= FFS_LOG_LEVEL_DEBUG && FFS_LOG_WRITE_OPS
/** @brief Write operation counter for debug logging */
uint32_t write_line_count = 0;
#endif 

// =============================================================================
// THREAD SAFETY (FFS_USE_LOCKING)
// =============================================================================

#if FFS_USE_LOCKING
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

/** Recursive so public API functions can call each other while holding it.
 *  Statically allocated — no heap. Created lazily on first use. */
static SemaphoreHandle_t ffs_mutex = NULL;
static StaticSemaphore_t ffs_mutex_storage;

void ffs_lock(void) {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return;   /* nothing else can run yet — no lock needed */
    if (ffs_mutex == NULL) {
        taskENTER_CRITICAL();
        if (ffs_mutex == NULL) {
            ffs_mutex = xSemaphoreCreateRecursiveMutexStatic(&ffs_mutex_storage);
        }
        taskEXIT_CRITICAL();
    }
    (void)xSemaphoreTakeRecursive(ffs_mutex, portMAX_DELAY);
}

void ffs_unlock(void) {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return;
    if (ffs_mutex != NULL) {
        (void)xSemaphoreGiveRecursive(ffs_mutex);
    }
}

bool ffs_lock_held_by_current_task(void) {
    if (ffs_mutex == NULL)
        return false;
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return false;
    return xSemaphoreGetMutexHolder(ffs_mutex) == (void*)xTaskGetCurrentTaskHandle();
}
#else
void ffs_lock(void)   {}
void ffs_unlock(void) {}
bool ffs_lock_held_by_current_task(void) { return false; }
#endif /* FFS_USE_LOCKING */

// =============================================================================
// PRIVATE HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Erase a single block with erase count tracking
 * @param block Block number to erase
 * @return 0 on success, negative on error
 */
static inline int ffs_erase_block(uint32_t block) {
    int result = ffs_config.erase(ffs_config.context, block);
    
    // Update erase count if successful and block table is available
    if (result == 0 && block_table && block < ffs_config.block_count) {
        block_table[block].erase_count++;
        FFS_LOG_WRITE_DEBUG("[WEAR] Block %lu erased, count now: %lu", 
                           (unsigned long)block, (unsigned long)block_table[block].erase_count);
    }
    
    return result;
}

/**
 * @brief Erase block only if it contains non-FF data
 * @param block Block number to check and erase
 * @return 0 on success (block was clean or erased), negative on error
 */
static int ffs_erase_block_if_needed(uint32_t block) {
    if (block >= ffs_config.block_count)
        return -1;

    uint32_t addr = block * ffs_config.block_size;
    uint8_t buf[64];   /* stack, not static — must be reentrant (A12) */

    for (uint32_t offset = 0; offset < ffs_config.block_size; offset += sizeof(buf)) {
        if (ffs_config.read(ffs_config.context, addr + offset, buf, sizeof(buf)) != 0)
            return -2;

        for (uint32_t i = 0; i < sizeof(buf); ++i) {
            if (buf[i] != 0xFF) {
                // Not clean — erase by block index
                return ffs_erase_block(block);
            }
        }
    }

    return 0; // Already clean
}

/**
 * @brief Scan block to determine actual data size
 * @param block Block number to scan
 * @param max_scan_bytes Maximum bytes to scan
 * @return Number of used bytes in block
 */
static uint32_t ffs_scan_block_for_actual_size(uint32_t block, uint32_t max_scan_bytes) {
    uint32_t block_addr = block * ffs_config.block_size;
    uint32_t scan_size = (max_scan_bytes < ffs_config.block_size) ? max_scan_bytes : ffs_config.block_size;
    
    // Read the block data
    uint8_t scan_buffer[256];  /* stack, not static — must be reentrant (A12) */
    uint32_t bytes_found = 0;
    
    // Scan in chunks to handle large blocks
    for (uint32_t offset = 0; offset < scan_size; offset += sizeof(scan_buffer)) {
        uint32_t chunk_size = sizeof(scan_buffer);
        if (offset + chunk_size > scan_size) {
            chunk_size = scan_size - offset;
        }
        
        if (ffs_config.read(ffs_config.context, block_addr + offset, scan_buffer, chunk_size) != 0) {
            FFS_LOG_FILE_INFO("Failed to read block %u at offset %u", block, offset);
            break;
        }
        
        // Find the last non-erased byte in this chunk
        for (uint32_t i = 0; i < chunk_size; i++) {
            if (scan_buffer[i] != 0xFF) {
                bytes_found = offset + i + 1; // +1 because we want the count, not index 
            }
        }
    }
    
    FFS_LOG_FILE_INFO("Scanned block %u: found %u actual bytes", block, bytes_found);
    return bytes_found;
}

// =============================================================================
// CORE FILESYSTEM API
// =============================================================================

static bool ffs_mount_impl(const ffs_config_t* cfg);

bool ffs_mount(const ffs_config_t* cfg) {
    ffs_lock();
    bool r = ffs_mount_impl(cfg);
    ffs_unlock();
    return r;
}

bool ffs_is_mounted(void) {
    ffs_lock();
    bool r = ffs_mounted;
    ffs_unlock();
    return r;
}

static bool ffs_mount_impl(const ffs_config_t* cfg) {
    if (ffs_mounted)
        return true;

    if (!cfg || !cfg->read || !cfg->prog || !cfg->erase || !cfg->eraseChip || !cfg->init)
        return false;
    if (cfg->block_size != FFS_DEFAULT_BLOCK_SIZE)
        return false;
    if (cfg->block_count == 0 || cfg->block_count > FFS_DEFAULT_BLOCK_COUNT)
        return false;
    if (cfg->max_files == 0 || cfg->max_files > FFS_MAX_FILES)
        return false;
    if (cfg->max_filename_len == 0 || cfg->max_filename_len > FFS_MAX_FILENAME)
        return false;
    if (cfg->header_block >= cfg->block_count || cfg->table_block >= cfg->block_count)
        return false;
    if (cfg->reserved_start_block + cfg->reserved_blocks > cfg->block_count)
        return false;
    if (cfg->blockmeta_start_block + cfg->blockmeta_block_count > cfg->block_count)
        return false;
    if (cfg->blockmeta_start_block + cfg->blockmeta_block_count > cfg->reserved_start_block)
        return false;
    if (cfg->table_addr != cfg->table_block * cfg->block_size ||
        cfg->header_addr != cfg->header_block * cfg->block_size)
        return false;

    ffs_mounted = false;
    memcpy(&ffs_config, cfg, sizeof(ffs_config_t));
    memset(file_table, 0, sizeof(file_table));

    ffs_clear_block_table();

    if (ffs_load_table() != 0) {
        /* No valid file table (virgin flash or corruption): leave everything
           cleared and report failure so the caller can decide to format.
           (Previously this returned true, leaving RAM state inconsistent
           with flash — the source of the "every create fails" wedge.) */
        memset(file_table, 0, sizeof(file_table));
        return false;
    }

    /* Order matters: load persisted block metadata FIRST (erase counts,
       valid_bytes, chain links), THEN reconcile in_use flags from the file
       table, which is the source of truth. The previous order let a stale
       flash copy overwrite the reconciled state (bug A2). */
    if (!ffs_block_alloc_load()) {
        FFS_LOG_SYSTEM_ERROR("[FFS] Block metadata load failed - wear/chain data reset, rebuilding from file table");
    }
    ffs_rebuild_block_usage();

    ffs_mounted = true;
    return true;
}

static int ffs_format_impl(void);

int ffs_format(void) {
    ffs_lock();
    int r = ffs_format_impl();
    ffs_unlock();
    return r;
}

static int ffs_format_impl(void) {
    ffs_mounted = false;

    if (!ffs_config.eraseChip)
        return -1;
    if (ffs_config.eraseChip(ffs_config.context) != 0)
        return -1;

    memset(file_table, 0, sizeof(file_table));

    /* Clear allocation state but KEEP wear history, then account for the
       chip-erase cycle every block just went through. (Previously the clear
       ran after the increment and zeroed the counts, which is why callers
       had a heap-based save/restore dance around ffs_format.) */
    ffs_clear_block_table_preserve_wear();
    for (uint32_t i = 0; i < ffs_config.block_count; i++) {
        block_table[i].erase_count++;
    }

    if (!ffs_block_alloc_save())
        return -2;
    if (ffs_serialize_table() != 0)
        return -3;
    return 0;
}

int ffs_init(void) {
    if (!ffs_config.init)
        return -1;
    ffs_config.init(ffs_config.context);
    return 0;
}

static int ffs_format_block_impl(void);

int ffs_format_block(void) {
    ffs_lock();
    int r = ffs_format_block_impl();
    ffs_unlock();
    return r;
}

static int ffs_format_block_impl(void) {
    for (uint32_t i = 0; i < ffs_config.block_count; i++) {
         if (ffs_erase_block(i) != 0)  return -1;   /* increments erase_count itself */
    }
    memset(file_table, 0, sizeof(file_table));
    ffs_clear_block_table_preserve_wear();

    if (!ffs_block_alloc_save())
        return -2;
    if (ffs_serialize_table() != 0)
        return -3;
    return 0;
}

static int ffs_create_impl(const char* name, uint32_t size);

int ffs_create(const char* name, uint32_t size) {
    ffs_lock();
    int r = ffs_create_impl(name, size);
    ffs_unlock();
    return r;
}

static int ffs_create_impl(const char* name, uint32_t size) {
    if (!name)
        return -4;

    size_t name_len = strnlen(name, ffs_config.max_filename_len);
    if (name_len == 0 || name_len >= ffs_config.max_filename_len)
        return -6;

    for (uint32_t i = 0; i < ffs_config.max_files; i++) {
        if (file_table[i].in_use &&
            strncmp(file_table[i].name, name, ffs_config.max_filename_len) == 0) {
            return -5;
        }
    }

    for (uint32_t i = 0; i < ffs_config.max_files; i++) {
        if (!file_table[i].in_use) {
            memset(&file_table[i], 0x00, sizeof(ffs_file_entry_t));

            strncpy(file_table[i].name, name, ffs_config.max_filename_len - 1);
            file_table[i].name[ffs_config.max_filename_len - 1] = '\0';

            int block = ffs_alloc_block();
            if (block < 0)
                return -2;

            //ERASE the block if needed before assigning to the file
            if (ffs_erase_block_if_needed(block) != 0)
                return -3;

            file_table[i].head_block = block;
            file_table[i].tail_block = block;
            file_table[i].reserved   = (size > 0) ? size : ffs_config.block_size;
            file_table[i].offset     = block * ffs_config.block_size;
            file_table[i].size       = 0;
            file_table[i].cursor     = 0;
            file_table[i].in_use     = true;

            FFS_LOG_FILE_INFO("Created file '%s' at block %u", file_table[i].name, block);

            ffs_serialize_table();
            return i;
        }
    }
    return -1;
}

int ffs_open(const char* name) {
    ffs_lock();
    int r = -1;
    for (uint32_t i = 0; i < ffs_config.max_files; i++) {
        if (file_table[i].in_use && strncmp(file_table[i].name, name, ffs_config.max_filename_len) == 0) {
            r = (int)i;
            break;
        }
    }
    ffs_unlock();
    return r;
}

static int ffs_write_impl(int file_id, const void* data, uint32_t len);

int ffs_write(int file_id, const void* data, uint32_t len) {
    ffs_lock();
    int r = ffs_write_impl(file_id, data, len);
    ffs_unlock();
    return r;
}

static int ffs_write_impl(int file_id, const void* data, uint32_t len) {
    if (file_id < 0 || (uint32_t)file_id >= ffs_config.max_files || !file_table[file_id].in_use)
        return -1;

    ffs_file_entry_t* entry = &file_table[file_id];
    if (entry->cursor != entry->size)
        return FFS_ERR_NOT_APPEND;

    uint32_t block_size = ffs_config.block_size;
    uint32_t remaining = len;
    const uint8_t* ptr = (const uint8_t*)data;

    while (remaining > 0) {
        uint32_t current_block = entry->tail_block;
        uint32_t block_offset = block_table[current_block].valid_bytes;
        uint32_t block_addr   = current_block * block_size + block_offset;
        uint32_t space        = block_size - block_offset;

        FFS_LOG_WRITE_DEBUG("Write loop %u: block=%u, offset=%u, remaining=%u, space=%u, cursor=%u",
                            write_line_count, current_block, block_offset, remaining, space, entry->cursor);
#if FFS_LOG_LEVEL >= FFS_LOG_LEVEL_DEBUG && FFS_LOG_WRITE_OPS
        ++write_line_count;
#endif 
        if (space == 0) {
            int new_block = ffs_alloc_block();
            if (new_block < 0) return -2;
            if (ffs_erase_block_if_needed(new_block) != 0) return -4;

            block_table[current_block].next = new_block;
            entry->tail_block = new_block;

            block_table[new_block].next = FFS_BLOCK_EOF;  // Ensure proper termination
            block_table[new_block].valid_bytes = 0;      // Initialize valid bytes to 0

            ffs_block_alloc_save();
            ffs_serialize_table();
            
            continue;
        }

        uint32_t to_write = (remaining < space) ? remaining : space;
        if (ffs_config.prog(ffs_config.context, block_addr, ptr, to_write) != 0)
            return -3;

        ptr += to_write;
        entry->cursor += to_write;
        entry->size   += to_write;
        remaining     -= to_write;

        if (block_table[current_block].valid_bytes < block_offset + to_write) {
            block_table[current_block].valid_bytes = block_offset + to_write;
            
        }
    }

    return len;
}

static int ffs_read_impl(int file_id, void* buffer, uint32_t len);

int ffs_read(int file_id, void* buffer, uint32_t len) {
    ffs_lock();
    int r = ffs_read_impl(file_id, buffer, len);
    ffs_unlock();
    return r;
}

static int ffs_read_impl(int file_id, void* buffer, uint32_t len) {
    if (file_id < 0 || (uint32_t)file_id >= ffs_config.max_files || !file_table[file_id].in_use)
        return -1;

    ffs_file_entry_t* entry = &file_table[file_id];
    uint8_t* buf = (uint8_t*)buffer;
    uint32_t total_read = 0;
    uint32_t block_size = ffs_config.block_size;

    uint32_t remaining = entry->size - entry->cursor;
    if (len > remaining) len = remaining;
    if (len == 0) return 0;

    uint32_t cursor_pos = entry->cursor;
    uint32_t block = entry->head_block;

    while (block != FFS_BLOCK_EOF) {
        uint32_t valid_bytes = block_table[block].valid_bytes;
        if (cursor_pos < valid_bytes)
            break;
        if (valid_bytes == 0)
            return -2;
        cursor_pos -= valid_bytes;
        block = block_table[block].next;
    }

    while (len > 0 && block != FFS_BLOCK_EOF) {
        if (block >= ffs_config.block_count)
            return -2;

        uint32_t valid_bytes = block_table[block].valid_bytes;
        if (cursor_pos >= valid_bytes)
            return -2;

        uint32_t block_remain = valid_bytes - cursor_pos;
        uint32_t chunk = (len < block_remain) ? len : block_remain;

        int res = ffs_config.read(ffs_config.context,
                                  block * block_size + cursor_pos,
                                  buf, chunk);
        if (res != 0)
            return -3;

        buf += chunk;
        len -= chunk;
        total_read += chunk;
        entry->cursor += chunk;

        block = block_table[block].next;
        cursor_pos = 0;  // next block starts at offset 0
    }

    return total_read;
}

static int ffs_seek_impl(int file_id, int32_t offset, int start);

int ffs_seek(int file_id, int32_t offset, int start) {
    ffs_lock();
    int r = ffs_seek_impl(file_id, offset, start);
    ffs_unlock();
    return r;
}

static int ffs_seek_impl(int file_id, int32_t offset, int start) {
    if (file_id < 0 || (uint32_t)file_id >= ffs_config.max_files || !file_table[file_id].in_use)
        return -1;

    ffs_file_entry_t *entry = &file_table[file_id];
    uint32_t new_pos;

    switch (start) {
        case FFS_SEEK_SET:
            if (offset < 0 || (uint32_t)offset > entry->size) return -2;
            new_pos = (uint32_t)offset;
            break;

        case FFS_SEEK_CUR:
            if ((offset < 0 && (uint32_t)(-offset) > entry->cursor) ||
                (offset > 0 && entry->cursor + (uint32_t)offset > entry->size))
                return -2;
            new_pos = entry->cursor + offset;
            break;

        case FFS_SEEK_END:
            if ((offset < 0 && (uint32_t)(-offset) > entry->size) || (offset > 0))
                return -2;
            new_pos = entry->size + offset;
            break;

        default:
            return -3;
    }

    entry->cursor = new_pos;
    return 0;
}

int32_t ffs_tell(int file_id) {
    ffs_lock();
    int32_t r = -1;
    if (file_id >= 0 && (uint32_t)file_id < ffs_config.max_files && file_table[file_id].in_use)
        r = (int32_t)file_table[file_id].cursor;
    ffs_unlock();
    return r;
}

static int ffs_delete_impl(const char* name);

int ffs_delete(const char* name) {
    ffs_lock();
    int r = ffs_delete_impl(name);
    ffs_unlock();
    return r;
}

static int ffs_delete_impl(const char* name) {
    for (uint32_t i = 0; i < ffs_config.max_files; i++) {
        if (file_table[i].in_use && strncmp(file_table[i].name, name, ffs_config.max_filename_len) == 0) {
            // Free all blocks in the file's chain before removing from table
            uint32_t block = file_table[i].head_block;
            FFS_LOG_CHAIN_INFO("Deleting file '%s' - freeing block chain starting at %u", name, block);
            
            while (block != FFS_BLOCK_EOF) {
                uint32_t next = block_table[block].next;
                FFS_LOG_CHAIN_INFO("Freeing block %u (next=%u)", block, next);
                
                // Erase the block if needed and mark as free
                ffs_erase_block_if_needed(block);
                ffs_free_block(block);
                
                block = next;
            }
            
            // Now clear the file table entry
            file_table[i].in_use = false;
            file_table[i].cursor = 0;
            file_table[i].size = 0;
            file_table[i].head_block = FFS_BLOCK_EOF;
            file_table[i].tail_block = FFS_BLOCK_EOF;

            ffs_block_alloc_save();
            ffs_serialize_table();
            return 0;
        }
    }
    return -1;
}

// =============================================================================
//  API
// =============================================================================

int ffs_open_or_create_reset(const char* name, uint32_t reserve_size) {
    ffs_lock();   /* hold across the whole compound op (recursive lock) */
    int id = ffs_open(name);
    if (id >= 0) {
        ffs_reset_file_chain(id);
    } else {
        id = ffs_create(name, reserve_size);
    }
    ffs_unlock();
    return id;
}

int ffs_open_log(const char* name) {
    ffs_lock();
    int id = ffs_open(name);
    if (id >= 0) {
        ffs_recover_file_state(id);
    } else {
        // Create with unlimited growth (reserved=0) or a cap you choose
        id = ffs_create(name, 0);
        if (id >= 0) {
            // Fresh file: ensure tail/head are valid and cursor=0
            ffs_recover_file_state(id);
        }
    }
    ffs_unlock();
    return id;
}

int ffs_open_binary(const char* name) {
    ffs_lock();
    int id = ffs_open(name);
    if (id >= 0) {
        ffs_recover_file_state(id);
        // For binary files, seek to end for appending
        ffs_seek(id, 0, FFS_SEEK_END);
    } else {
        // Create with unlimited growth (reserved=0)
        id = ffs_create(name, 0);
        if (id >= 0) {
            // Fresh file: ensure tail/head are valid and cursor=0
            ffs_recover_file_state(id);
        }
    }
    ffs_unlock();
    return id;
}

int ffs_write_line(int file_id, const char* line) {
    if (!line) return -1;

    size_t len = strlen(line);
    if (len == 0) return -1;   /* A7: empty string would read line[-1] below */
    bool needs_newline = (line[len - 1] != '\n');

    if (len + 1 >= 256) return -2;

    char temp[256];
    strcpy(temp, line);
    if (needs_newline) {
        temp[len++] = '\n';
        temp[len] = '\0';
    }

    if (len > ffs_config.block_size) return -3;

    /* ffs_write() takes the lock */
    return ffs_write(file_id, temp, len);
}

static int ffs_read_line_impl(int file_id, char* buf, uint32_t max_len);

int ffs_read_line(int file_id, char* buf, uint32_t max_len) {
    ffs_lock();
    int r = ffs_read_line_impl(file_id, buf, max_len);
    ffs_unlock();
    return r;
}

static int ffs_read_line_impl(int file_id, char* buf, uint32_t max_len) {
    if (file_id < 0 || (uint32_t)file_id >= ffs_config.max_files || !file_table[file_id].in_use)
        return -1;

    ffs_file_entry_t* entry = &file_table[file_id];
    if (!buf || max_len == 0)
        return -1;

    uint32_t block_size = ffs_config.block_size;
    size_t i = 0;

    while (entry->cursor < entry->size && i < (max_len - 1)) {
        int block = ffs_resolve_block_for_offset(entry, entry->cursor);
        if (block < 0) return -2;

        uint32_t offset = entry->cursor;
        uint32_t walk = 0;
        int blk = entry->head_block;

        // Walk again to get correct offset within block (matching resolve_block behavior)
        while (blk != block && blk != FFS_BLOCK_EOF) {
            walk += block_table[blk].valid_bytes;
            blk = block_table[blk].next;
        }
        offset -= walk;

        // Don't read past valid_bytes
        if (offset >= block_table[block].valid_bytes)
            break;

        uint32_t addr = block * block_size + offset;
        uint8_t ch;

        if (ffs_config.read(ffs_config.context, addr, &ch, 1) != 0)
            return -2;

        // Skip padding 0xFF/0x00
        if (ch == 0xFF || ch == 0x00) {
            do {
                entry->cursor++;
                if (entry->cursor >= entry->size) break;

                block = ffs_resolve_block_for_offset(entry, entry->cursor);
                if (block < 0) return -2;

                offset = entry->cursor;
                walk = 0;
                blk = entry->head_block;
                while (blk != block && blk != FFS_BLOCK_EOF) {
                    walk += block_table[blk].valid_bytes;
                    blk = block_table[blk].next;
                }
                offset -= walk;

                if (offset >= block_table[block].valid_bytes)
                    break;

                addr = block * block_size + offset;
                if (ffs_config.read(ffs_config.context, addr, &ch, 1) != 0)
                    return -2;
            } while (ch == 0xFF || ch == 0x00);

            if (entry->cursor >= entry->size)
                break;
        }

        buf[i++] = ch;
        entry->cursor++;

        if (ch == '\n') {
        FFS_LOG_READ_DEBUG("[read] Addr: %08lX Block: %d Offset: %lu Cursor: %lu",
                     (unsigned long)addr,
                     block,
                     (unsigned long)offset,
                     (unsigned long)entry->cursor);
            break;
        }
    }

    buf[i] = '\0';
    return (int)i;
}

bool ffs_file_exists(const char* name) {
    return ffs_find_file(name) >= 0;
}

int ffs_find_file(const char* name) {
    ffs_lock();
    int r = -1;
    for (uint32_t i = 0; i < ffs_config.max_files; i++) {
        if (file_table[i].in_use && strncmp(file_table[i].name, name, ffs_config.max_filename_len) == 0) {
            r = (int)i;
            break;
        }
    }
    ffs_unlock();
    return r;
}

int ffs_list_files(ffs_file_info_t* out, int max_count) {
    ffs_lock();
    int count = 0;
    for (uint32_t i = 0; i < ffs_config.max_files && count < max_count; i++) {
        if (file_table[i].in_use) {
            strncpy(out[count].name, file_table[i].name, ffs_config.max_filename_len);
            out[count].size = file_table[i].size;
            count++;
        }
    }
    ffs_unlock();
    return count;
}

int ffs_log_appendf(int file_id, const char* fmt, ...) {
    if (file_id < 0 || (uint32_t)file_id >= ffs_config.max_files || !file_table[file_id].in_use)
        return -1;

    const uint32_t bs = ffs_config.block_size;
    char linebuf[512];   /* stack, not static — must be reentrant (A12) */
    char *buf = linebuf;
    uint32_t bufcap = (sizeof(linebuf) < bs) ? (uint32_t)sizeof(linebuf) : bs;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, bufcap, fmt, ap);
    va_end(ap);
    if (n < 0) return -2;

    // Ensure newline
    if ((uint32_t)n + 1 >= bufcap) return -3;    // would exceed block limit
    buf[n++] = '\n';
    buf[n]   = '\0';

    // Single atomic write for this record
    int wr = ffs_write(file_id, (const uint8_t*)buf, (uint32_t)n);
    return wr == (int)n ? wr : -4;
}

// =============================================================================
// INTERNAL/ADVANCED API
// =============================================================================

static int ffs_serialize_table_impl(void);

int ffs_serialize_table(void) {
    ffs_lock();
    int r = ffs_serialize_table_impl();
    ffs_unlock();
    return r;
}

static int ffs_serialize_table_impl(void) {
    if (!ffs_config.prog || !ffs_config.erase)
        return -1;

    uint32_t crc = ffs_crc32(0, file_table, sizeof(file_table));
    ffs_table_header_t header = {
        .magic = FFS_MAGIC,
        .count = ffs_config.max_files,
        .crc   = crc
    };

    ffs_erase_block(ffs_config.header_block);
    ffs_config.prog(ffs_config.context, ffs_config.header_addr, &header, sizeof(header));

    ffs_erase_block(ffs_config.table_block);
    ffs_config.prog(ffs_config.context, ffs_config.table_addr, file_table, sizeof(file_table));

#if FFS_USE_CLEAN_DCACHE
    #if defined(STM32F7xx) || defined(STM32F4xx)
        SCB_CleanDCache_by_Addr((uint32_t*)file_table, sizeof(file_table));
    #endif
#endif 

    return 0;
}

int ffs_load_table(void) {
    if (!ffs_config.read)
        return -1;

    ffs_table_header_t header = {0};
    ffs_config.read(ffs_config.context, ffs_config.header_addr, &header, sizeof(header));
    if (header.magic != FFS_MAGIC || header.count != ffs_config.max_files)
        return -1;

    ffs_config.read(ffs_config.context, ffs_config.table_addr, file_table, sizeof(file_table));

    uint32_t calc_crc = ffs_crc32(0, file_table, sizeof(file_table));
    if (calc_crc != header.crc)
        return -2;

    return 0;
}

int ffs_init_table(void) {
    if (!ffs_config.prog || !ffs_config.erase) return -1;

    memset(file_table, 0, sizeof(file_table));
    uint32_t crc = ffs_crc32(0, file_table, sizeof(file_table));

    ffs_table_header_t header = {
        .magic = FFS_MAGIC,
        .count = ffs_config.max_files,
        .crc   = crc
    };

    ffs_erase_block(ffs_config.header_block);
    ffs_erase_block(ffs_config.table_block);

    int res1 = ffs_config.prog(ffs_config.context, ffs_config.header_addr, &header, sizeof(header));
    if (res1 != 0) return -2;

    int res2 = ffs_config.prog(ffs_config.context, ffs_config.table_addr, file_table, sizeof(file_table));
    if (res2 != 0) return -3;

    return 0;
}

int ffs_resolve_block_for_offset(ffs_file_entry_t* entry, uint32_t offset) {
    int block = entry->head_block;
    uint32_t steps = 0;

    while (block != FFS_BLOCK_EOF) {
        if (block < 0 || (uint32_t)block >= ffs_config.block_count)
            return -1;
        if (++steps > ffs_config.block_count)
            return -1;

        uint16_t valid = block_table[block].valid_bytes;
        if (offset < valid)
            return block;

        offset -= valid;
        block = block_table[block].next;
    }

    return -1;  // Offset exceeds valid content
}

int ffs_get_current_write_block(int file_id) {
    ffs_lock();
    int r = -1;
    if (file_id >= 0 && (uint32_t)file_id < ffs_config.max_files && file_table[file_id].in_use)
        r = (int)file_table[file_id].tail_block;
    ffs_unlock();
    return r;
}

static uint32_t ffs_calculate_actual_file_size_impl(int file_id);

uint32_t ffs_calculate_actual_file_size(int file_id) {
    ffs_lock();
    uint32_t r = ffs_calculate_actual_file_size_impl(file_id);
    ffs_unlock();
    return r;
}

static uint32_t ffs_calculate_actual_file_size_impl(int file_id) {
    if (file_id < 0 || (uint32_t)file_id >= ffs_config.max_files || !file_table[file_id].in_use)
        return 0;

    ffs_file_entry_t* entry = &file_table[file_id];
    uint32_t total_size = 0;
    uint32_t block = entry->head_block;
    int block_count = 0;
    
    FFS_LOG_FILE_INFO("Calculating file size for '%s' (file_id=%d, head_block=%u, tail_block=%u)", 
                      entry->name, file_id, entry->head_block, entry->tail_block);
    
    // Check if head block is valid
    if (block >= ffs_config.block_count) {
        FFS_LOG_FILE_INFO("Invalid head block %u for file '%s'", block, entry->name);
        return 0;
    }
    
    // Walk through all blocks in the file's chain
    while (block != FFS_BLOCK_EOF) {
        // Validate block index is within range
        if (block >= ffs_config.block_count) {
            FFS_LOG_CHAIN_INFO("Invalid block %u in chain for file_id %d (max: %u)", 
                              block, file_id, ffs_config.block_count - 1);
            break;
        }
        
        uint16_t block_valid_bytes = block_table[block].valid_bytes;
        uint32_t next_block = block_table[block].next;
        uint32_t block_actual_size = 0;
        
        // Special case: single block file or last block with valid_bytes=0
        if (block_valid_bytes == 0 && (block == entry->head_block)) {
            // This could be a single block file where valid_bytes wasn't persisted
            // Scan the block to find actual data
            block_actual_size = ffs_scan_block_used_bytes(block);
            
            FFS_LOG_FILE_INFO("Block %u: scanned size=%u (valid_bytes was 0)", block, block_actual_size);
        } else if (block_valid_bytes == 0 && next_block == FFS_BLOCK_EOF) {
            // This is the last block in a chain but valid_bytes=0, scan it
            block_actual_size = ffs_scan_block_used_bytes(block);
            FFS_LOG_FILE_INFO("Block %u: scanned last block size=%u", block, block_actual_size);
        } else {
            // Use the stored valid_bytes value
            block_actual_size = block_valid_bytes;
        }
        
        // Add bytes from this block
        total_size += block_actual_size;
        
        block = next_block;
        
        // Safety check to prevent infinite loops on corrupted chains
        if (++block_count > ffs_config.block_count) {
            FFS_LOG_CHAIN_INFO("File size calculation stopped - chain too long for file_id %d (over %u blocks)", 
                              file_id, ffs_config.block_count);
            break;
        }
    }
    
    FFS_LOG_FILE_INFO("Final calculated file size for '%s': %lu bytes across %d blocks", 
                      entry->name, (unsigned long)total_size, block_count);
    return total_size;
}

static void ffs_recover_file_state_impl(int file_id);

void ffs_recover_file_state(int file_id) {
    ffs_lock();
    ffs_recover_file_state_impl(file_id);
    ffs_unlock();
}

static void ffs_recover_file_state_impl(int file_id) {
    ffs_file_entry_t* f = &file_table[file_id];
    const uint32_t bs = ffs_config.block_size;

    uint32_t size = 0;
    uint32_t block = f->head_block;
    uint32_t last_block = block;
    uint32_t steps = 0;

    while (block != FFS_BLOCK_EOF && block < ffs_config.block_count) {
        if (++steps > ffs_config.block_count) {
            FFS_LOG_SYSTEM_WARN("Recover stopped for '%s' - chain too long", f->name);
            break;
        }

        uint32_t used = block_table[block].valid_bytes;

        if (used == 0) {
            // Unknown/unsaved — scan the block to reconstruct
            used = ffs_scan_block_used_bytes(block);
            block_table[block].valid_bytes = (uint16_t)((used <= 0xFFFF) ? used : 0xFFFF);
        } else if (used > bs) {
            used = bs; // safety clamp
        }

        size += used;
        last_block = block;

        uint32_t next = block_table[block].next;
        if (next == block) {
            break; // corruption guard
        }
        if (next != FFS_BLOCK_EOF && next >= ffs_config.block_count) {
            FFS_LOG_SYSTEM_WARN("Recover stopped for '%s' - invalid next block %lu",
                                f->name, (unsigned long)next);
            break;
        }
        block = next;
    }

    f->size = size;
    f->tail_block = last_block;
    f->cursor = size; 
}

uint32_t ffs_scan_block_used_bytes(uint32_t block) {
    const uint32_t bs = ffs_config.block_size;
    const uint32_t addr0 = block * bs;

    // Scan forward in small chunks for the first 0xFF
    uint8_t buf[256];
    uint32_t pos = 0;

    while (pos < bs) {
        uint32_t chunk = (bs - pos > sizeof(buf)) ? sizeof(buf) : (bs - pos);
        if (ffs_config.read(ffs_config.context, addr0 + pos, buf, chunk) != 0)
            return 0; // conservative fallback

        for (uint32_t i = 0; i < chunk; ++i) {
            if (buf[i] == 0xFF) {
                // We found the first erased byte — return offset
                return pos + i;
            }
        }
        pos += chunk;
    }
    return bs; // fully used
}

int ffs_resolve_block_and_offset(const ffs_file_entry_t* entry, uint32_t cursor, int* out_block, uint32_t* out_offset) {
    // TODO:Resolve cursor position to block and offset
    return -1;
}

bool block_needs_erase(uint32_t addr, uint32_t len) {
    uint8_t temp[256];  // Adjust if len > 256 in rare cases
    if (len > sizeof(temp)) return true;  // Fallback to safe erase

    if (ffs_config.read(ffs_config.context, addr, temp, len) != 0)
        return true;

    for (uint32_t i = 0; i < len; i++) {
        if (temp[i] != 0xFF)
            return true;
    }
    return false;
}

bool is_block_reserved_or_meta(uint32_t idx) {
    if (idx >= ffs_config.reserved_start_block &&
        idx <  ffs_config.reserved_start_block + ffs_config.reserved_blocks)
        return true;

    if (idx >= ffs_config.blockmeta_start_block &&
        idx <  ffs_config.blockmeta_start_block + ffs_config.blockmeta_block_count)
        return true;

    return false;
}

uint32_t ffs_remaining_space(int file_id) {
    ffs_lock();
    uint32_t r = 0;
    if (file_id >= 0 && (uint32_t)file_id < ffs_config.max_files && file_table[file_id].in_use) {
        ffs_file_entry_t* entry = &file_table[file_id];
        if (entry->reserved == 0) {
            r = UINT32_MAX;   // Unlimited file size (log files typically)
        } else if (entry->size < entry->reserved) {
            r = entry->reserved - entry->size;
        }
    }
    ffs_unlock();
    return r;
}

// =============================================================================
// DEBUG & DIAGNOSTICS API
// =============================================================================

void debug_dump_block_raw(uint32_t block) {
    uint8_t buf[DEBUG_BLOCK_SIZE];  // Adjust size if needed
    uint32_t addr = block * ffs_config.block_size;
    if (ffs_config.read(ffs_config.context, addr, buf, sizeof(buf)) != 0) {
        FFS_LOG_SYSTEM_ERROR("Failed to read block %lu for debug", (unsigned long)block);
        return;
    }

    char line[128];
    for (size_t i = 0; i < sizeof(buf); i += 16) {
        snprintf(line, sizeof(line),
                 "%04X: %02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X",
                 (unsigned)(i),
                 buf[i], buf[i+1], buf[i+2], buf[i+3],
                 buf[i+4], buf[i+5], buf[i+6], buf[i+7],
                 buf[i+8], buf[i+9], buf[i+10], buf[i+11],
                 buf[i+12], buf[i+13], buf[i+14], buf[i+15]);
                 FFS_LOG_DEBUG_DUMP_INFO("%s", line); 
    }
}

void debug_dump_block_range(uint32_t block, uint32_t offset, uint32_t length) {
    if (offset + length > ffs_config.block_size) {
        FFS_LOG_SYSTEM_WARN("Invalid range: offset=%u, length=%u exceeds block size %u",
                            offset, length, ffs_config.block_size);
        return;
    }

    uint8_t buf[256];  // Safe upper bound
    uint32_t addr = block * ffs_config.block_size + offset;

    if (ffs_config.read(ffs_config.context, addr, buf, length) != 0) {
        FFS_LOG_SYSTEM_ERROR("Failed to read block %lu at offset %u", (unsigned long)block, offset);
        return;
    }

    char line[128];
    for (uint32_t i = 0; i < length; i += 16) {
        uint32_t line_len = (i + 16 <= length) ? 16 : (length - i);
        snprintf(line, sizeof(line),
                 "%04X: %02X %02X %02X %02X %02X %02X %02X %02X  "
                 "%02X %02X %02X %02X %02X %02X %02X %02X",
                 offset + i,
                 buf[i],   (line_len > 1  ? buf[i+1]  : 0),
                 (line_len > 2  ? buf[i+2]  : 0), (line_len > 3  ? buf[i+3]  : 0),
                 (line_len > 4  ? buf[i+4]  : 0), (line_len > 5  ? buf[i+5]  : 0),
                 (line_len > 6  ? buf[i+6]  : 0), (line_len > 7  ? buf[i+7]  : 0),
                 (line_len > 8  ? buf[i+8]  : 0), (line_len > 9  ? buf[i+9]  : 0),
                 (line_len > 10 ? buf[i+10] : 0), (line_len > 11 ? buf[i+11] : 0),
                 (line_len > 12 ? buf[i+12] : 0), (line_len > 13 ? buf[i+13] : 0),
                 (line_len > 14 ? buf[i+14] : 0), (line_len > 15 ? buf[i+15] : 0));
                 FFS_LOG_DEBUG_DUMP_INFO("%s", line); 
    }
}

static void ffs_debug_print_file_blocks_impl(int file_id);

void ffs_debug_print_file_blocks(int file_id) {
    ffs_lock();
    ffs_debug_print_file_blocks_impl(file_id);
    ffs_unlock();
}

static void ffs_debug_print_file_blocks_impl(int file_id) {
    if (file_id < 0 || file_id >= (int)ffs_config.max_files || !file_table[file_id].in_use) {
        FFS_LOG_SYSTEM_ERROR("Invalid file ID");
        return;
    }

    const ffs_file_entry_t* file = &file_table[file_id];
    FFS_LOG_SYSTEM_INFO("File '%s' uses blocks:", file->name);
    FFS_LOG_SYSTEM_INFO("  Size = %lu bytes, Cursor = %lu", (unsigned long)file->size, (unsigned long)file->cursor); 
    uint32_t block = file->head_block;
    int count = 0;
    while (block != FFS_BLOCK_EOF) {
        if (block >= ffs_config.block_count) {
            FFS_LOG_SYSTEM_ERROR("Invalid block index %u — possible corruption", block);
            break;
        }
        FFS_LOG_SYSTEM_INFO("  Block %u (next = %u)", block, block_table[block].next); 
        uint32_t next = block_table[block].next;
        if (next == block) {
            FFS_LOG_SYSTEM_ERROR("Self-referencing block %u — breaking", block);
            break;
        }

        block = next;
        if (++count > ffs_config.block_count) {
            FFS_LOG_SYSTEM_ERROR("Too many blocks — loop?");
            break;
        }
    }
}

void ffs_debug_check_block_table(void) {
    FFS_LOG_SYSTEM_INFO("=== [FFS] Verifying Block Table ===");
    int corruption_found = 0;

    for (uint32_t i = 0; i < ffs_config.block_count; ++i) {
        if (block_table[i].in_use && block_table[i].next == i) {
            FFS_LOG_SYSTEM_ERROR("Block %lu is self-referencing!", i);
            corruption_found++;
        }
        if (block_table[i].next >= ffs_config.block_count && block_table[i].next != FFS_BLOCK_EOF) {
            FFS_LOG_SYSTEM_ERROR("Block %lu has invalid .next = %u", i, block_table[i].next);
            corruption_found++;
        }
    }

    if (corruption_found == 0) {
        FFS_LOG_SYSTEM_INFO("Block table integrity check passed"); 
    } else {
        FFS_LOG_SYSTEM_WARN("Block table has %d corrupted entries", corruption_found);
    }
}

void ffs_rebuild_block_usage(void) {
    /* Reconcile allocation state with the file table (the source of truth).
       Persisted in_use flags can be stale — e.g. power loss between the file
       table save and the block metadata save — so first clear every
       non-reserved block's in_use flag (preserving erase_count, valid_bytes
       and next), then re-mark blocks that belong to live file chains.
       Blocks marked used in flash but referenced by no chain are thereby
       reclaimed instead of leaking. */
    for (uint32_t b = 0; b < ffs_config.block_count; ++b) {
        if (b == ffs_config.header_block || b == ffs_config.table_block)
            continue;
        if (is_block_reserved_or_meta(b))
            continue;
        block_table[b].in_use = false;
    }
    block_table[ffs_config.header_block].in_use = true;
    block_table[ffs_config.table_block].in_use = true;

    for (uint32_t i = 0; i < ffs_config.max_files; ++i) {
        if (!file_table[i].in_use)
            continue;

        uint32_t block = file_table[i].head_block;
        uint32_t count = 0;

        while (block != FFS_BLOCK_EOF) {
            if (block >= ffs_config.block_count)
                break;

            if (block_table[block].in_use) {
                // Already seen (cross-linked chain or loop) — stop here
                break;
            }

            block_table[block].in_use = true;

            uint32_t next = block_table[block].next;

            if (next == block) {
                // Self-referencing block — break to avoid loop
                break;
            }

            block = next;

            if (++count > ffs_config.block_count) {
                // Too many — avoid infinite loop on corruption
                break;
            }
        }
    }
}

static void ffs_reset_file_chain_impl(int file_id);

void ffs_reset_file_chain(int file_id) {
    ffs_lock();
    ffs_reset_file_chain_impl(file_id);
    ffs_unlock();
}

static void ffs_reset_file_chain_impl(int file_id) {
    if (file_id < 0 || (uint32_t)file_id >= ffs_config.max_files || !file_table[file_id].in_use)
        return;

    uint32_t block = file_table[file_id].head_block;
    uint32_t steps = 0;

    FFS_LOG_CHAIN_INFO("Resetting block chain for file '%s' (id=%d)", file_table[file_id].name, file_id);

    while (block != FFS_BLOCK_EOF) {
        if (block >= ffs_config.block_count) {
            FFS_LOG_SYSTEM_WARN("Reset stopped for '%s' - invalid block %lu",
                                file_table[file_id].name,
                                (unsigned long)block);
            break;
        }
        if (++steps > ffs_config.block_count) {
            FFS_LOG_SYSTEM_WARN("Reset stopped for '%s' - chain too long",
                                file_table[file_id].name);
            break;
        }

        uint32_t next = block_table[block].next;

        FFS_LOG_CHAIN_INFO("Erasing block %u (next=%u)", block, next);

        ffs_erase_block_if_needed(block);
        block_table[block].in_use = false;
        block_table[block].next = FFS_BLOCK_EOF;

        block = next;
    }

    // Allocate fresh block for clean start
    int new_block = ffs_alloc_block();
    if (new_block < 0) {
        FFS_LOG_SYSTEM_ERROR("Failed to allocate block for reset");
        return;
    }

    FFS_LOG_CHAIN_INFO("Allocated new head block %d for reset", new_block);

    file_table[file_id].head_block = new_block;
    file_table[file_id].tail_block = new_block;
    file_table[file_id].cursor     = 0;
    file_table[file_id].size       = 0;
    block_table[new_block].next = FFS_BLOCK_EOF;

    ffs_erase_block_if_needed(new_block);  // optional safety
    block_table[new_block].valid_bytes = 0;
}

void ffs_rebuild_block_links(void) {
    for (uint32_t i = 0; i < ffs_config.max_files; i++) {
        if (!file_table[i].in_use)
            continue;

        ffs_file_entry_t* file = &file_table[i];
        uint32_t remaining = file->size;
        uint32_t block = file->head_block;
        uint32_t block_size = ffs_config.block_size;

        if (block >= ffs_config.block_count)
            continue;

        uint32_t last_valid = block;

        FFS_LOG_CHAIN_INFO("Rebuilding chain for file '%s' (head block = %u, size = %lu)", file->name, block, (unsigned long)file->size);

        while (remaining > block_size && block != FFS_BLOCK_EOF) {
            bool found = false;

            for (uint32_t next = 0; next < ffs_config.block_count; next++) {
                if (next == block) continue;
                if (!block_table[next].in_use) continue;

                // Check if next block is used as a head by another file
                bool is_head_of_other = false;
                for (uint32_t j = 0; j < ffs_config.max_files; j++) {
                    if (j != i && file_table[j].in_use && file_table[j].head_block == next) {
                        is_head_of_other = true;
                        break;
                    }
                }
                if (is_head_of_other) continue;

                block_table[block].next = next;
                last_valid = next;

                FFS_LOG_CHAIN_INFO("  Linked block %u → %u", block, next);

                block = next;
                found = true;
                break;
            }

            if (!found) {
                FFS_LOG_CHAIN_INFO("  No further block found after block %u", block);
                break;
            }

            remaining -= block_size;
        }

        file->tail_block = last_valid;
        FFS_LOG_CHAIN_INFO("  Tail block for '%s' set to %u", file->name, last_valid);
    }
}


int ffs_get_file_blocks(int file_id, uint32_t* out_blocks, size_t max_blocks) {
    ffs_lock();
    if (file_id < 0 || (uint32_t)file_id >= ffs_config.max_files || !file_table[file_id].in_use) {
        ffs_unlock();
        return -1;
    }

    uint32_t block = file_table[file_id].head_block;
    int count = 0;
    while (block != FFS_BLOCK_EOF && block < ffs_config.block_count && count < (int)max_blocks) {
        out_blocks[count++] = block;
        block = block_table[block].next;
    }
    ffs_unlock();
    return count;
}
