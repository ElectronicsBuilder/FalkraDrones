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
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef WIFI_TCP_SERVER_H
#define WIFI_TCP_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Exported constants --------------------------------------------------------*/
#define TCP_SERVER_BUFFER_SIZE      2048
#define TCP_SERVER_MAX_CLIENTS      4
#define TCP_SERVER_DEFAULT_PORT     8080
#define TCP_SERVER_RX_RING_SIZE     4096  // 4KB ring buffer per client for bootloader data

/* Exported types ------------------------------------------------------------*/
typedef enum {
    TCP_SERVER_STOPPED = 0,
    TCP_SERVER_STARTING,
    TCP_SERVER_RUNNING,
    TCP_SERVER_ERROR
} tcp_server_state_t;

/**
 * @brief Per-client ring buffer for bootloader data reception
 * @details Thread-safe ring buffer using FreeRTOS critical sections.
 *          Decouples TCP reception (tcp_server_handle_client) from
 *          bootloader consumption (wifi_transport_read).
 */
typedef struct {
    uint8_t buffer[TCP_SERVER_RX_RING_SIZE];  // Ring buffer storage
    volatile size_t head;                      // Write position (producer: TCP server)
    volatile size_t tail;                      // Read position (consumer: bootloader)
    volatile size_t count;                     // Available bytes
    volatile bool overflow;                    // Overflow flag (data loss indicator)
} wifi_rx_ring_buffer_t;

typedef struct {
    int32_t socket;
    int32_t connection_id;                     // ST67W6X connection ID (p_net_ctx->Sockets[sock].Number)
    bool active;
    uint32_t bytes_received;
    uint32_t bytes_sent;
    char client_ip[16];
    uint16_t client_port;
    wifi_rx_ring_buffer_t rx_buffer;           // Per-client bootloader data ring buffer
} tcp_client_info_t;

typedef struct {
    tcp_server_state_t state;
    int32_t server_socket;
    uint16_t port;
    uint32_t total_connections;
    uint32_t active_connections;
    tcp_client_info_t clients[TCP_SERVER_MAX_CLIENTS];
    uint8_t buffer[TCP_SERVER_BUFFER_SIZE];
} tcp_server_context_t;

/* Exported variables --------------------------------------------------------*/
extern tcp_server_context_t tcp_server_ctx;

/* Exported functions --------------------------------------------------------*/
/**
 * @brief Initialize TCP server
 * @param port Port number to listen on (0 uses default port)
 * @retval 0 on success, -1 on error
 */
int32_t wifi_tcp_server_init(uint16_t port);

/**
 * @brief Start TCP server (non-blocking)
 * @retval 0 on success, -1 on error
 */
int32_t wifi_tcp_server_start(void);

/**
 * @brief Stop TCP server and close all connections
 * @retval 0 on success, -1 on error
 */
int32_t wifi_tcp_server_stop(void);

/**
 * @brief Process server operations (call periodically)
 * @retval Number of clients served, -1 on error
 */
int32_t wifi_tcp_server_process(void);

/**
 * @brief Get server status information
 * @param state Pointer to store current state
 * @param active_clients Pointer to store active client count
 * @param total_connections Pointer to store total connection count
 */
void wifi_tcp_server_get_status(tcp_server_state_t* state, uint32_t* active_clients, uint32_t* total_connections);

/**
 * @brief Send data to all connected clients
 * @param data Data buffer to send
 * @param data_len Length of data
 * @retval Number of clients data was sent to, -1 on error
 */
int32_t wifi_tcp_server_broadcast(const uint8_t* data, uint32_t data_len);

/**
 * @brief Simple TCP server test with default parameters
 * @param argc Number of arguments
 * @param argv Pointer to the arguments
 * @retval 0 on success, -1 on error
 */
int32_t wifi_tcp_server_test(int32_t argc, char **argv);

/**
 * @brief Read data from client's ring buffer (for bootloader)
 * @param client_index Index of client in tcp_server_ctx.clients array
 * @param buf Buffer to store read data
 * @param len Maximum bytes to read
 * @retval Number of bytes read, 0 if no data, -1 on error
 */
int32_t wifi_tcp_server_read_client_data(int client_index, uint8_t* buf, size_t len);

/**
 * @brief Get number of bytes available in client's ring buffer
 * @param client_index Index of client in tcp_server_ctx.clients array
 * @retval Number of bytes available to read
 */
size_t wifi_tcp_server_get_available_data(int client_index);

/**
 * @brief Check if client's ring buffer has overflowed
 * @param client_index Index of client in tcp_server_ctx.clients array
 * @retval true if overflow occurred, false otherwise
 */
bool wifi_tcp_server_check_overflow(int client_index);

/**
 * @brief Clear client's ring buffer overflow flag
 * @param client_index Index of client in tcp_server_ctx.clients array
 */
void wifi_tcp_server_clear_overflow(int client_index);

/**
 * @brief Check if WiFi data availability hint flag is set
 * @details Fast non-blocking check to optimize transport polling.
 *          This is a hint only - false positives are harmless.
 * @retval true if data may be available, false if definitely no data
 */
bool wifi_tcp_server_has_data_hint(void);

/**
 * @brief Clear WiFi data availability hint flag
 * @details Call after processing all available data from ring buffers.
 *          Safe to call from any task.
 */
void wifi_tcp_server_clear_data_hint(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_TCP_SERVER_H */