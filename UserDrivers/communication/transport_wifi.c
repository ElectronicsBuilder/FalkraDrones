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
 * @file    transport_wifi.c
 * @brief   WiFi Boot Transport Driver Implementation
 * @details WiFi transport using TCP server for bootloader communications.
 *          Supports same protocol as UART transport (STX/ETX framing, CRC).
 */

#include "transport_wifi.h"
#include "wifi_tcpServer.h"
#include "user_config.h"
#include "log.hpp"
#include "fs_command_parser.h"
#include "command_parser.h"
#include "fs_binary_upload.h"
#include "app_defs.hpp"
#include <string.h>

// ============================================================================
// External References
// ============================================================================

extern userconfig_t* g_userConfig;
extern tcp_server_context_t tcp_server_ctx;

// ============================================================================
// Private State
// ============================================================================

static WiFi_RxMode wifi_App_mode = WIFI_TRANSPORT_MODE_COMMAND;
static WiFiTransportState wifi_transport_state = WIFI_TRANSPORT_DISCONNECTED;
static bool wifi_initialized = false;

static uint8_t chunk_buf[APP_BINARY_CHUNK_SIZE];
static size_t chunk_pos = 0;

// ============================================================================
// Private Function Declarations
// ============================================================================

static bool wifi_transport_is_available(void);
static void wifi_transport_init(void);
static bool wifi_transport_poll(void);
static int wifi_transport_read(uint8_t *buf, size_t len);
static int wifi_transport_write(const uint8_t *buf, size_t len);
static void wifi_transport_flush(void);
static void wifi_transport_set_mode_impl(AppTransportMode mode);

// ============================================================================
// Public API Implementation
// ============================================================================

WiFiTransportState wifi_transport_get_state(void) {
    return wifi_transport_state;
}

void wifi_transport_set_boot_mode(WiFi_RxMode mode) {
    wifi_App_mode = mode;
    LOG_INFO("[WiFi Transport] Mode set to: %d", mode);
}

// ============================================================================
// Driver Implementation (Private)
// ============================================================================

/**
 * @brief Check if WiFi transport is available
 * @details Checks:
 *          1. WiFi credentials exist in NVRAM
 *          2. TCP server is initialized
 *          3. At least one client is connected
 * @return true if WiFi transport can be used
 */
static bool wifi_transport_is_available(void) {
    // Check if WiFi credentials are configured
    if (!g_userConfig || !userconfig_has_wifi_credentials(g_userConfig)) {
        return false;
    }

    // Check if TCP server is running and has active clients
    if (tcp_server_ctx.state != TCP_SERVER_RUNNING) {
        return false;
    }

    if (tcp_server_ctx.active_connections == 0) {
        return false;
    }

    return true;
}

/**
 * @brief Initialize WiFi transport
 * @details Initializes TCP server on bootloader port if not already running.
 *          Note: WiFi driver initialization must be done before this.
 */
static void wifi_transport_init(void) {
    LOG_INFO("[WiFi Transport] Initializing WiFi bootloader transport");

    // TCP server should already be initialized in tcpServerTask
    // Just verify it's running
    if (tcp_server_ctx.state == TCP_SERVER_RUNNING) {
        wifi_initialized = true;
        wifi_transport_state = WIFI_TRANSPORT_CONNECTED;
        LOG_INFO("[WiFi Transport] TCP server ready for bootloader traffic");
    } else {
        wifi_initialized = false;
        wifi_transport_state = WIFI_TRANSPORT_ERROR;
        LOG_ERROR("[WiFi Transport] TCP server not running");
    }
}


static void process_filesystem_binary_chunk(const uint8_t* chunk_data, size_t chunk_size) {
    // For now, just process as raw data without CRC
    // TODO: Add CRC validation later
    fs_cmd_handle_upload_chunk_raw(chunk_data, chunk_size);
}

static void flush_filesystem_partial_chunk(void) {
    // Process any remaining data in the chunk buffer
    if (chunk_pos > 0) {
        process_filesystem_binary_chunk(chunk_buf, chunk_pos);
        chunk_pos = 0;
    }
}

static size_t get_filesystem_buffer_size(void) {
    return chunk_pos;
}


/**
 * @brief Poll WiFi transport for incoming data
 * @details Processes data from active TCP clients based on current boot mode:
 *          - COMMAND: Parses command packets (STX/ETX framing)
 *          - BINARY: Accumulates firmware chunks
 *          - EXTMEM: Accumulates external memory chunks
 * @return true if data was processed
 */
static bool wifi_transport_poll(void) {
    if (!wifi_initialized || tcp_server_ctx.state != TCP_SERVER_RUNNING) {
        return false;
    }

    // Quick check: Do we even have data? (Optimization: avoids ring buffer checks)
    if (!wifi_tcp_server_has_data_hint()) {
        return false;  // Fast path: no data available
    }

    bool processed_any_data = false;

    // Process each active client
    for (int i = 0; i < TCP_SERVER_MAX_CLIENTS; i++) {
        if (!tcp_server_ctx.clients[i].active) {
            continue;
        }

        // Read available data in bulk to avoid per-byte overhead. We ask
        // the TCP server how many bytes are available and read up to that
        // amount in chunks. This copies the entire available block(s)
        // into a local buffer for processing.
        size_t available = wifi_tcp_server_get_available_data(i);
        if (available == 0) {
            continue;
        }

        // Temporary read buffer - use a static buffer sized to the per-client
        // RX ring size so we can absorb large bursts (e.g. 2052 bytes)
        // without splitting. Use static to avoid large stack allocation.
        static uint8_t temp_buf[TCP_SERVER_RX_RING_SIZE];

        while (available > 0) {
            size_t to_read = (available > sizeof(temp_buf)) ? sizeof(temp_buf) : available;
            // Read directly from the specific client's ring buffer to avoid
            // ambiguity about which client `wifi_transport_read()` would pick.
            int32_t n = wifi_tcp_server_read_client_data(i, temp_buf, to_read);
            if (n <= 0) {
                break; // nothing read or error
            }

            processed_any_data = true;

            size_t offset = 0;
            while (offset < (size_t)n) {
                switch (wifi_App_mode) {
                    case WIFI_TRANSPORT_MODE_COMMAND: {
                        // Feed each byte to the packet parser - still per-byte
                        // processing but with no transport syscall overhead.
                        process_packet_byte(  temp_buf[offset]);
                        offset++;
                        break;
                    }
                    case WIFI_TRANSPORT_MODE_FILESYSTEM_COMMAND: {
                        // Feed each byte to the packet parser - still per-byte
                        // processing but with no transport syscall overhead.
                        fs_process_packet_byte(temp_buf[offset]);
                        offset++;
                        break;
                    }

                    case WIFI_TRANSPORT_MODE_FILESYSTEM_DATA: {
                        size_t need = APP_BINARY_CHUNK_SIZE - chunk_pos;
                        size_t take = (size_t)n - offset;
                        if (take > need) take = need;
                        memcpy(&chunk_buf[chunk_pos], &temp_buf[offset], take);
                        chunk_pos += take;
                        offset += take;

                        if (chunk_pos == APP_BINARY_CHUNK_SIZE) {
                            process_filesystem_binary_chunk(chunk_buf, chunk_pos);
                            chunk_pos = 0;
                        } else if (upload_state.active) {
                            // Check if we have received all expected data for a partial final chunk
                            size_t bytes_remaining = upload_state.expected_file_size - upload_state.total_bytes_received;
                            
                            // If chunk_pos equals remaining bytes + 2 CRC, we have the final chunk
                            if (bytes_remaining > 0 && chunk_pos == bytes_remaining + 2) {
                                process_filesystem_binary_chunk(chunk_buf, chunk_pos);
                                chunk_pos = 0;
                            }
                        }
                        break;
                    }
            
                    
                }
            }

            // Re-query how much remains in the client's ring buffer
            available = wifi_tcp_server_get_available_data(i);
        }
    }

    // Clear hint if we didn't process any data (ring buffers are now empty)
    if (!processed_any_data) {
        wifi_tcp_server_clear_data_hint();
    }

    return processed_any_data;
}

/**
 * @brief Read data from WiFi transport
 * @details Reads from first active TCP client's ring buffer
 * @param buf Buffer to store read data
 * @param len Maximum bytes to read
 * @return Number of bytes read, or -1 on error
 */
static int wifi_transport_read(uint8_t *buf, size_t len) {
    if (!wifi_initialized || tcp_server_ctx.state != TCP_SERVER_RUNNING) {
        return -1;
    }

    // Find first active client and read from its ring buffer
    for (int i = 0; i < TCP_SERVER_MAX_CLIENTS; i++) {
        if (tcp_server_ctx.clients[i].active) {
            // Check for ring buffer overflow (data loss indicator)
            if (wifi_tcp_server_check_overflow(i)) {
                LOG_ERROR("[WiFi Transport] Client %d ring buffer overflowed - data may be corrupted!", i);
                wifi_tcp_server_clear_overflow(i);

                // Reset App state to COMMAND mode on overflow
                wifi_transport_set_boot_mode(WIFI_TRANSPORT_MODE_FILESYSTEM_COMMAND);
                chunk_pos = 0;
            }

            // Read from ring buffer (non-blocking)
            int32_t bytes_read = wifi_tcp_server_read_client_data(i, buf, len);

            if (bytes_read > 0) {
                LOG_DEBUG("[WiFi Transport] Read %ld bytes from client %d", bytes_read, i);
            }

            return (bytes_read >= 0) ? bytes_read : 0;
        }
    }

    return 0;  // No active clients
}

/**
 * @brief Write data to WiFi transport
 * @details Broadcasts to all active TCP clients
 * @param buf Data buffer to send
 * @param len Number of bytes to send
 * @return Number of bytes written, or -1 on error
 */
static int wifi_transport_write(const uint8_t *buf, size_t len) {
    if (!wifi_initialized || tcp_server_ctx.state != TCP_SERVER_RUNNING) {
        return -1;
    }

    // Broadcast to all connected clients
    int32_t result = wifi_tcp_server_broadcast(buf, len);
    if (result < 0) {
        LOG_ERROR("[WiFi Transport] Failed to send %d bytes", len);
        return -1;
    }

    return (int)len;
}

/**
 * @brief Flush WiFi transport buffers
 * @details No-op for WiFi (TCP handles buffering)
 */
static void wifi_transport_flush(void) {
    // TCP stack handles buffering, no action needed
}

/**
 * @brief Set WiFi transport operating mode
 * @details Maps AppTransportDriver to WiFiBootMode
 * @param mode Transport mode to set
 */
static void wifi_transport_set_mode_impl(AppTransportMode mode) {
    switch (mode) {
        case APP_TRANSPORT_MODE_COMMAND:
            wifi_transport_set_boot_mode(WIFI_TRANSPORT_MODE_COMMAND);
            break;
        case APP_TRANSPORT_MODE_DATA:
            wifi_transport_set_boot_mode(WIFI_TRANSPORT_MODE_DATA);
            chunk_pos = 0;  // Reset chunk accumulator
            break;
        case APP_TRANSPORT_MODE_FILESYSTEM_COMMAND: 
            wifi_transport_set_boot_mode(WIFI_TRANSPORT_MODE_FILESYSTEM_COMMAND);
            break;
        case APP_TRANSPORT_MODE_FILESYSTEM_DATA:
            wifi_transport_set_boot_mode(WIFI_TRANSPORT_MODE_FILESYSTEM_DATA);
            chunk_pos = 0;  // Reset chunk accumulator
            break;
        case APP_TRANSPORT_MODE_CONSOLE:  // Switch back to UART for console input
            data_transport_select(DATA_TRANSPORT_UART);
            data_transport_set_mode(APP_TRANSPORT_MODE_DATA); //This for now will be mostl likely from the a helper gui. //Todo global fix this later 
            
            break;
    }
}

// ============================================================================
// Driver Export
// ============================================================================

const AppTransportDriver wifi_transport_driver = {
    .type = DATA_TRANSPORT_WIFI,
    .name = "WiFi",
    .is_available = wifi_transport_is_available,
    .init  = wifi_transport_init,
    .poll  = wifi_transport_poll,
    .read  = wifi_transport_read,
    .write = wifi_transport_write,
    .flush = wifi_transport_flush,
    .set_mode = wifi_transport_set_mode_impl
};
