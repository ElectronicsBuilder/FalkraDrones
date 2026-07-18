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
 * @file    ffs_block_alloc.c
 * @brief   FlashFs - Block Allocation Implementation
 * @details Implementation of block allocation and wear leveling algorithms for
 *          FlashFs, providing efficient flash memory management
 *          with statistical wear distribution and power-failure recovery.
 */

#include "ffs_block_alloc.h"
#include "ffs.h"  // For ffs_config and block_table
#include "ffs_config.h"
#include <string.h>
#include <stdlib.h>

#ifndef LOG_INFO
#define LOG_INFO(...) FFS_LOG_SYSTEM_INFO(__VA_ARGS__)
#endif
#ifndef LOG_WARN
#define LOG_WARN(...) FFS_LOG_SYSTEM_WARN(__VA_ARGS__)
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(...) FFS_LOG_SYSTEM_ERROR(__VA_ARGS__)
#endif

#if BLOCK_ALLOC_USES_RNG
    // STM32 Hardware RNG support
    #if defined(STM32F7xx)
        #include "stm32f7xx.h"
    #elif defined(STM32F4xx)
        #include "stm32f4xx.h" 
    #elif defined(STM32H7xx)
        #include "stm32h7xx.h"
    #elif defined(STM32F1xx)
        #include "stm32f1xx.h"
    #elif defined(STM32F2xx)
        #include "stm32f2xx.h"
    #elif defined(STM32F3xx)
        #include "stm32f3xx.h"
    #elif defined(STM32L4xx)
        #include "stm32l4xx.h"
    #elif defined(STM32G4xx)
        #include "stm32g4xx.h"
    #else
        #warning "Unknown STM32 family - RNG may not be available"
    #endif
#else
    // Portable pseudo-random using standard C
    #include <stdlib.h>
    #include <time.h>
#endif

// === Random Number Generation Helpers ===

#if BLOCK_ALLOC_USES_RNG
// Hardware RNG initialization and generation
static bool ffs_rng_init(void) {
    #if defined(STM32F7xx) || defined(STM32F4xx) || defined(STM32H7xx) || defined(STM32L4xx) || defined(STM32G4xx)
        __HAL_RCC_RNG_CLK_ENABLE();
        RNG->CR |= RNG_CR_RNGEN;
        return true;
    #else
        // RNG not available on this STM32 family
        return false;
    #endif
}

static uint32_t ffs_get_random(void) {
    #if defined(STM32F7xx) || defined(STM32F4xx) || defined(STM32H7xx) || defined(STM32L4xx) || defined(STM32G4xx)
        // Wait for random data to be ready (with timeout)
        for (int attempts = 0; attempts < 100; ++attempts) {
            if (RNG->SR & RNG_SR_DRDY) {
                return RNG->DR;
            }
        }
        // Fallback to simple pseudo-random if RNG fails
        return (uint32_t)(HAL_GetTick() * 1103515245U + 12345U);
    #else
        // Fallback for STM32 families without RNG
        return (uint32_t)(HAL_GetTick() * 1103515245U + 12345U);
    #endif
}

#else
// Software pseudo-random generation
static bool ffs_rng_init(void) {
    static bool initialized = false;
    if (!initialized) {
        // Simple initialization using system tick or similar
        // In embedded systems without time(), use a system counter
        #ifdef HAL_GetTick
            srand((unsigned int)HAL_GetTick());
        #else
            // Fallback initialization
            srand(1); 
        #endif
        initialized = true;
    }
    return true;
}

static uint32_t ffs_get_random(void) {
    return (uint32_t)rand();
}
#endif

void ffs_free_block(uint32_t block_index) {
    if (block_index >= ffs_config.block_count)
        return;

    ffs_lock();
    block_table[block_index].in_use = false;
    block_table[block_index].next = FFS_BLOCK_EOF;
    block_table[block_index].valid_bytes = 0;
    ffs_unlock();
}

void ffs_clear_block_table(void) {
    memset(block_table, 0, sizeof(ffs_block_entry_t) * ffs_config.block_count);

    /* A cleared entry must terminate chains: next=0 would alias block 0 and
       let a chain walk on unknown metadata cross-link into real data. */
    for (uint32_t i = 0; i < ffs_config.block_count; i++) {
        block_table[i].next = FFS_BLOCK_EOF;
    }

    block_table[ffs_config.header_block].in_use = true;
    block_table[ffs_config.table_block].in_use = true;

    for (uint32_t i = 0; i < ffs_config.reserved_blocks; i++) {
        uint32_t b = ffs_config.reserved_start_block + i;
        if (b < ffs_config.block_count)
            block_table[b].in_use = true;
    }
}

void ffs_clear_block_table_preserve_wear(void) {
    /* Same as ffs_clear_block_table() but keeps erase_count so format
       operations no longer destroy wear-leveling history (previously worked
       around with a heap-allocated save/restore dance). */
    for (uint32_t i = 0; i < ffs_config.block_count; i++) {
        block_table[i].next        = FFS_BLOCK_EOF;
        block_table[i].in_use      = 0;
        block_table[i].padding     = 0;
        block_table[i].valid_bytes = 0;
        /* erase_count intentionally preserved */
    }

    block_table[ffs_config.header_block].in_use = true;
    block_table[ffs_config.table_block].in_use = true;

    for (uint32_t i = 0; i < ffs_config.reserved_blocks; i++) {
        uint32_t b = ffs_config.reserved_start_block + i;
        if (b < ffs_config.block_count)
            block_table[b].in_use = true;
    }
}



static int ffs_alloc_block_impl(void);

int ffs_alloc_block(void) {
    ffs_lock();
    int r = ffs_alloc_block_impl();
    ffs_unlock();
    return r;
}

static int ffs_alloc_block_impl(void) {
    static bool rng_initialized = false;
    if (!rng_initialized) {
        rng_initialized = ffs_rng_init();
    }

    // Generate a random start index using portable RNG
    uint32_t random_value = ffs_get_random();
    uint32_t start = random_value % ffs_config.block_count;

    int best_idx = -1;
    uint32_t min_erase = UINT32_MAX;

    for (uint32_t i = 0; i < ffs_config.block_count; ++i) {
        uint32_t idx = (start + i) % ffs_config.block_count;

        // Skip reserved or meta blocks
        bool is_reserved =
            (idx == ffs_config.header_block) ||
            (idx == ffs_config.table_block) ||
            (idx >= ffs_config.reserved_start_block &&
             idx <  ffs_config.reserved_start_block + ffs_config.reserved_blocks) ||
            (idx >= ffs_config.blockmeta_start_block &&
             idx <  ffs_config.blockmeta_start_block + ffs_config.blockmeta_block_count);

        if (is_reserved || block_table[idx].in_use)
            continue;

        if (block_table[idx].erase_count < min_erase) {
            best_idx = (int)idx;
            min_erase = block_table[idx].erase_count;
        }
    }

    if (best_idx >= 0) {
        block_table[best_idx].in_use = true;
        block_table[best_idx].next = FFS_BLOCK_EOF;
        block_table[best_idx].valid_bytes = 0;  // Initialize valid_bytes to 0

            ffs_serialize_table(); 
            ffs_block_alloc_save();

        return best_idx;
    }

    return -1; // No available block
}

// =============================================================================
// BLOCK ALLOCATION PERSISTENCE
// =============================================================================

/**
 * @brief Load block allocation metadata from flash storage
 * @details Reads directly into block_table — no heap allocation. (The
 *          previous implementation malloc'd a 48KB staging buffer that
 *          intermittently failed on a ~50KB-free heap, silently losing all
 *          wear-leveling data; see .claude/ffs_review.md bug A1.)
 * @return true if valid metadata was loaded, false otherwise (block table
 *         is left in the cleared/initial state on failure)
 */
static bool ffs_block_alloc_load_impl(void);

bool ffs_block_alloc_load(void) {
    ffs_lock();
    bool r = ffs_block_alloc_load_impl();
    ffs_unlock();
    return r;
}

static bool ffs_block_alloc_load_impl(void) {
    const uint32_t block_size = ffs_config.block_size;
    const uint32_t total_blocks = ffs_config.block_count;
    const uint32_t meta_size = sizeof(ffs_block_entry_t) * total_blocks;

    if (ffs_is_mounted()) {
        FFS_LOG_SYSTEM_WARN("[FFS] Refusing to reload block metadata while filesystem is mounted");
        return false;
    }

    if (!ffs_config.read)
        return false;
    if (meta_size > block_size * ffs_config.blockmeta_block_count)
        return false;

    uint32_t meta_addr = ffs_config.blockmeta_start_block * block_size;
    if (ffs_config.read(ffs_config.context, meta_addr, block_table, meta_size) != 0) {
        /* Partial read leaves unknown state — reset to a safe empty table */
        ffs_clear_block_table();
        return false;
    }

    /* New/erased flash reads back all 0xFF: treat as "no metadata yet" */
    bool data_valid = false;
    for (uint32_t i = 0; i < total_blocks && !data_valid; ++i) {
        if (block_table[i].next != 0xFFFFFFFFu ||
            block_table[i].in_use != 0xFF ||
            block_table[i].valid_bytes != 0xFFFF ||
            block_table[i].erase_count != 0xFFFFFFFFu) {
            data_valid = true;
        }
    }

    if (!data_valid) {
        ffs_clear_block_table();
        return false;
    }

    return true;
}

/**
 * @brief Save block allocation metadata to flash storage
 * @details Programs directly from block_table — no heap allocation. The
 *          prog() implementations stage through a CPU-side page buffer, so
 *          no D-cache maintenance is required on block_table here.
 * @return true on success, false on failure
 */
static bool ffs_block_alloc_save_impl(void);

bool ffs_block_alloc_save(void) {
    ffs_lock();
    bool r = ffs_block_alloc_save_impl();
    ffs_unlock();
    return r;
}

static bool ffs_block_alloc_save_impl(void) {
    const uint32_t block_size = ffs_config.block_size;
    const uint32_t total_blocks = ffs_config.block_count;
    const uint32_t meta_size = sizeof(ffs_block_entry_t) * total_blocks;

    if (!ffs_config.prog || !ffs_config.erase)
        return false;
    if (meta_size > block_size * ffs_config.blockmeta_block_count)
        return false;

    /* Erase metadata blocks first, tracking their erase counts so the values
       written below include this erase cycle */
    for (uint32_t i = 0; i < ffs_config.blockmeta_block_count; ++i) {
        uint32_t block_idx = ffs_config.blockmeta_start_block + i;
        if (ffs_config.erase(ffs_config.context, block_idx) != 0)
            return false;
        if (block_idx < total_blocks)
            block_table[block_idx].erase_count++;
    }

    uint32_t meta_addr = ffs_config.blockmeta_start_block * block_size;
    return ffs_config.prog(ffs_config.context, meta_addr, block_table, meta_size) == 0;
}

/**
 * @brief Dump all block information in a formatted table
 * @details Displays a comprehensive overview of all blocks including their
 *          allocation status, next block pointer, valid bytes, and erase count.
 *          Useful for debugging and system monitoring. Uses LOG_INFO for output.
 */
static void ffs_dump_blocks_impl(void);

void ffs_dump_blocks(void) {
    ffs_lock();
    ffs_dump_blocks_impl();
    ffs_unlock();
}

static void ffs_dump_blocks_impl(void) {
    if (!ffs_is_mounted()) {
        FFS_LOG_SYSTEM_ERROR("[DUMPBLOCKS] Filesystem is not mounted");
        return;
    }

#if FFS_LOG_LEVEL > FFS_LOG_LEVEL_NONE
    #include "log.hpp"
#endif

    // Print header with divider
    LOG_INFO("");
    LOG_INFO("╔═══════════════════════════════════════════════════════════════════════╗");
    LOG_INFO("║                         FlashFs Block Status                          ║");
    LOG_INFO("╠═══════════════════════════════════════════════════════════════════════╣");
    LOG_INFO("║ Block │ Status │ Next  │ Valid Bytes │ Erase Count │      Type        ║");
    LOG_INFO("╠═══════╪════════╪═══════╪═════════════╪═════════════╪══════════════════╣");

    for (uint32_t i = 0; i < ffs_config.block_count; i++) {

        const ffs_block_meta_t *block = &block_table[i];

        // For data blocks with valid_bytes=0, scan actual flash to get real size
        uint16_t display_valid_bytes = block->valid_bytes;
        if (block->in_use && display_valid_bytes == 0) {
            // Check if this is a data block (not reserved/meta)
            bool is_data_block = true;
            if (i == ffs_config.header_block || i == ffs_config.table_block) {
                is_data_block = false;
            } else if (i >= ffs_config.reserved_start_block &&
                       i < ffs_config.reserved_start_block + ffs_config.reserved_blocks) {
                is_data_block = false;
            } else if (i >= ffs_config.blockmeta_start_block &&
                       i < ffs_config.blockmeta_start_block + ffs_config.blockmeta_block_count) {
                is_data_block = false;
            }

            // If it's a data block, scan flash for actual usage
            if (is_data_block) {
                display_valid_bytes = (uint16_t)ffs_scan_block_used_bytes(i);
            }
        }

        // Determine block type/status
        const char *type_str = "";
        const char *status_str = block->in_use ? " USED " : " FREE ";

        // Check for special block types
        if (i == ffs_config.header_block) {
            type_str = "Header Block    ";
        } else if (i == ffs_config.table_block) {
            type_str = "Table Block     ";
        } else if (i >= ffs_config.reserved_start_block &&
                   i < ffs_config.reserved_start_block + ffs_config.reserved_blocks) {
            type_str = "Reserved Block  ";
        } else if (i >= ffs_config.blockmeta_start_block &&
                   i < ffs_config.blockmeta_start_block + ffs_config.blockmeta_block_count) {
            type_str = "BlockMeta Block ";
        } else {
            type_str = block->in_use ? "Data Block      " : "Free Block      ";
        }

        // Format next block pointer (handle invalid/end cases)
        char next_str[8];
        if (block->next == 0xFFFFFFFF) {
            strcpy(next_str, " END ");
        } else if (block->next >= ffs_config.block_count) {
            strcpy(next_str, " INV ");
        } else {
            snprintf(next_str, sizeof(next_str), "%5lu", (unsigned long)block->next);
        }

        // Print block information with scanned valid_bytes if applicable
        LOG_INFO("║ %5lu │ %-6s │ %-5s │ %11u │ %11lu │ %s ║",
                (unsigned long)i,
                status_str,
                next_str,
                display_valid_bytes,
                (unsigned long)block->erase_count,
                type_str);
        if ((i & 0x1Du) == 0x1Du) {
            osDelay(1);
        }
    }

    // Print footer with summary statistics
    LOG_INFO("╚═══════╧════════╧═══════╧═════════════╧═════════════╧══════════════════╝");
    
    // Calculate and display summary statistics
    uint32_t used_blocks = 0, free_blocks = 0;
    uint32_t min_erase = UINT32_MAX, max_erase = 0;
    uint64_t total_erase = 0, total_valid_bytes = 0;
    
    for (uint32_t i = 0; i < ffs_config.block_count; i++) {
        const ffs_block_meta_t *block = &block_table[i];
        
        if (block->in_use) {
            used_blocks++;
            total_valid_bytes += block->valid_bytes;
        } else {
            free_blocks++;
        }
        
        if (block->erase_count < min_erase) min_erase = block->erase_count;
        if (block->erase_count > max_erase) max_erase = block->erase_count;
        total_erase += block->erase_count;
    }
    
    uint32_t avg_erase = ffs_config.block_count > 0 ? (uint32_t)(total_erase / ffs_config.block_count) : 0;
    
    LOG_INFO("");
    LOG_INFO(" Summary Statistics:");
    LOG_INFO("   • Total Blocks: %lu (Used: %lu, Free: %lu)", 
             (unsigned long)ffs_config.block_count, (unsigned long)used_blocks, (unsigned long)free_blocks);
    LOG_INFO("   • Total Valid Data: %lu bytes", (unsigned long)total_valid_bytes);
    LOG_INFO("   • Erase Count - Min: %lu, Max: %lu, Avg: %lu", 
             (unsigned long)min_erase, (unsigned long)max_erase, (unsigned long)avg_erase);
    LOG_INFO("   • Wear Level Spread: %lu (Lower is better)", (unsigned long)(max_erase - min_erase));
    LOG_INFO("");
}

/**
 * @brief Get detailed information about a specific block and its chain
 * @param block_num Block number to query (0 to block_count-1)
 * @details Displays block status, type, and follows the chain if it exists
 */
static void ffs_get_block_info_impl(uint32_t block_num);

void ffs_get_block_info(uint32_t block_num) {
    ffs_lock();
    ffs_get_block_info_impl(block_num);
    ffs_unlock();
}

static void ffs_get_block_info_impl(uint32_t block_num) {
    if (!ffs_is_mounted()) {
        FFS_LOG_SYSTEM_ERROR("[GETBLOCK] Filesystem is not mounted");
        return;
    }

    if (block_num >= ffs_config.block_count) {
        LOG_ERROR("[GETBLOCK] Invalid block number %lu (max: %lu)",
                  (unsigned long)block_num, (unsigned long)(ffs_config.block_count - 1));
        return;
    }

    LOG_INFO("");
    LOG_INFO("╔═══════════════════════════════════════════════════════════════════════╗");
    LOG_INFO("║                       Block Information                               ║");
    LOG_INFO("╚═══════════════════════════════════════════════════════════════════════╝");

    // Determine block type
    const char* block_type = "Data Block";
    if (block_num == ffs_config.header_block) {
        block_type = "Header Block";
    } else if (block_num == ffs_config.table_block) {
        block_type = "Table Block";
    } else if (block_num >= ffs_config.reserved_start_block &&
               block_num < ffs_config.reserved_start_block + ffs_config.reserved_blocks) {
        block_type = "Reserved Block";
    } else if (block_num >= ffs_config.blockmeta_start_block &&
               block_num < ffs_config.blockmeta_start_block + ffs_config.blockmeta_block_count) {
        block_type = "BlockMeta Block";
    }

    const ffs_block_meta_t *block = &block_table[block_num];

    LOG_INFO(" Block Number:   %lu", (unsigned long)block_num);
    LOG_INFO(" Block Type:     %s", block_type);
    LOG_INFO(" Status:         %s", block->in_use ? "USED" : "FREE");
    LOG_INFO(" Next Block:     %lu (0x%08lX)", (unsigned long)block->next, (unsigned long)block->next);
    LOG_INFO(" Valid Bytes:    %u bytes", block->valid_bytes);
    LOG_INFO(" Erase Count:    %lu", (unsigned long)block->erase_count);
    LOG_INFO(" Flash Address:  0x%08lX", (unsigned long)(block_num * ffs_config.block_size));

    // If block has a chain, follow it
    if (block->in_use && block->next != FFS_BLOCK_EOF && block->next < ffs_config.block_count) {
        LOG_INFO("");
        LOG_INFO("╔═══════════════════════════════════════════════════════════════════════╗");
        LOG_INFO("║                         Block Chain                                   ║");
        LOG_INFO("╠═══════╦════════╦═══════════════╦═════════════╦══════════════════════════╣");
        LOG_INFO("║ Block ║ Status ║  Valid Bytes  ║ Erase Count ║         Type             ║");
        LOG_INFO("╠═══════╬════════╬═══════════════╬═════════════╬══════════════════════════╣");

        uint32_t current = block->next;
        uint32_t chain_length = 1; // Already counted the head block
        uint32_t total_valid = block->valid_bytes;

        while (current != FFS_BLOCK_EOF && current < ffs_config.block_count && chain_length < 100) {
            const ffs_block_meta_t *chain_block = &block_table[current];

            // Determine chain block type
            const char* chain_type = "Data Block";
            if (current == ffs_config.header_block) {
                chain_type = "Header Block";
            } else if (current == ffs_config.table_block) {
                chain_type = "Table Block";
            } else if (current >= ffs_config.reserved_start_block &&
                       current < ffs_config.reserved_start_block + ffs_config.reserved_blocks) {
                chain_type = "Reserved Block";
            } else if (current >= ffs_config.blockmeta_start_block &&
                       current < ffs_config.blockmeta_start_block + ffs_config.blockmeta_block_count) {
                chain_type = "BlockMeta Block";
            }

            LOG_INFO("║ %5lu ║  %s  ║     %5u     ║    %7lu    ║ %-24s ║",
                     (unsigned long)current,
                     chain_block->in_use ? "USED" : "FREE",
                     chain_block->valid_bytes,
                     (unsigned long)chain_block->erase_count,
                     chain_type);

            total_valid += chain_block->valid_bytes;
            chain_length++;
            current = chain_block->next;
        }

        LOG_INFO("╚═══════╩════════╩═══════════════╩═════════════╩══════════════════════════╝");
        LOG_INFO("");
        LOG_INFO(" Chain Summary:");
        LOG_INFO("   • Chain Length:     %lu blocks", (unsigned long)chain_length);
        LOG_INFO("   • Total Valid Data: %lu bytes", (unsigned long)total_valid);

        if (chain_length >= 100) {
            LOG_WARN("   ⚠ Chain truncated at 100 blocks (possible loop detected)");
        }
    }

    LOG_INFO("");
}

// =============================================================================
// ERASE COUNT PRESERVATION FOR FORMAT OPERATIONS
// =============================================================================

/** @brief Static storage for erase counts during format operations */
static uint32_t *saved_erase_counts = NULL;
static uint32_t saved_block_count = 0;

/**
 * @brief Save erase counts to RAM before format operation
 * @details Allocates memory and copies all erase counts to preserve wear
 *          leveling data across format operations. Call before ffs_format().
 * @return true if successful, false if memory allocation failed
 */
bool ffs_save_erase_counts(void) {
    if (!block_table) {
        FFS_LOG_SYSTEM_INFO("[WEAR] No block table available to save erase counts");
        return false;
    }
    
    // Free any previously saved data
    ffs_free_saved_erase_counts();
    
    // Allocate memory for erase counts
    saved_block_count = ffs_config.block_count;
    saved_erase_counts = (uint32_t*)malloc(saved_block_count * sizeof(uint32_t));
    
    if (!saved_erase_counts) {
        FFS_LOG_SYSTEM_INFO("[WEAR] Failed to allocate %lu bytes for erase count preservation", 
                           (unsigned long)(saved_block_count * sizeof(uint32_t)));
        saved_block_count = 0;
        return false;
    }
    
    // Copy erase counts
    for (uint32_t i = 0; i < saved_block_count; i++) {
        saved_erase_counts[i] = block_table[i].erase_count;
    }
    
    FFS_LOG_SYSTEM_INFO("[WEAR] Saved %lu block erase counts to RAM (%lu bytes)", 
                       (unsigned long)saved_block_count, 
                       (unsigned long)(saved_block_count * sizeof(uint32_t)));
    return true;
}



/**
 * @brief Increment and Save erase counts to RAM before format operation
 * @details Allocates memory and copies all erase counts to preserve wear
 *          leveling data across format operations. Call before ffs_format().
 * @return true if successful, false if memory allocation failed
 */
bool ffs_incr_save_erase_counts(void) {
    if (!block_table) {
        FFS_LOG_SYSTEM_INFO("[WEAR] No block table available to save erase counts");
        return false;
    }
    
    // Free any previously saved data
    ffs_free_saved_erase_counts();
    
    // Allocate memory for erase counts
    saved_block_count = ffs_config.block_count;
    saved_erase_counts = (uint32_t*)malloc(saved_block_count * sizeof(uint32_t));
    
    if (!saved_erase_counts) {
        FFS_LOG_SYSTEM_INFO("[WEAR] Failed to allocate %lu bytes for erase count preservation", 
                           (unsigned long)(saved_block_count * sizeof(uint32_t)));
        saved_block_count = 0;
        return false;
    }

    // Copy and increment erase counts
    for (uint32_t i = 0; i < saved_block_count; i++) {
        saved_erase_counts[i] = block_table[i].erase_count;
        saved_erase_counts[i]++;  // Increment erase count
    }
    
    FFS_LOG_SYSTEM_INFO("[WEAR] Saved %lu block erase counts to RAM (%lu bytes)", 
                       (unsigned long)saved_block_count, 
                       (unsigned long)(saved_block_count * sizeof(uint32_t)));
    return true;
}
/**
 * @brief Restore erase counts from RAM after format operation  
 * @details Restores previously saved erase counts back to the block table
 *          after format operations. Call after ffs_format() and filesystem
 *          initialization to preserve wear leveling data.
 * @return true if successful, false if no saved data available
 */
bool ffs_restore_erase_counts(void) {
    if (!saved_erase_counts || !block_table) {
        FFS_LOG_SYSTEM_INFO("[WEAR] No saved erase counts to restore");
        return false;
    }
    
    if (saved_block_count != ffs_config.block_count) {
        FFS_LOG_SYSTEM_INFO("[WEAR] Block count mismatch: saved=%lu, current=%lu", 
                           (unsigned long)saved_block_count, (unsigned long)ffs_config.block_count);
        return false;
    }
    
    // Restore erase counts
    uint32_t restored_count = 0;
    uint32_t total_erases = 0;
    
    for (uint32_t i = 0; i < saved_block_count; i++) {
        if (saved_erase_counts[i] > 0) {
            block_table[i].erase_count = saved_erase_counts[i];
            restored_count++;
            total_erases += saved_erase_counts[i];
        }
    }
    
    FFS_LOG_SYSTEM_INFO("[WEAR] Restored %lu erase counts (total: %lu erases across all blocks)", 
                       (unsigned long)restored_count, (unsigned long)total_erases);
    
    return true;
}


/**
 * @brief Reset erase counts 
 * @details Reset previously saved erase counts back to the block table 
 * @return true if successful, false if no saved data available
 */
bool ffs_reset_erase_counts(void) {
    ffs_lock();
    for (uint32_t i = 0; i < ffs_config.block_count; i++) {
        block_table[i].erase_count = 0;
    }
    ffs_unlock();   /* save/serialize below take the lock themselves */
    FFS_LOG_SYSTEM_INFO("[WEAR] Reset %lu erase counts", (unsigned long)FFS_DEFAULT_BLOCK_COUNT);

    if (!ffs_block_alloc_save()) {
        FFS_LOG_SYSTEM_ERROR("[WEAR] Failed to save reset erase counts to flash");
        return false;
    }

    if (ffs_serialize_table() != 0) {
        FFS_LOG_SYSTEM_ERROR("[WEAR] Failed to serialize file table");
        return false;
    }

    FFS_LOG_SYSTEM_INFO("[WEAR] Successfully reset %lu block erase counts to zero",
                       (unsigned long)ffs_config.block_count);

    return true;
}

/**
 * @brief Free saved erase count memory
 * @details Cleans up memory allocated for erase count preservation.
 *          Call after successful restore or when preservation is no longer needed.
 */
void ffs_free_saved_erase_counts(void) {
    if (saved_erase_counts) {
        free(saved_erase_counts);
        saved_erase_counts = NULL;
        saved_block_count = 0;
        FFS_LOG_SYSTEM_INFO("[WEAR] Freed saved erase count memory");
    }
}
