/**
 * @file    fs_binary_upload.c
 * @brief   Binary file upload command handlers implementation
 * @details Implements handlers for file upload operations over UART,
 *          including start, chunk transfer with CRC validation, and
 *          completion of file uploads to the flash filesystem.
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

#include "fs_binary_upload.h"
#include "app_reply.h"
#include "ffs.h"
#include "spi_flash_block_device.hpp"
#include "log.hpp"
#include <string.h>
#include "app_defs.hpp"
#include "data_transport.h"
#include "stm32f7xx_hal.h"
extern ffs_config_t spi_flash_fs_config;

upload_state_t upload_state = {0};

void fs_cmd_handle_upload_start(const uint8_t* args, uint8_t len) {
    LOG_INFO("[FS_UPLOAD] Upload start command received, len=%d", len);
    send_framed_response(RESP_TYPE_INFO, "DEBUG: Upload start handler called");
    
    if (!args || len < 5) {
        LOG_ERROR("[FS_UPLOAD] Invalid upload start format: args=%p, len=%d", args, len);
        send_framed_response(RESP_TYPE_ERROR, "Invalid upload start format");
        return;
    }
    send_framed_response(RESP_TYPE_INFO, "DEBUG: Args validation passed");

    // Initialize FFS if needed
    send_framed_response(RESP_TYPE_INFO, "DEBUG: About to mount FFS");
    if (!ffs_mount(&spi_flash_fs_config)) {
        LOG_WARN("FFS mount failed, attempting format...");
        send_framed_response(RESP_TYPE_INFO, "DEBUG: FFS mount failed, trying format");
        if (ffs_format() != 0 || !ffs_mount(&spi_flash_fs_config)) {
            send_framed_response(RESP_TYPE_ERROR, "FS MOUNT FAILED");
            return;
        }
    }
    send_framed_response(RESP_TYPE_INFO, "DEBUG: FFS mount successful");

    // Reset any previous upload state
    if (upload_state.active && upload_state.file_id >= 0) {
        // Close any previously open file
        LOG_WARN("[FS_UPLOAD] Aborting previous upload");
    }
    memset(&upload_state, 0, sizeof(upload_state));

    // Parse filename (null-terminated string at start)
    const char* filename = (const char*)args;
    size_t filename_len = strnlen(filename, len - 4);  // Leave 4 bytes for file size
    
    LOG_INFO("[FS_UPLOAD] Filename: '%s', len=%lu", filename, (unsigned long)filename_len);
    
    if (filename_len == 0 || filename_len >= sizeof(upload_state.filename)) {
        LOG_ERROR("[FS_UPLOAD] Invalid filename length: %zu", filename_len);
        send_framed_response(RESP_TYPE_ERROR, "Invalid filename");
        return;
    }

    // Parse expected file size (4 bytes little-endian after filename + null)
    const uint8_t* size_bytes = args + filename_len + 1;
    uint32_t file_size = size_bytes[0] | (size_bytes[1] << 8) | (size_bytes[2] << 16) | (size_bytes[3] << 24);
    
    LOG_INFO("[FS_UPLOAD] Parsed file size: %lu bytes", (unsigned long)file_size);

    if (file_size == 0) {
        send_framed_response(RESP_TYPE_ERROR, "Invalid file size");
        return;
    }

    // Create file for upload
    send_framed_response(RESP_TYPE_INFO, "DEBUG: About to create file");
    int file_id = ffs_open_or_create_reset(filename, 0);
    if (file_id < 0) {
        send_framed_response(RESP_TYPE_ERROR, "Failed to create file");
        LOG_ERROR("[FS_UPLOAD] Failed to create file: %s", filename);
        return;
    }
    send_framed_response(RESP_TYPE_INFO, "DEBUG: File created successfully");

    // Initialize upload state
    upload_state.active = true;
    strncpy(upload_state.filename, filename, sizeof(upload_state.filename) - 1);
    upload_state.file_id = file_id;
    upload_state.total_bytes_received = 0;
    upload_state.expected_file_size = file_size;

    LOG_INFO("[FS_UPLOAD] Starting upload: %s (%lu bytes)", filename, (unsigned long)file_size);
    
    // Switch to data mode for receiving raw chunks (transport-agnostic)
    data_transport_set_mode(APP_TRANSPORT_MODE_FILESYSTEM_DATA);
    send_framed_response(RESP_TYPE_SUCCESS, "Upload started - switched to data mode");
}

void fs_cmd_handle_upload_chunk(const uint8_t* args, uint8_t len) {
    static uint32_t chunk_count = 0;
    chunk_count++;
    
    if (!upload_state.active) {
        send_framed_response(RESP_TYPE_ERROR, "No active upload");
        LOG_ERROR("[FS_UPLOAD] Chunk %lu: Upload not active!", chunk_count);
        return;
    }

    if (!args || len == 0) {
        send_framed_response(RESP_TYPE_ERROR, "Empty chunk");
        LOG_ERROR("[FS_UPLOAD] Chunk %lu: Empty chunk received", chunk_count);
        return;
    }
    
    if (chunk_count % 50 == 1) {  // Log every 50th chunk to avoid spam
        LOG_INFO("[FS_UPLOAD] Chunk %lu: %d bytes, total: %lu/%lu", 
                 chunk_count, len, upload_state.total_bytes_received, upload_state.expected_file_size);
    }

    // Write chunk data to file
    int bytes_written = ffs_write(upload_state.file_id, args, len);
    if (bytes_written != len) {
        send_framed_response(RESP_TYPE_ERROR, "Chunk write failed");
        LOG_ERROR("[FS_UPLOAD] Chunk write failed: wrote %d of %d bytes", bytes_written, len);
        
        // Abort upload
        upload_state.active = false;
        return;
    }

    upload_state.total_bytes_received += len;
    
    // Send progress response
    char progress[64];
    uint32_t percent = (upload_state.total_bytes_received * 100) / upload_state.expected_file_size;
    snprintf(progress, sizeof(progress), "Progress: %lu/%lu bytes (%lu%%)", 
             (unsigned long)upload_state.total_bytes_received,
             (unsigned long)upload_state.expected_file_size,
             (unsigned long)percent);
    
    send_framed_response(RESP_TYPE_INFO, progress);
    
    LOG_INFO("[FS_UPLOAD] Received chunk: %d bytes (total: %lu/%lu)", 
             len, (unsigned long)upload_state.total_bytes_received, 
             (unsigned long)upload_state.expected_file_size);
}

void fs_cmd_handle_upload_chunk_raw(const uint8_t* data, size_t len) {
    static uint32_t raw_chunk_count = 0;
    raw_chunk_count++;
    
    if (!upload_state.active) {
        LOG_ERROR("[FS_UPLOAD] Raw chunk %lu: Upload not active!", raw_chunk_count);
        return;
    }

    if (!data || len == 0) {
        LOG_ERROR("[FS_UPLOAD] Raw chunk %lu: Empty chunk received", raw_chunk_count);
        return;
    }
    
    // For 1024+2 CRC chunks, extract and verify the data part
    if (len < 2) {
        LOG_ERROR("[FS_UPLOAD] Raw chunk %lu: Too small (no CRC)", raw_chunk_count);
        upload_state.active = false;
        data_transport_set_mode(APP_TRANSPORT_MODE_FILESYSTEM_COMMAND);  // Switch back to command/receive mode
        return;
    }
    
    size_t data_len = len - 2;  // Data is all bytes except last 2 CRC bytes
    const uint8_t* chunk_data = data;
    const uint8_t* crc_bytes = data + data_len;
    
    // Extract CRC16 (big-endian)
    uint16_t received_crc = (crc_bytes[0] << 8) | crc_bytes[1];
    uint16_t calculated_crc = app_crc16(chunk_data, data_len);
    
    if (received_crc != calculated_crc) {
        LOG_ERROR("[FS_UPLOAD] Raw chunk %lu: CRC mismatch! Received 0x%04X, calculated 0x%04X", 
                  raw_chunk_count, received_crc, calculated_crc);
        app_reply_send_chunk_ack(APP_REPLY_CHUNK_ERR, upload_state.total_bytes_received);
        return;  // Don't abort upload, just NACK this chunk
    }
    
    // Removed excessive per-chunk logging - only log errors or every 100th chunk
    if (raw_chunk_count % 100 == 0) {
        LOG_INFO("[FS_UPLOAD] Progress: chunk %lu, %lu/%lu bytes (%.1f%%)",
                 raw_chunk_count,
                 upload_state.total_bytes_received + data_len,
                 upload_state.expected_file_size,
                 (float)(upload_state.total_bytes_received + data_len) * 100.0f / upload_state.expected_file_size);
    }

    // Write chunk data to file
    uint32_t write_start = HAL_GetTick();
    
    
    int bytes_written = ffs_write(upload_state.file_id, chunk_data, data_len);
    uint32_t write_time = HAL_GetTick() - write_start;
    
    if (bytes_written != data_len) {
        LOG_ERROR("[FS_UPLOAD] Raw chunk write failed: wrote %d of %d bytes", bytes_written, (int)data_len);
        upload_state.active = false;
        data_transport_set_mode(APP_TRANSPORT_MODE_FILESYSTEM_COMMAND);  // Switch back to command/receive mode
        return;
    }
    
    
    if (write_time > 10) {
        LOG_INFO("[FS_UPLOAD] Chunk %lu write took %lu ms (new block allocation?)", raw_chunk_count, write_time);
    }

    upload_state.total_bytes_received += data_len;


    app_reply_send_chunk_ack(APP_REPLY_CHUNK_OK, upload_state.total_bytes_received);
    // Removed per-chunk ACK logging - reduces log spam during uploads

    // Removed HAL_Delay(2) - unnecessary since Python client already has 20ms inter-chunk delay

    if (upload_state.total_bytes_received >= upload_state.expected_file_size) {
        LOG_INFO("[FS_UPLOAD] File upload complete (%lu/%lu bytes), switching to command mode", 
                 upload_state.total_bytes_received, upload_state.expected_file_size);
        data_transport_set_mode(APP_TRANSPORT_MODE_FILESYSTEM_COMMAND);
    }
}

void fs_cmd_handle_upload_end(const uint8_t* args, uint8_t len) {
    LOG_INFO("[FS_UPLOAD] Upload end command received");
    
    if (!upload_state.active) {
        LOG_ERROR("[FS_UPLOAD] Upload end called but no active upload");
        send_framed_response(RESP_TYPE_ERROR, "No active upload");
        return;
    }

    // NOTE: No need to flush partial chunks - transport already auto-switched back to RX mode
    // at line 249 when total_bytes_received >= expected_file_size, which resets chunk buffers

    // Verify we received the expected amount of data
    LOG_INFO("[FS_UPLOAD] Checking file size: received %lu, expected %lu", 
             upload_state.total_bytes_received, upload_state.expected_file_size);
             
    if (upload_state.total_bytes_received != upload_state.expected_file_size) {
        char size_msg[100];
        snprintf(size_msg, sizeof(size_msg), "Upload END: Size mismatch - got %lu, expected %lu", 
                 (unsigned long)upload_state.total_bytes_received,
                 (unsigned long)upload_state.expected_file_size);
        send_framed_response(RESP_TYPE_ERROR, size_msg);
        LOG_ERROR("[FS_UPLOAD] Size mismatch: received %lu, expected %lu", 
                  (unsigned long)upload_state.total_bytes_received,
                  (unsigned long)upload_state.expected_file_size);
        upload_state.active = false;
        return;
    }


    // Save file table and block table to flash
    LOG_INFO("[FS_UPLOAD] Saving file table and block allocation to flash");
    extern void ffs_block_alloc_save(void);
    ffs_block_alloc_save();  // Save block metadata (including valid_bytes)
    ffs_serialize_table();   // Save file table
    LOG_INFO("[FS_UPLOAD] Flash save operations completed");

    char response[128];
    snprintf(response, sizeof(response), "Upload complete: %s (%lu bytes)", 
             upload_state.filename, (unsigned long)upload_state.total_bytes_received);
    
    LOG_INFO("[FS_UPLOAD] Sending success response: %s", response);
    send_framed_response(RESP_TYPE_SUCCESS, response);
    LOG_INFO("[FS_UPLOAD] Upload end process completed successfully");

    // Reset upload state
    memset(&upload_state, 0, sizeof(upload_state));
}