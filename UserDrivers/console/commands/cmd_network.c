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
 * @file    cmd_network.c
 * @brief   TCP/network console commands
 */

#include "console_internal.h"
#include "wifi_tcpserver.h"
#include "stm32f7xx_hal.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>

// External TCP client queue from main_cpp.cpp
extern QueueHandle_t tcpClientQueue;
#define TCP_MSG_MAX_LEN 256

// ============================================================================
// Command Handlers - TCP Client
// ============================================================================

static bool cmd_send_tcp(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* message = console_get_arg(cmd_buffer, "SEND_TCP");
    if (!message || strlen(message) == 0) {
        console_send(huart, "\r\n[ERROR] Usage: SEND_TCP:your message here\r\n");
        return true;
    }

    if (strlen(message) >= TCP_MSG_MAX_LEN) {
        console_send(huart, "\r\n[ERROR] Message too long (max 255 chars)\r\n");
        return true;
    }

    // Send message to TCP client task via queue
    char tcp_message[TCP_MSG_MAX_LEN];
    strncpy(tcp_message, message, TCP_MSG_MAX_LEN - 1);
    tcp_message[TCP_MSG_MAX_LEN - 1] = '\0';

    if (tcpClientQueue != NULL) {
        if (xQueueSend(tcpClientQueue, tcp_message, pdMS_TO_TICKS(100)) == pdTRUE) {
            char msg[128];
            snprintf(msg, sizeof(msg), "\r\n[INFO] Queued message for TCP transmission: %s\r\n", message);
            console_send(huart, msg);
        } else {
            console_send(huart, "\r\n[ERROR] Failed to queue message (queue full or timeout)\r\n");
        }
    } else {
        console_send(huart, "\r\n[ERROR] TCP client queue not available\r\n");
    }

    return true;
}

// ============================================================================
// Command Handlers - TCP Server
// ============================================================================

static bool cmd_tcp_server_start(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Starting TCP server...\r\n");

    if (wifi_tcp_server_init(0) == 0 && wifi_tcp_server_start() == 0) {
        console_send(huart, "[INFO] TCP server started successfully on port 8080\r\n");
    } else {
        console_send(huart, "[ERROR] Failed to start TCP server\r\n");
    }

    return true;
}

static bool cmd_tcp_server_stop(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] Stopping TCP server...\r\n");

    if (wifi_tcp_server_stop() == 0) {
        console_send(huart, "[INFO] TCP server stopped successfully\r\n");
    } else {
        console_send(huart, "[ERROR] Failed to stop TCP server\r\n");
    }

    return true;
}

static bool cmd_tcp_server_status(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    console_send(huart, "\r\n[INFO] TCP server status:\r\n");

    tcp_server_state_t state;
    uint32_t active_clients, total_connections;
    wifi_tcp_server_get_status(&state, &active_clients, &total_connections);

    const char* state_str = (state == TCP_SERVER_STOPPED) ? "STOPPED" :
                           (state == TCP_SERVER_STARTING) ? "STARTING" :
                           (state == TCP_SERVER_RUNNING) ? "RUNNING" :
                           (state == TCP_SERVER_ERROR) ? "ERROR" : "UNKNOWN";

    char status_msg[256];
    snprintf(status_msg, sizeof(status_msg),
            "[STATUS] State: %s, Active Clients: %lu, Total Connections: %lu\r\n",
            state_str, active_clients, total_connections);
    console_send(huart, status_msg);

    return true;
}

static bool cmd_broadcast_tcp(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* message = console_get_arg(cmd_buffer, "BROADCAST_TCP");
    if (!message || strlen(message) == 0) {
        console_send(huart, "\r\n[ERROR] Usage: BROADCAST_TCP:your message here\r\n");
        return true;
    }

    // Broadcast message to all connected clients
    int32_t clients_sent = wifi_tcp_server_broadcast((const uint8_t*)message, strlen(message));

    if (clients_sent > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "\r\n[TCP_SERVER] Broadcast sent to %ld client(s)\r\n", clients_sent);
        console_send(huart, msg);
    } else if (clients_sent == 0) {
        console_send(huart, "\r\n[TCP_SERVER] No clients connected to broadcast to\r\n");
    } else {
        console_send(huart, "\r\n[TCP_SERVER] Broadcast failed\r\n");
    }

    return true;
}

// ============================================================================
// Command Registration
// ============================================================================

const console_command_t network_commands[] = {
    {
        .command = "SEND_TCP",
        .handler = cmd_send_tcp,
        .help_text = "Send message via TCP client"
    },
    {
        .command = "tcpServerStart",
        .handler = cmd_tcp_server_start,
        .help_text = "Start TCP server"
    },
    {
        .command = "tcpServerStop",
        .handler = cmd_tcp_server_stop,
        .help_text = "Stop TCP server"
    },
    {
        .command = "tcpServerStatus",
        .handler = cmd_tcp_server_status,
        .help_text = "Get TCP server status"
    },
    {
        .command = "BROADCAST_TCP",
        .handler = cmd_broadcast_tcp,
        .help_text = "Broadcast message to all TCP clients"
    }
};

const size_t network_commands_count = sizeof(network_commands) / sizeof(network_commands[0]);
