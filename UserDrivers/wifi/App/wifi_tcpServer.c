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

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "wifi_tcpServer.h"
#include "main.h"
#include "app_config.h"

#include "w6x_api.h"
#include "common_parser.h"
#include "shell.h"

#include "data_transport.h"
#include "transport_wifi.h"

#include "FreeRTOS.h"
#include "task.h"

#define TCP_ACCEPT_TASK_STACK_WORDS 1024U
#define TCP_ACCEPT_TASK_PRIORITY    (tskIDLE_PRIORITY + 3)

/* Network constants (if not defined elsewhere) */
#ifndef INADDR_ANY
#define INADDR_ANY          ((uint32_t)0x00000000UL)
#endif

/* Private variables ---------------------------------------------------------*/
tcp_server_context_t tcp_server_ctx = {0};
static TaskHandle_t tcp_accept_task_handle = NULL;

// WiFi data availability hint flag (for transport layer optimization)
static volatile bool wifi_data_available_hint = false;

/* Private function prototypes -----------------------------------------------*/
static int32_t tcp_server_accept_client(void);
static int32_t tcp_server_handle_client(int client_index);
static void tcp_server_close_client(int client_index);
static void tcp_server_reset_context(void);
static void tcp_server_accept_task(void *argument);

/* Ring buffer helpers -------------------------------------------------------*/
static void wifi_ringbuf_init(wifi_rx_ring_buffer_t* rb);
static int32_t wifi_ringbuf_push(wifi_rx_ring_buffer_t* rb, const uint8_t* data, size_t len);
static int32_t wifi_ringbuf_pop(wifi_rx_ring_buffer_t* rb, uint8_t* data, size_t len);
static size_t wifi_ringbuf_available(wifi_rx_ring_buffer_t* rb);

/* Functions Definition ------------------------------------------------------*/

int32_t wifi_tcp_server_init(uint16_t port)
{
    LogInfo("Initializing TCP Server\n");

    // Reset context
    tcp_server_reset_context();

    // Set port
    tcp_server_ctx.port = (port == 0) ? TCP_SERVER_DEFAULT_PORT : port;
    tcp_server_ctx.state = TCP_SERVER_STOPPED;

    LogInfo("TCP Server initialized on port %d\n", tcp_server_ctx.port);
    return 0;
}

int32_t wifi_tcp_server_start(void)
{
    struct sockaddr_in server_addr = {0};
    int32_t net_ret;

    if (tcp_server_ctx.state != TCP_SERVER_STOPPED)
    {
        LogError("TCP Server already running or in error state\n");
        return -1;
    }

    tcp_server_ctx.state = TCP_SERVER_STARTING;
    LogInfo("Starting TCP Server on port %d\n", tcp_server_ctx.port);

    // Create server socket
    tcp_server_ctx.server_socket = W6X_Net_Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_server_ctx.server_socket < 0)
    {
        LogError("Failed to create server socket\n");
        tcp_server_ctx.state = TCP_SERVER_ERROR;
        return -1;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = PP_HTONS(tcp_server_ctx.port);
    server_addr.sin_addr_t.s_addr = INADDR_ANY;

    // Bind socket
    net_ret = W6X_Net_Bind(tcp_server_ctx.server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (net_ret != 0)
    {
        LogError("Failed to bind server socket (%" PRId32 ")\n", net_ret);
        W6X_Net_Close(tcp_server_ctx.server_socket);
        tcp_server_ctx.state = TCP_SERVER_ERROR;
        return -1;
    }

    // Start listening
    net_ret = W6X_Net_Listen(tcp_server_ctx.server_socket, TCP_SERVER_MAX_CLIENTS);
    if (net_ret != 0)
    {
        LogError("Failed to start listening (%" PRId32 ")\n", net_ret);
        W6X_Net_Close(tcp_server_ctx.server_socket);
        tcp_server_ctx.state = TCP_SERVER_ERROR;
        return -1;
    }

    tcp_server_ctx.state = TCP_SERVER_RUNNING;

    if (tcp_accept_task_handle == NULL)
    {
        BaseType_t task_status = xTaskCreate(tcp_server_accept_task,
                                             "tcpAccept",
                                             TCP_ACCEPT_TASK_STACK_WORDS,
                                             NULL,
                                             TCP_ACCEPT_TASK_PRIORITY,
                                             &tcp_accept_task_handle);
        if (task_status != pdPASS)
        {
            LogError("Failed to create TCP accept task\n");
            tcp_accept_task_handle = NULL;
            W6X_Net_Close(tcp_server_ctx.server_socket);
            tcp_server_ctx.server_socket = -1;
            tcp_server_ctx.state = TCP_SERVER_ERROR;
            return -1;
        }
    }

    LogInfo("TCP Server started successfully, listening on port %d\n", tcp_server_ctx.port);
    return 0;
}

int32_t wifi_tcp_server_stop(void)
{
    LogInfo("Stopping TCP Server\n");

    if (tcp_accept_task_handle != NULL)
    {
        vTaskDelete(tcp_accept_task_handle);
        tcp_accept_task_handle = NULL;
    }

    // Close all client connections
    for (int i = 0; i < TCP_SERVER_MAX_CLIENTS; i++)
    {
        if (tcp_server_ctx.clients[i].active)
        {
            tcp_server_close_client(i);
        }
    }

    // Close server socket
    if (tcp_server_ctx.server_socket >= 0)
    {
        W6X_Net_Close(tcp_server_ctx.server_socket);
        tcp_server_ctx.server_socket = -1;
    }

    tcp_server_ctx.state = TCP_SERVER_STOPPED;
    LogInfo("TCP Server stopped\n");
    return 0;
}

int32_t wifi_tcp_server_process(void)
{
    int32_t clients_served = 0;

    if (tcp_server_ctx.state != TCP_SERVER_RUNNING)
    {
        return -1;
    }

    // Client acceptance handled by dedicated accept task

    // Handle existing clients
    for (int i = 0; i < TCP_SERVER_MAX_CLIENTS; i++)
    {
        if (tcp_server_ctx.clients[i].active)
        {
            int32_t result = tcp_server_handle_client(i);
            if (result > 0)
            {
                clients_served++;
            }
        }
    }

    return clients_served;
}

void wifi_tcp_server_get_status(tcp_server_state_t* state, uint32_t* active_clients, uint32_t* total_connections)
{
    if (state) *state = tcp_server_ctx.state;
    if (active_clients) *active_clients = tcp_server_ctx.active_connections;
    if (total_connections) *total_connections = tcp_server_ctx.total_connections;
}

int32_t wifi_tcp_server_broadcast(const uint8_t* data, uint32_t data_len)
{
    int32_t clients_sent = 0;

    if (tcp_server_ctx.state != TCP_SERVER_RUNNING || !data || data_len == 0)
    {
        return -1;
    }

    for (int i = 0; i < TCP_SERVER_MAX_CLIENTS; i++)
    {
        if (tcp_server_ctx.clients[i].active)
        {
            int32_t sent = W6X_Net_Send(tcp_server_ctx.clients[i].socket,
                                       (struct sockaddr*)data, data_len, 0);
            if (sent > 0)
            {
                tcp_server_ctx.clients[i].bytes_sent += sent;
                clients_sent++;
            }
        }
    }

    return clients_sent;
}

/* Private Functions ---------------------------------------------------------*/

static void tcp_server_accept_task(void *argument)
{
    (void)argument;

    for (;;)
    {
        if (tcp_server_ctx.state != TCP_SERVER_RUNNING || tcp_server_ctx.server_socket < 0)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (tcp_server_ctx.active_connections >= TCP_SERVER_MAX_CLIENTS)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (tcp_server_accept_client() <= 0)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

static int32_t tcp_server_accept_client(void)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int32_t client_socket;
    int client_index = -1;

    for (int i = 0; i < TCP_SERVER_MAX_CLIENTS; i++)
    {
        if (!tcp_server_ctx.clients[i].active)
        {
            client_index = i;
            break;
        }
    }

    if (client_index == -1)
    {
        return 0;
    }

    client_socket = W6X_Net_Accept(tcp_server_ctx.server_socket,
                                   (struct sockaddr *)&client_addr, &addr_len);

    if (client_socket < 0)
    {
        return 0;
    }

    tcp_client_info_t* client = &tcp_server_ctx.clients[client_index];
    uint32_t ip = client_addr.sin_addr_t.s_addr;
    uint16_t port = PP_NTOHS(client_addr.sin_port);
    char ip_str[sizeof(client->client_ip)];

    snprintf(ip_str, sizeof(ip_str),
             "%d.%d.%d.%d",
             (int)(ip & 0xFF),
             (int)((ip >> 8) & 0xFF),
             (int)((ip >> 16) & 0xFF),
             (int)((ip >> 24) & 0xFF));

    taskENTER_CRITICAL();
    client->socket = client_socket;
    client->connection_id = client_socket;  // For ST67W6X, socket descriptor maps to connection
    client->bytes_received = 0;
    client->bytes_sent = 0;
    client->client_port = port;
    strncpy(client->client_ip, ip_str, sizeof(client->client_ip));
    client->client_ip[sizeof(client->client_ip) - 1] = '\0';
    client->active = true;
    wifi_ringbuf_init(&client->rx_buffer);  // Initialize ring buffer for this client
    tcp_server_ctx.active_connections += 1;
    tcp_server_ctx.total_connections += 1;
    taskEXIT_CRITICAL();

    LogInfo("Client connected from %s:%d (slot %d)\n",
            client->client_ip, client->client_port, client_index);

    const char* welcome = "Hello from STM32 TCP Server!\r\n";
    W6X_Net_Send(client_socket, (struct sockaddr*)welcome, strlen(welcome), 0);

    data_transport_select(DATA_TRANSPORT_WIFI); //todo handle differently later
    data_transport_set_mode(APP_TRANSPORT_MODE_COMMAND);

    return 1;
}


static int32_t tcp_server_handle_client(int client_index)
{
    tcp_client_info_t* client = &tcp_server_ctx.clients[client_index];
    int32_t received;

    if (!client->active)
    {
        return 0;
    }

    // Receive data from WiFi module
    received = W6X_Net_Recv(client->socket, tcp_server_ctx.buffer,
                           TCP_SERVER_BUFFER_SIZE - 1, 0);

    if (received < 0)
    {
        LogError("Receive error from client %d (code: %ld)\n", client_index, received);
        /* Retry a few times for transient NCP timeouts before closing the client.
         * This helps avoid dropping the connection on intermittent W61 timeouts. */
        int retry = 0;
        const int max_retries = 3;
        while (retry < max_retries && received < 0)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            received = W6X_Net_Recv(client->socket, tcp_server_ctx.buffer, TCP_SERVER_BUFFER_SIZE - 1, 0);
            if (received > 0)
            {
                LogInfo("Receive retry succeeded on attempt %d for client %d (got %ld bytes)\n", retry + 1, client_index, received);
                break;
            }
            retry++;
        }

        if (received < 0)
        {
            LogError("Persistent receive error for client %d after %d retries, closing\n", client_index, max_retries);
            tcp_server_close_client(client_index);
            return 0;
        }
        /* else fall through and handle data as usual */
    }

    if (received == 0)
    {
        // No data available yet - not an error
        return 0;
    }

    // Update statistics
    client->bytes_received += received;

    // Push data to client's ring buffer for bootloader consumption
    int32_t pushed = wifi_ringbuf_push(&client->rx_buffer, tcp_server_ctx.buffer, received);

    if (pushed < 0)
    {
        // Ring buffer overflow - data lost!
        LogError("[TCP Server] Client %d ring buffer overflow! %ld bytes lost\n",
                 client_index, received);

        // Could optionally disconnect client on overflow
        // tcp_server_close_client(client_index);

        return 0;
    }

    LogDebug("[TCP Server] Client %d: Received %ld bytes, pushed to ring buffer (available: %zu)\n",
             client_index, received, wifi_ringbuf_available(&client->rx_buffer));
     
    return 1;
}

static void tcp_server_close_client(int client_index)
{
    tcp_client_info_t* client = &tcp_server_ctx.clients[client_index];

    if (!client->active)
    {
        return;
    }

    int32_t socket = client->socket;
    uint32_t bytes_received = client->bytes_received;
    uint32_t bytes_sent = client->bytes_sent;
    uint16_t client_port = client->client_port;
    char client_ip[sizeof(client->client_ip)];

    strncpy(client_ip, client->client_ip, sizeof(client_ip));
    client_ip[sizeof(client_ip) - 1] = '\0';

    LogInfo("Closing client %d (%s:%d) - RX: %" PRIu32 " bytes, TX: %" PRIu32 " bytes\n",
            client_index, client_ip, client_port, bytes_received, bytes_sent);

    if (socket >= 0)
    {
        W6X_Net_Close(socket);
    }

    taskENTER_CRITICAL();
    memset(client, 0, sizeof(tcp_client_info_t));
    client->socket = -1;
    client->connection_id = -1;
    wifi_ringbuf_init(&client->rx_buffer);  // Reset ring buffer
    if (tcp_server_ctx.active_connections > 0)
    {
        tcp_server_ctx.active_connections -= 1;
    }
    taskEXIT_CRITICAL();
}


static void tcp_server_reset_context(void)
{
    memset(&tcp_server_ctx, 0, sizeof(tcp_server_ctx));
    tcp_server_ctx.server_socket = -1;

    for (int i = 0; i < TCP_SERVER_MAX_CLIENTS; i++)
    {
        tcp_server_ctx.clients[i].socket = -1;
        tcp_server_ctx.clients[i].connection_id = -1;
        wifi_ringbuf_init(&tcp_server_ctx.clients[i].rx_buffer);
    }

    tcp_accept_task_handle = NULL;
}

int32_t wifi_tcp_server_test(int32_t argc, char **argv)
{
    uint16_t port = TCP_SERVER_DEFAULT_PORT;

    // Parse port argument if provided
    if (argc > 1)
    {
        port = (uint16_t)strtoul(argv[1], NULL, 10);
        if (port == 0)
        {
            port = TCP_SERVER_DEFAULT_PORT;
        }
    }

    LogInfo("Starting TCP server test on port %d\n", port);

    // Initialize and start server
    if (wifi_tcp_server_init(port) != 0)
    {
        LogError("Failed to initialize TCP server\n");
        return -1;
    }

    if (wifi_tcp_server_start() != 0)
    {
        LogError("Failed to start TCP server\n");
        return -1;
    }

    LogInfo("TCP server test started successfully\n");
    LogInfo("Server running on port %d - use 'tcpServerStop' to stop\n", port);
    return 0;
}

/* ============================================================================
 * Ring Buffer Implementation (for WiFi Bootloader Transport)
 * ============================================================================ */

/**
 * @brief Initialize ring buffer
 * @param rb Pointer to ring buffer structure
 */
static void wifi_ringbuf_init(wifi_rx_ring_buffer_t* rb)
{
    taskENTER_CRITICAL();
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->overflow = false;
    taskEXIT_CRITICAL();
}

/**
 * @brief Push data into ring buffer (thread-safe)
 * @param rb Pointer to ring buffer structure
 * @param data Data to push
 * @param len Length of data
 * @retval Number of bytes pushed, -1 if overflow
 */
static int32_t wifi_ringbuf_push(wifi_rx_ring_buffer_t* rb, const uint8_t* data, size_t len)
{
    taskENTER_CRITICAL();

    size_t available_space = TCP_SERVER_RX_RING_SIZE - rb->count;

    if (len > available_space) {
        // Buffer overflow - set flag and reject data
        rb->overflow = true;
        taskEXIT_CRITICAL();
        LogError("[Ring Buffer] Overflow! Attempted to push %zu bytes, only %zu available\n",
                 len, available_space);
        return -1;
    }

    // Push data byte by byte (circular)
    for (size_t i = 0; i < len; i++) {
        rb->buffer[rb->head] = data[i];
        rb->head = (rb->head + 1) % TCP_SERVER_RX_RING_SIZE;
    }

    rb->count += len;

    taskEXIT_CRITICAL();

    // Set hint flag (atomic write) - signals transport layer data is available
    if (len > 0) {
        wifi_data_available_hint = true;
    }

    return (int32_t)len;
}

/**
 * @brief Pop data from ring buffer (thread-safe)
 * @param rb Pointer to ring buffer structure
 * @param data Buffer to store popped data
 * @param len Maximum bytes to pop
 * @retval Number of bytes popped, 0 if empty
 */
static int32_t wifi_ringbuf_pop(wifi_rx_ring_buffer_t* rb, uint8_t* data, size_t len)
{
    taskENTER_CRITICAL();

    if (rb->count == 0) {
        taskEXIT_CRITICAL();
        return 0;  // No data available
    }

    size_t to_read = (len < rb->count) ? len : rb->count;

    // Pop data byte by byte (circular)
    for (size_t i = 0; i < to_read; i++) {
        data[i] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % TCP_SERVER_RX_RING_SIZE;
    }

    rb->count -= to_read;

    taskEXIT_CRITICAL();
    return (int32_t)to_read;
}

/**
 * @brief Get number of bytes available in ring buffer
 * @param rb Pointer to ring buffer structure
 * @retval Number of bytes available to read
 */
static size_t wifi_ringbuf_available(wifi_rx_ring_buffer_t* rb)
{
    size_t count;

    taskENTER_CRITICAL();
    count = rb->count;
    taskEXIT_CRITICAL();

    return count;
}

/* ============================================================================
 * Public Ring Buffer API (for WiFi Transport Layer)
 * ============================================================================ */

/**
 * @brief Read data from client's ring buffer (for bootloader)
 * @param client_index Index of client in tcp_server_ctx.clients array
 * @param buf Buffer to store read data
 * @param len Maximum bytes to read
 * @retval Number of bytes read, 0 if no data, -1 on error
 */
int32_t wifi_tcp_server_read_client_data(int client_index, uint8_t* buf, size_t len)
{
    if (client_index < 0 || client_index >= TCP_SERVER_MAX_CLIENTS) {
        return -1;
    }

    tcp_client_info_t* client = &tcp_server_ctx.clients[client_index];

    if (!client->active) {
        return -1;
    }

    return wifi_ringbuf_pop(&client->rx_buffer, buf, len);
}

/**
 * @brief Get number of bytes available in client's ring buffer
 * @param client_index Index of client in tcp_server_ctx.clients array
 * @retval Number of bytes available to read
 */
size_t wifi_tcp_server_get_available_data(int client_index)
{
    if (client_index < 0 || client_index >= TCP_SERVER_MAX_CLIENTS) {
        return 0;
    }

    tcp_client_info_t* client = &tcp_server_ctx.clients[client_index];

    if (!client->active) {
        return 0;
    }

    return wifi_ringbuf_available(&client->rx_buffer);
}

/**
 * @brief Check if client's ring buffer has overflowed
 * @param client_index Index of client in tcp_server_ctx.clients array
 * @retval true if overflow occurred, false otherwise
 */
bool wifi_tcp_server_check_overflow(int client_index)
{
    if (client_index < 0 || client_index >= TCP_SERVER_MAX_CLIENTS) {
        return false;
    }

    tcp_client_info_t* client = &tcp_server_ctx.clients[client_index];

    bool overflow;
    taskENTER_CRITICAL();
    overflow = client->rx_buffer.overflow;
    taskEXIT_CRITICAL();

    return overflow;
}

/**
 * @brief Clear client's ring buffer overflow flag
 * @param client_index Index of client in tcp_server_ctx.clients array
 */
void wifi_tcp_server_clear_overflow(int client_index)
{
    if (client_index < 0 || client_index >= TCP_SERVER_MAX_CLIENTS) {
        return;
    }

    tcp_client_info_t* client = &tcp_server_ctx.clients[client_index];

    taskENTER_CRITICAL();
    client->rx_buffer.overflow = false;
    taskEXIT_CRITICAL();
}

/**
 * @brief Check if WiFi data availability hint flag is set
 * @details Fast non-blocking check to optimize transport polling.
 *          This is a hint only - false positives are harmless.
 * @retval true if data may be available, false if definitely no data
 */
bool wifi_tcp_server_has_data_hint(void)
{
    return wifi_data_available_hint;
}

/**
 * @brief Clear WiFi data availability hint flag
 * @details Call after processing all available data from ring buffers.
 *          Safe to call from any task.
 */
void wifi_tcp_server_clear_data_hint(void)
{
    wifi_data_available_hint = false;
}