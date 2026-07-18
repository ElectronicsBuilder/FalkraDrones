/**
 * @file    test_FileSystem.cpp
 * @brief   Flash FileSystem Test Implementation
 * @details Implementation of comprehensive test suite for Flash filesystem
 *          functionality validation
 * 
 * Part of FalkraController - STM32F767-based drone controller firmware
 * 
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
 */

#include "test_FileSystem.hpp"
#include "main.h"
#include "log.hpp"
#include "rtc.hpp"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "ffs.h"
#include "spi_flash_block_device.hpp"
#include "ffs_block_alloc.h"
#include "spi_flash.hpp"

// External references
extern SpiFlash spiFlash;
extern ffs_block_entry_t block_table[];
extern ffs_file_entry_t file_table[FFS_MAX_FILES];
extern ffs_config_t spi_flash_fs_config;

// Static state
//static bool ffs_initialized = false;

// =============================================================================
// INITIALIZATION HELPERS
// =============================================================================

// static void ffs_init_if_needed(void) {
//     if (!ffs_initialized) {
//         ffs_config = spi_flash_fs_config;
//         ffs_initialized = true;
//     }
// }

static bool ffs_mount_or_format(void) {
    ffs_init_if_needed();
    spiFlash.init();
    
    if (!ffs_mount(&spi_flash_fs_config)) {
        LOG_WARN("Mount failed, formatting...");
        if (ffs_format() != 0 || !ffs_mount(&spi_flash_fs_config)) {
            LOG_ERROR("Mount failed after format");
            return false;
        }
    }
    return true;
}

// =============================================================================
// BASIC FUNCTIONALITY TESTS
// =============================================================================

static void test_basic_write_read(void) {
    LOG_INFO("=== [FFS] Basic Write/Read Test ===");
    
    if (!ffs_mount_or_format()) return;

    const char* fname = "basic_test.txt";
    const char* sample = "This is a basic test of the FFS write and read logic.";
    char readback[128] = {0};

    int id = ffs_open_or_create_reset(fname, strlen(sample));
    if (id < 0) {
        LOG_ERROR("Failed to open/create %s", fname);
        return;
    }

    if (ffs_write(id, sample, strlen(sample)) < 0) {
        LOG_ERROR("Write failed for %s", fname);
        return;
    }

    ffs_seek(id, 0, FFS_SEEK_SET);
    if (ffs_read(id, readback, strlen(sample)) < 0) {
        LOG_ERROR("Read failed for %s", fname);
        return;
    }

    if (strncmp(readback, sample, strlen(sample)) == 0) {
        LOG_INFO("Basic test PASSED");
    } else {
        LOG_ERROR("Basic test FAILED");
        LOG_ERROR("Expected: %s", sample);
        LOG_ERROR("Got     : %s", readback);
    }

    LOG_INFO("=== [FFS] End Basic Write/Read Test ===");
}

static void test_text_log_file(void) {
    LOG_INFO("=== [FFS] Text Log File Test ===");
    
    const char* log_name = "logfile.txt";
    int log_id = ffs_open_or_create_reset(log_name, 512);
    if (log_id < 0) {
        LOG_ERROR("Failed to create %s", log_name);
        return;
    }

    const char* lines[] = {
        "[INFO ] Boot complete",
        "[WARN ] Voltage drop", 
        "[ERROR] Sensor failed",
        "[INFO ] Restart initiated"
    };

    // Write test lines
    for (int i = 0; i < 4; i++) {
        ffs_write_line(log_id, lines[i]);
    }
    ffs_serialize_table();

    // Read back and verify
    int read_id = ffs_open(log_name);
    ffs_seek(read_id, 0, FFS_SEEK_SET);
    
    char line[128];
    int line_count = 0;
    while (ffs_read_line(read_id, line, sizeof(line)) > 0) {
        LOG_INFO("[%d] %s", line_count, line);
        line_count++;
    }
    
    LOG_INFO("Text log test completed: %d lines", line_count);
    LOG_INFO("=== [FFS] End Text Log File Test ===");
}

static void test_binary_file(void) {
    LOG_INFO("=== [FFS] Binary File Test ===");

    const char* bin_name = "binary_test.dat";
    int bin_id = ffs_open_or_create_reset(bin_name, 256);
    if (bin_id < 0) {
        LOG_ERROR("Failed to open/create %s", bin_name);
        return;
    }

    // Create test pattern
    uint8_t pattern[256];
    for (int i = 0; i < 256; i++) {
        pattern[i] = (uint8_t)(i & 0xFF);
    }

    ffs_write(bin_id, pattern, sizeof(pattern));
    ffs_serialize_table();

    // Read back and verify
    ffs_seek(bin_id, 0, FFS_SEEK_SET);
    uint8_t readback[256] = {0};
    ffs_read(bin_id, readback, sizeof(readback));

    int mismatch = 0;
    for (int i = 0; i < 256; i++) {
        if (readback[i] != pattern[i]) {
            if (mismatch < 5) { // Limit error spam
                LOG_ERROR("Mismatch at %d: got 0x%02X, expected 0x%02X", 
                          i, readback[i], pattern[i]);
            }
            mismatch++;
        }
    }

    if (mismatch == 0) {
        LOG_INFO("Binary test PASSED");
    } else {
        LOG_ERROR("Binary test FAILED: %d mismatches", mismatch);
    }

    LOG_INFO("=== [FFS] End Binary File Test ===");
}

// =============================================================================
// MULTIBLOCK TESTS  
// =============================================================================

static void test_multiblock_log_file(void) {
    LOG_INFO("=== [FFS] Multiblock Log Test ===");

    const char* fname = "multiblock.txt";
    int log_id = ffs_open_or_create_reset(fname, 0);
    if (log_id < 0) {
        LOG_ERROR("Failed to open/create %s", fname);
        return;
    }

    const char* line = "[TESTING] Test log line that fills multiple blocks.";
    const int repeats = 120;

    // Write test lines
    for (int i = 0; i < repeats; ++i) {
        if (ffs_write_line(log_id, line) < 0) {
            LOG_ERROR("Write failed at line %d", i);
            return;
        }
    }

    ffs_serialize_table();
    ffs_debug_print_file_blocks(log_id);

    // Read back and verify
    int rd_id = ffs_open(fname);
    if (rd_id < 0) {
        LOG_ERROR("Failed to reopen %s", fname);
        return;
    }

    ffs_seek(rd_id, 0, FFS_SEEK_SET);
    
    char lineBuf[128];
    int line_count = 0;
    while (true) {
        memset(lineBuf, 0, sizeof(lineBuf));
        int len = ffs_read_line(rd_id, lineBuf, sizeof(lineBuf));
        
        if (len <= 0) break;
        
        if (len < (int)sizeof(lineBuf)) {
            lineBuf[len] = '\0';
        }
        
        // Show first and last few lines only
        if (line_count < 5 || line_count >= repeats - 5) {
            LOG_INFO("[%d] %s", line_count, lineBuf);
        }
        line_count++;
        
        osDelay(1); // RTOS cooperation
    }

    if (line_count == repeats) {
        LOG_INFO("Multiblock test PASSED: %d/%d lines", line_count, repeats);
    } else {
        LOG_ERROR("Multiblock test FAILED: %d/%d lines", line_count, repeats);
    }

    LOG_INFO("=== [FFS] End Multiblock Log Test ===");
}

// Binary pattern utilities
static void generate_binary_pattern(uint8_t* buffer, uint32_t size, uint32_t seed) {
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t)((seed + i * 37 + (i >> 8) * 113) & 0xFF);
    }
}

static bool verify_binary_pattern(const uint8_t* buffer, uint32_t size, uint32_t seed) {
    for (uint32_t i = 0; i < size; i++) {
        uint8_t expected = (uint8_t)((seed + i * 37 + (i >> 8) * 113) & 0xFF);
        if (buffer[i] != expected) {
            return false;
        }
    }
    return true;
}

static void test_multiblock_binary_file(void) {
    LOG_INFO("=== [FFS] Multiblock Binary Test ===");

    const char* fname = "binary_multiblock.bin";
    int file_id = ffs_open_or_create_reset(fname, 0);
    if (file_id < 0) {
        LOG_ERROR("Failed to open/create %s", fname);
        return;
    }

    const uint32_t chunk_size = 512;
    const uint32_t num_chunks = 20;  // 10KB total
    const uint32_t total_size = chunk_size * num_chunks;
    
    uint8_t write_buffer[512];
    uint32_t total_written = 0;

    LOG_INFO("Writing %u bytes in %u chunks", total_size, num_chunks);

    // Write binary data in chunks
    for (uint32_t chunk = 0; chunk < num_chunks; chunk++) {
        uint32_t seed = 0x12345678 + chunk;
        generate_binary_pattern(write_buffer, chunk_size, seed);

        int bytes_written = ffs_write(file_id, write_buffer, chunk_size);
        if (bytes_written != (int)chunk_size) {
            LOG_ERROR("Write failed for chunk %u", chunk);
            return;
        }
        total_written += bytes_written;
    }

    LOG_INFO("Total written: %u bytes", total_written);
    ffs_serialize_table();

    // Read back and verify
    int read_id = ffs_open(fname);
    if (read_id < 0) {
        LOG_ERROR("Failed to reopen %s", fname);
        return;
    }

    ffs_seek(read_id, 0, FFS_SEEK_SET);
    uint8_t read_buffer[512];
    uint32_t verified_chunks = 0;

    for (uint32_t chunk = 0; chunk < num_chunks; chunk++) {
        memset(read_buffer, 0, chunk_size);
        
        int bytes_read = ffs_read(read_id, read_buffer, chunk_size);
        if (bytes_read != (int)chunk_size) {
            LOG_ERROR("Read failed for chunk %u", chunk);
            break;
        }

        uint32_t seed = 0x12345678 + chunk;
        if (verify_binary_pattern(read_buffer, chunk_size, seed)) {
            verified_chunks++;
        } else {
            LOG_ERROR("Verification failed for chunk %u", chunk);
            break;
        }
        
        osDelay(1);
    }

    if (verified_chunks == num_chunks) {
        LOG_INFO("Multiblock binary test PASSED: %u/%u chunks", verified_chunks, num_chunks);
    } else {
        LOG_ERROR("Multiblock binary test FAILED: %u/%u chunks", verified_chunks, num_chunks);
    }

    LOG_INFO("=== [FFS] End Multiblock Binary Test ===");
}

// =============================================================================
// PERSISTENT BINARY FILE FUNCTIONALITY
// =============================================================================

static void test_persistent_binary_file(void) {
    LOG_INFO("=== [FFS] Persistent Binary File Test ===");
    
    // Define binary chunk header structure
    typedef struct {
        uint32_t magic;
        uint32_t counter;
        uint32_t chunk_id;
        uint32_t data_size;
    } bin_chunk_header_t;
    
    const char* BIN_FILE_NAME = "persist_binary.dat";
    const char* COUNTER_NAME = "bin_counter.txt";
    
    if (!ffs_mount_or_format()) return;
    
    // Get persistent write counter
    uint32_t write_counter = 0;
    int counter_id = ffs_open_log(COUNTER_NAME);
    if (counter_id >= 0) {
        LOG_INFO("[PBIN] Opened counter file (id=%d)", counter_id);
        ffs_seek(counter_id, 0, FFS_SEEK_SET);
        char counter_str[32] = {0};
        int line_len = ffs_read_line(counter_id, counter_str, sizeof(counter_str));
        if (line_len > 0) {
            write_counter = (uint32_t)atoi(counter_str);
            LOG_INFO("[PBIN] Counter: %lu", (unsigned long)write_counter);
        }
    } else {
        LOG_INFO("[PBIN] Creating new counter file");
        counter_id = ffs_create(COUNTER_NAME, 32);
        if (counter_id >= 0) {
            char counter_str[32];
            snprintf(counter_str, sizeof(counter_str), "%lu", (unsigned long)write_counter);
            ffs_write_line(counter_id, counter_str);
        }
    }
    
    // Open/create persistent binary file
    int bin_id = ffs_open_binary(BIN_FILE_NAME);
    if (bin_id < 0) {
        LOG_ERROR("[PBIN] Failed to open/create %s", BIN_FILE_NAME);
        return;
    }
    LOG_INFO("[PBIN] Opened %s (id=%d)", BIN_FILE_NAME, bin_id);
    
    // Generate and write binary data chunks
    const uint32_t chunk_size = 64;
    const int num_chunks = 20;
    uint8_t write_buffer[64];
    
    for (int chunk = 0; chunk < num_chunks; chunk++) {
        // Generate unique pattern based on write_counter and chunk
        uint32_t seed = 0xDEADBEEF + write_counter * 1000 + chunk;
        generate_binary_pattern(write_buffer, chunk_size, seed);
        
        // Add a small header with counter and chunk info
        bin_chunk_header_t header = {
            .magic = 0xCAFEBABE,
            .counter = write_counter,
            .chunk_id = chunk,
            .data_size = chunk_size
        };
        
        // Write header then data
        if (ffs_write(bin_id, &header, sizeof(header)) < 0) {
            LOG_ERROR("[PBIN] Failed to write header for chunk %d", chunk);
            break;
        }
        
        if (ffs_write(bin_id, write_buffer, chunk_size) < 0) {
            LOG_ERROR("[PBIN] Failed to write data for chunk %d", chunk);
            break;
        }
        
        write_counter++;
    }
    
    // Save updated counter
    LOG_INFO("[PBIN] Saving counter: %lu", (unsigned long)write_counter);
    ffs_delete(COUNTER_NAME);
    counter_id = ffs_create(COUNTER_NAME, 32);
    
    if (counter_id >= 0) {
        char counter_str[32];
        snprintf(counter_str, sizeof(counter_str), "%lu", (unsigned long)write_counter);
        ffs_write_line(counter_id, counter_str);
        ffs_serialize_table();
        LOG_INFO("[PBIN] Counter saved: %s", counter_str);
    }
    
    LOG_INFO("[PBIN] Session complete: %d chunks written", num_chunks);
    LOG_INFO("[PBIN] Global counter now: %lu", (unsigned long)write_counter);
    
    // Read back and verify the entire file
    LOG_INFO("[PBIN] Verifying entire binary file...");
    ffs_seek(bin_id, 0, FFS_SEEK_SET); // Start from beginning
    
    uint8_t read_buffer[64];
    int verified_chunks = 0;
    int total_chunks_in_file = 0;
    uint32_t chunk_entry_size = sizeof(bin_chunk_header_t) + chunk_size;
    
    // Calculate total chunks in file based on file size
    uint32_t file_size = file_table[bin_id].size;
    total_chunks_in_file = file_size / chunk_entry_size;
    
    LOG_INFO("[PBIN] File size: %lu bytes, Expected chunks: %d", 
             (unsigned long)file_size, total_chunks_in_file);
    
    for (int chunk_idx = 0; chunk_idx < total_chunks_in_file; chunk_idx++) {
        bin_chunk_header_t read_header;
        
        if (ffs_read(bin_id, &read_header, sizeof(read_header)) != sizeof(read_header)) {
            LOG_ERROR("[PBIN] Failed to read header for chunk %d", chunk_idx);
            break;
        }
        
        if (ffs_read(bin_id, read_buffer, chunk_size) != (int)chunk_size) {
            LOG_ERROR("[PBIN] Failed to read data for chunk %d", chunk_idx);
            break;
        }
        
        // Verify header magic
        if (read_header.magic != 0xCAFEBABE) {
            LOG_ERROR("[PBIN] Invalid magic in chunk %d: 0x%08lX", chunk_idx, (unsigned long)read_header.magic);
            break;
        }
        
        // Verify data pattern using the stored counter and chunk_id from header
        uint32_t seed = 0xDEADBEEF + read_header.counter * 1000 + read_header.chunk_id;
        
        if (verify_binary_pattern(read_buffer, chunk_size, seed)) {
            verified_chunks++;
            // Show details for first/last few chunks and newly written ones
            bool is_new_chunk = (chunk_idx >= total_chunks_in_file - num_chunks);
            if (chunk_idx < 3 || chunk_idx >= total_chunks_in_file - 3 || is_new_chunk) {
                LOG_INFO("[PBIN] Chunk %d verified: counter=%lu, chunk_id=%lu %s", 
                         chunk_idx, (unsigned long)read_header.counter, 
                         (unsigned long)read_header.chunk_id,
                         is_new_chunk ? "[NEW]" : "");
            }
        } else {
            LOG_ERROR("[PBIN] Verification failed for chunk %d (counter=%lu, chunk_id=%lu)", 
                      chunk_idx, (unsigned long)read_header.counter, (unsigned long)read_header.chunk_id);
            break;
        }
        
        // RTOS cooperation for large files
        if (chunk_idx % 50 == 0) osDelay(1);
    }
    
    if (verified_chunks == total_chunks_in_file) {
        LOG_INFO("[PBIN] FULL FILE Verification PASSED: %d/%d chunks", verified_chunks, total_chunks_in_file);
    } else {
        LOG_ERROR("[PBIN] FULL FILE Verification FAILED: %d/%d chunks", verified_chunks, total_chunks_in_file);
    }
    
    LOG_INFO("[PBIN] Current session added %d chunks, total file now has %d chunks", 
             num_chunks, total_chunks_in_file);
    
    LOG_INFO("=== [FFS] End Persistent Binary File Test ===");
}

// =============================================================================
// PERSISTENT LOG FUNCTIONALITY
// =============================================================================

static void plog_dump_all_lines(int file_id) {
    if (file_id < 0) return;

    uint32_t saved_cursor = file_table[file_id].cursor;
    ffs_seek(file_id, 0, FFS_SEEK_SET);

    char line[256];
    uint32_t ln = 0;
    for (;;) {
        uint32_t n = ffs_read_line(file_id, line, sizeof(line));
        if (n <= 0) break;

        if (n >= (uint32_t)sizeof(line)) n = (uint32_t)sizeof(line) - 1;
        line[n] = '\0';

        if (n > 0 && line[n-1] == '\n') line[n-1] = '\0';

        LOG_INFO("[PLOG][%03d] %s", ln++, line);
        osDelay(1);
    }

    file_table[file_id].cursor = saved_cursor;
}

static void test_persistent_log_once(const char* tag) {
    const char* LOG_NAME = "persist_log.txt";
    const char* COUNTER_NAME = "line_counter.txt";

    if (!ffs_mount_or_format()) return;

    // Get persistent line counter
    uint32_t line_counter = 0;
    int counter_id = ffs_open_log(COUNTER_NAME);
    if (counter_id >= 0) {
        LOG_INFO("[PLOG] Opened counter file (id=%d)", counter_id);
        ffs_seek(counter_id, 0, FFS_SEEK_SET);
        char counter_str[32] = {0};
        int line_len = ffs_read_line(counter_id, counter_str, sizeof(counter_str));
        if (line_len > 0) {
            line_counter = (uint32_t)atoi(counter_str);
            LOG_INFO("[PLOG] Counter: %lu", (unsigned long)line_counter);
        }
    } else {
        LOG_INFO("[PLOG] Creating new counter file");
        counter_id = ffs_create(COUNTER_NAME, 32);
        if (counter_id >= 0) {
            char counter_str[32];
            snprintf(counter_str, sizeof(counter_str), "%lu", (unsigned long)line_counter);
            ffs_write_line(counter_id, counter_str);
        }
    }

    // Open/create log file
    int id = ffs_open_log(LOG_NAME);
    if (id < 0) {
        id = ffs_create(LOG_NAME, 0);
        if (id < 0) {
            LOG_ERROR("[PLOG] Failed to create %s", LOG_NAME);
            return;
        }
        LOG_INFO("[PLOG] Created %s", LOG_NAME);
    } else {
        LOG_INFO("[PLOG] Opened existing %s", LOG_NAME);
    }

    // Write log entries
    char line[128];
    const int repeats = 50;
    
    for (int i = 0; i < repeats; ++i) {
        const char* ts = rtc_get_time_str();
        snprintf(line, sizeof(line),
                 "[%s] [INFO ] Testing multiblock write number[%lu]",
                 ts, (unsigned long)line_counter);

        if (ffs_write_line(id, line) < 0) {
            LOG_ERROR("[PLOG] write_line failed at line %lu", (unsigned long)line_counter);
            return;
        }
        line_counter++;
    }

    // Save updated counter
    LOG_INFO("[PLOG] Saving counter: %lu", (unsigned long)line_counter);
    ffs_delete(COUNTER_NAME);
    counter_id = ffs_create(COUNTER_NAME, 32);
    
    if (counter_id >= 0) {
        char counter_str[32];
        snprintf(counter_str, sizeof(counter_str), "%lu", (unsigned long)line_counter);
        ffs_write_line(counter_id, counter_str);
        ffs_serialize_table();
        LOG_INFO("[PLOG] Counter saved: %s", counter_str);
    }

    LOG_INFO("[PLOG] Session complete: %d lines written", repeats);
    LOG_INFO("[PLOG] Global counter now: %lu", (unsigned long)line_counter);
    
    ffs_debug_print_file_blocks(id);
    plog_dump_all_lines(id);
}

static void test_persistent_log(void) {
    test_persistent_log_once("BOOT");
}

// =============================================================================
// FFS FIX-PLAN REGRESSION TESTS (see .claude/ffs_review.md)
// =============================================================================

#if TEST_STRADDLE
/** Bug A3: writes that cross the 4096-byte block boundary must read back
 *  byte-exact across the whole multi-block chain.
 *
 *  Part 1 (always on): three 3000-byte writes = 9000 bytes spanning 3+ blocks,
 *  with two boundary-straddling writes. Read back and verify every byte.
 *
 *  Part 2 (TEST_STRADDLE_LARGE, enable ONLY after Stage 3): one single
 *  9000-byte write crossing two boundaries in one ffs_write() call. With the
 *  pre-Stage-3 code this would loop allocating fresh blocks until the disk is
 *  exhausted (space==4096 < remaining -> abandon -> alloc -> repeat), so it
 *  stays compiled out until the write path is fixed.
 *
 *  Buffers are static: test task stack is only 4KB. */
#define TEST_STRADDLE_LARGE 1U

static uint8_t straddle_buf[3000];
static const uint32_t straddle_seeds[3] = { 0xA5, 0x5A, 0xC3 };

static uint8_t straddle_expected_byte(uint32_t abs_off) {
    uint32_t region = abs_off / 3000;          // 0,1,2
    uint32_t rel    = abs_off % 3000;
    uint32_t seed   = straddle_seeds[region];
    return (uint8_t)((seed + rel * 37 + (rel >> 8) * 113) & 0xFF);
}

static bool straddle_locate_file_offset(int file_id, uint32_t file_off,
                                        uint32_t* out_block, uint32_t* out_block_off) {
    if (file_id < 0 || file_id >= FFS_MAX_FILES || !file_table[file_id].in_use) {
        return false;
    }

    uint32_t block = file_table[file_id].head_block;
    uint32_t off = file_off;
    uint32_t steps = 0;

    while (block != FFS_BLOCK_EOF && block < ffs_config.block_count) {
        uint32_t valid = block_table[block].valid_bytes;
        if (off < valid) {
            *out_block = block;
            *out_block_off = off;
            return true;
        }

        off -= valid;
        block = block_table[block].next;
        if (++steps > ffs_config.block_count) {
            break;
        }
    }

    return false;
}

static void straddle_probe_mismatch(int file_id, uint32_t file_off,
                                    uint8_t got, uint8_t expected) {
    uint32_t block = 0;
    uint32_t block_off = 0;

    if (!straddle_locate_file_offset(file_id, file_off, &block, &block_off)) {
        LOG_ERROR("[FFS-BASELINE] straddle probe: could not map file offset %lu",
                  (unsigned long)file_off);
        return;
    }

    uint32_t probe_start = (block_off >= 4U) ? (block_off - 4U) : 0U;
    uint32_t probe_len = 16U;
    if (probe_start + probe_len > ffs_config.block_size) {
        probe_len = ffs_config.block_size - probe_start;
    }

    uint8_t raw[16] = {0};
    uintptr_t base = (uintptr_t)ffs_config.context;
    uint32_t flash_addr = (uint32_t)(base + block * ffs_config.block_size + probe_start);
    spiFlash.readData(flash_addr, raw, probe_len);

    LOG_ERROR("[FFS-BASELINE] straddle mismatch detail: file_off=%lu block=%lu block_off=%lu got=0x%02X expected=0x%02X raw_start=0x%08lX",
              (unsigned long)file_off,
              (unsigned long)block,
              (unsigned long)block_off,
              got,
              expected,
              (unsigned long)flash_addr);

    for (uint32_t i = 0; i < probe_len; i++) {
        uint32_t sample_file_off = file_off + i + probe_start - block_off;
        LOG_ERROR("[FFS-BASELINE] raw[%02lu] file_off=%lu val=0x%02X expected=0x%02X%s",
                  (unsigned long)i,
                  (unsigned long)sample_file_off,
                  raw[i],
                  straddle_expected_byte(sample_file_off),
                  (probe_start + i == block_off) ? " <-- first bad" : "");
    }
}

/* Read total_size bytes from file id (cursor at 0) and verify against
   straddle_expected_byte(). Returns number of mismatches. */
static uint32_t straddle_verify(int rd, uint32_t total_size, uint32_t* first_bad) {
    uint32_t mismatches = 0;
    *first_bad = 0xFFFFFFFF;
    uint8_t chunk[500];

    for (uint32_t off = 0; off < total_size; off += sizeof(chunk)) {
        uint32_t want = (total_size - off > sizeof(chunk)) ? sizeof(chunk) : (total_size - off);
        int got = ffs_read(rd, chunk, want);
        if (got != (int)want) {
            LOG_ERROR("[FFS-BASELINE] straddle: read at %lu returned %d (wanted %lu)",
                      (unsigned long)off, got, (unsigned long)want);
            mismatches += want;
            break;
        }
        for (uint32_t i = 0; i < want; i++) {
            uint8_t expected = straddle_expected_byte(off + i);
            if (chunk[i] != expected) {
                if (*first_bad == 0xFFFFFFFF) {
                    *first_bad = off + i;
                    straddle_probe_mismatch(rd, off + i, chunk[i], expected);
                }
                mismatches++;
            }
        }
        osDelay(1);
    }
    return mismatches;
}

static void test_write_straddle(void) {
    LOG_INFO("=== [FFS-BASELINE] Block-Straddle Write Test (bug A3) ===");

    // ---- Part 1: three 3000-byte writes -> 9000 bytes over 3+ blocks ----
    const char* fname = "straddle_test.bin";
    int id = ffs_open_or_create_reset(fname, 0);
    if (id < 0) { LOG_ERROR("[FFS-BASELINE] straddle: create failed"); return; }

    for (int w = 0; w < 3; w++) {
        generate_binary_pattern(straddle_buf, sizeof(straddle_buf), straddle_seeds[w]);
        if (ffs_write(id, straddle_buf, sizeof(straddle_buf)) != (int)sizeof(straddle_buf)) {
            LOG_ERROR("[FFS-BASELINE] straddle: write %d failed", w);
            return;
        }
    }
    ffs_serialize_table();
    ffs_debug_print_file_blocks(id);   // show the multi-block chain

    int rd = ffs_open(fname);
    if (rd < 0) { LOG_ERROR("[FFS-BASELINE] straddle: reopen failed"); return; }
    ffs_seek(rd, 0, FFS_SEEK_SET);

    uint32_t first_bad = 0;
    uint32_t mismatches = straddle_verify(rd, 9000, &first_bad);

    if (mismatches == 0) {
        LOG_INFO("[FFS-BASELINE] STRADDLE TEST PASSED (9000/9000 bytes over 3 blocks)");
    } else {
        LOG_ERROR("[FFS-BASELINE] STRADDLE TEST FAILED: %lu bad bytes, first at offset %lu (expected to fail until Stage 3)",
                  (unsigned long)mismatches, (unsigned long)first_bad);
    }

#if TEST_STRADDLE_LARGE
    // ---- Part 2: single 9000-byte write crossing two block boundaries ----
    static uint8_t large_buf[9000];
    const char* fname2 = "straddle_large.bin";
    int id2 = ffs_open_or_create_reset(fname2, 0);
    if (id2 < 0) { LOG_ERROR("[FFS-BASELINE] straddle-large: create failed"); return; }

    for (uint32_t i = 0; i < sizeof(large_buf); i++) large_buf[i] = straddle_expected_byte(i);

    if (ffs_write(id2, large_buf, sizeof(large_buf)) != (int)sizeof(large_buf)) {
        LOG_ERROR("[FFS-BASELINE] STRADDLE-LARGE FAILED: single 9000B write rejected");
        return;
    }
    ffs_serialize_table();

    int rd2 = ffs_open(fname2);
    ffs_seek(rd2, 0, FFS_SEEK_SET);
    mismatches = straddle_verify(rd2, 9000, &first_bad);

    if (mismatches == 0) {
        LOG_INFO("[FFS-BASELINE] STRADDLE-LARGE PASSED (single 9000B write over 3 blocks)");
    } else {
        LOG_ERROR("[FFS-BASELINE] STRADDLE-LARGE FAILED: %lu bad bytes, first at offset %lu",
                  (unsigned long)mismatches, (unsigned long)first_bad);
    }
#endif /* TEST_STRADDLE_LARGE */

    LOG_INFO("=== [FFS-BASELINE] End Straddle Test ===");
}
#endif /* TEST_STRADDLE */

#if TEST_PERSISTENCE
/** Bug A1: erase counts must survive a power cycle. Strategy: at the END of a
 *  test run, record the session's total erase count INTO the file; at the
 *  START of the next run, the in-RAM total (loaded at mount) must be >= the
 *  recorded value. Erase counts only grow, so "less than recorded" means the
 *  persisted block table failed to load (the silent 48KB-malloc failure). */
static const char* PERSIST_FNAME  = "persist_test.txt";
static const char* PERSIST_MARKER = "persist-marker-1234567890";
static const char* PERSIST_TOTAL_PREFIX = "erase_total_hi=";

static uint64_t persist_total_erase_count(void) {
    uint64_t total = 0;
    for (uint32_t i = 0; i < ffs_config.block_count; i++) {
        if (block_table[i].erase_count != UINT32_MAX) {
            total += block_table[i].erase_count;
        }
    }
    return total;
}

static void persist_format_total_line(char* line, size_t line_size, uint64_t total) {
    uint32_t hi = (uint32_t)(total >> 32);
    uint32_t lo = (uint32_t)(total & 0xFFFFFFFFu);
    snprintf(line, line_size, "%s%lu lo=%lu",
             PERSIST_TOTAL_PREFIX,
             (unsigned long)hi,
             (unsigned long)lo);
}

static bool persist_parse_total_line(const char* line, uint64_t* total) {
    if (line == NULL || total == NULL) {
        return false;
    }

    if (strncmp(line, PERSIST_TOTAL_PREFIX, strlen(PERSIST_TOTAL_PREFIX)) != 0) {
        return false;
    }

    const char* hi_str = line + strlen(PERSIST_TOTAL_PREFIX);
    char* lo_key = strstr(hi_str, " lo=");
    if (lo_key == NULL) {
        return false;
    }

    char* hi_end = NULL;
    unsigned long hi = strtoul(hi_str, &hi_end, 10);
    if (hi_end != lo_key) {
        return false;
    }

    const char* lo_str = lo_key + strlen(" lo=");
    char* lo_end = NULL;
    unsigned long lo = strtoul(lo_str, &lo_end, 10);
    if (lo_end == lo_str) {
        return false;
    }
    if (*lo_end == '\r' || *lo_end == '\n') {
        lo_end++;
    }
    if (*lo_end != '\0') {
        return false;
    }

    *total = ((uint64_t)hi << 32) | (uint32_t)lo;
    return true;
}

/* Call FIRST in test_filesystem(), before the suites inflate this session's counts. */
static void test_persistence_check(void) {
    LOG_INFO("=== [FFS-BASELINE] Persistence Check (bug A1) ===");

    if (!ffs_file_exists(PERSIST_FNAME)) {
        LOG_INFO("[FFS-BASELINE] PERSIST: no record file - first run, check skipped.");
        LOG_INFO("[FFS-BASELINE] >>> After this run completes, POWER-CYCLE and rerun for the real check <<<");
        return;
    }

    int id = ffs_open(PERSIST_FNAME);
    ffs_seek(id, 0, FFS_SEEK_SET);

    char line[64] = {0};
    int len = ffs_read_line(id, line, sizeof(line));
    bool content_ok = (len > 0) && (strncmp(line, PERSIST_MARKER, strlen(PERSIST_MARKER)) == 0);
    if (content_ok) {
        LOG_INFO("[FFS-BASELINE] PERSIST content PASSED");
    } else {
        LOG_ERROR("[FFS-BASELINE] PERSIST content FAILED (got '%s')", line);
    }

    uint64_t recorded = 0;
    memset(line, 0, sizeof(line));
    if (ffs_read_line(id, line, sizeof(line)) > 0 &&
        persist_parse_total_line(line, &recorded)) {
        uint64_t now = persist_total_erase_count();
        if (now >= recorded) {
            LOG_INFO("[FFS-BASELINE] PERSIST erase-counts PASSED (now 0x%08lX%08lX >= recorded 0x%08lX%08lX)",
                     (unsigned long)(now >> 32),
                     (unsigned long)(now & 0xFFFFFFFFu),
                     (unsigned long)(recorded >> 32),
                     (unsigned long)(recorded & 0xFFFFFFFFu));
        } else {
            LOG_ERROR("[FFS-BASELINE] PERSIST erase-counts FAILED: now 0x%08lX%08lX < recorded 0x%08lX%08lX - "
                      "block table did not persist (expected to fail until Stage 1)",
                      (unsigned long)(now >> 32),
                      (unsigned long)(now & 0xFFFFFFFFu),
                      (unsigned long)(recorded >> 32),
                      (unsigned long)(recorded & 0xFFFFFFFFu));
        }
    } else {
        LOG_ERROR("[FFS-BASELINE] PERSIST: record line missing/unparseable ('%s')", line);
    }
}

/* Call LAST in test_filesystem(), after the suites have performed many erases. */
static void test_persistence_record(void) {
    int id = ffs_open_or_create_reset(PERSIST_FNAME, 0);
    if (id < 0) {
        LOG_ERROR("[FFS-BASELINE] PERSIST: failed to create record file");
        return;
    }
    uint64_t total = persist_total_erase_count();
    char line[64];
    persist_format_total_line(line, sizeof(line), total);

    ffs_write_line(id, PERSIST_MARKER);
    ffs_write_line(id, line);
    ffs_serialize_table();
    ffs_block_alloc_save();

    LOG_INFO("[FFS-BASELINE] PERSIST recorded erase_total=0x%08lX%08lX - power-cycle and rerun to verify",
             (unsigned long)(total >> 32),
             (unsigned long)(total & 0xFFFFFFFFu));
}
#endif /* TEST_PERSISTENCE */

// =============================================================================
// TEST SUITES
// =============================================================================

static void test_ffs_write_read_suite(void) {
    LOG_INFO("=== [FFS] Write/Read Test Suite ===");
    
    if (!ffs_mount_or_format()) return;
    
    test_basic_write_read();
    osDelay(500);
    
    test_text_log_file();
    osDelay(500);
    
    test_binary_file();
    osDelay(500);
    
    test_multiblock_log_file();
    
    LOG_INFO("=== [FFS] Write/Read Suite Complete ===");
}

static void test_ffs_binary_suite(void) {
    LOG_INFO("=== [FFS] Binary Test Suite ===");
    
    if (!ffs_mount_or_format()) return;
    
    test_multiblock_binary_file();
    
    LOG_INFO("=== [FFS] Binary Suite Complete ===");
}

static void list_files(void)
{
    char files[10][32];
    int file_count = log_list_files(files, 10);

    LOG_INFO("Found %d log files in filesystem:", file_count);

    for (int i = 0; i < file_count; i++) {
        LOG_INFO("  - %s", files[i]);
    }
}

// =============================================================================
// MAIN TEST ENTRY POINTS
// =============================================================================

// =============================================================================
// PERSISTENT LOGGING TEST
// =============================================================================

static void test_persistent_logging(void) {
    LOG_INFO("=== [FFS] Persistent Logging Test ===");
    
    // Test different log levels to verify they persist
    LOG_INFO("This is an INFO message that should persist to daily log file");
    osDelay(100);
    
    LOG_WARN("This is a WARNING message - should persist and be visible after reset");
    osDelay(100);
    
    LOG_ERROR("This is an ERROR message - critical for troubleshooting after reset");
    osDelay(100);
    clear_error(); // fake error
    LOG_SYSSTATUS("System status: All systems operational - persistent logging active");
    osDelay(100);
    
    // Test log management functions
    LOG_INFO("Current log file: %s", log_get_current_filename());
    LOG_INFO("Current log file size: %lu bytes", (unsigned long)log_get_current_file_size());
    
    // List all log files
    char log_files[SPI_FLASH_MAX_FILES][SPI_FLASH_MAX_FILE_NAME];
    int log_count = log_list_files(log_files, 10);
    LOG_INFO("Found %d log files in filesystem:", log_count);
    for (int i = 0; i < log_count; i++) {
        LOG_INFO("  - %s", log_files[i]);
    }
    
    LOG_INFO("=== [FFS] Persistent Logging Test Complete ===");
}


void test_filesystem(void) {
    LOG_INFO("=== [FFS] Starting Filesystem Tests ===");

#if TEST_PERSISTENCE
    // Must run FIRST: compares loaded erase counts against the previous run's
    // record before this session inflates them.
    if (!ffs_mount_or_format()) return;
    test_persistence_check();
    osDelay(500);
#endif

#if TEST_READ_WRITE_SUITE
   test_ffs_write_read_suite();
   osDelay(1000);
#endif

#if TEST_BINARY_SUITE
   test_ffs_binary_suite();
   osDelay(1000);
#endif

#if TEST_PERSISTENT_BINARY
   test_persistent_binary_file();
   osDelay(1000);
#endif

#if TEST_PERSISTENT_LOG
    test_persistent_logging();
    osDelay(1000);
#endif

#if TEST_STRADDLE
    test_write_straddle();
    osDelay(500);
#endif

#if TEST_PERSISTENCE
    // Must run LAST: records this session's erase totals for the next boot.
    test_persistence_record();
#endif

    list_files();


    LOG_INFO("=== [FFS] All Filesystem Tests Complete ===");
}

void test_block_allocation(void) {
    LOG_INFO("=== [FFS] Block Allocation Test ===");
    
    if (!ffs_mount_or_format()) return;

    const char* fname = "allocation_test.txt";
    int fid = ffs_open_or_create_reset(fname, 0);
    if (fid < 0) {
        LOG_ERROR("File open/create failed");
        return;
    }

    LOG_INFO("Created file %s, checking allocation...", fname);
    
    // Show first and last 20 blocks
    for (int i = 0; i < 20; ++i) {
        LOG_INFO("Block[%02d]: used = %s", i, block_table[i].in_use ? "true" : "false");
    }
    
    for (int i = 4076; i < 4096; ++i) {
        LOG_INFO("Block[%d]: used = %s", i, block_table[i].in_use ? "true" : "false");
    }

    LOG_INFO("=== [FFS] End Block Allocation Test ===");
}
