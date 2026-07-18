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
 * @file    tcp_server.c
 * @brief   TCP Server Task Implementation
 * @details FreeRTOS task implementation for TCP server operations.
 *          Manages WiFi TCP server lifecycle, client connections, and status monitoring.
 */

#include "tcp_server.h"
#include "main.h"
#include "log.hpp"

#include "wifi_tcpClient.h"
#include "wifi_tcpServer.h"
#include "main_app.h"

// ============================================================================
// Private Function Declarations
// ============================================================================

static void waitWifiInit(void);

// ============================================================================
// External Variables
// ============================================================================

extern bool TouchGFX_init;
extern bool wifiDriverInit;
extern bool PeripheralsTestComplete;
// ============================================================================
// TCP Server Task Implementation
// ============================================================================

/**
 * @brief TCP Server FreeRTOS task
 * @details Main task function that manages TCP server operations:
 *          1. Waits for TouchGFX initialization
 *          2. Waits for WiFi driver initialization
 *          3. Initializes TCP server on default port (8080)
 *          4. Starts TCP server
 *          5. Processes client connections in main loop
 *          6. Logs server status every 30 seconds
 * @param argument Task argument (unused)
 */
void tcpServerTask(void *argument)
{
    osDelay(6000);
    // Wait for TouchGFX to be initialized
    while (!TouchGFX_init && !PeripheralsTestComplete) {
        osDelay(100);
    }
    main_app();
    // Wait for Wi-Fi to be initialized
    waitWifiInit();

    LOG_INFO("[TCP Server] Task started, initializing server...");

    // Initialize TCP server on default port (8080)
    if (wifi_tcp_server_init(0) != 0) {
        LOG_ERROR("[TCP Server] Failed to initialize server");
        goto error_exit;
    }

    // Start TCP server
    if (wifi_tcp_server_start() != 0) {
        LOG_ERROR("[TCP Server] Failed to start server");
        goto error_exit;
    }

    LOG_INFO("[TCP Server] Server started successfully");

    // Main server loop
     for (;;) {
        // Process server operations
        int32_t clients_served = wifi_tcp_server_process();

        if (clients_served < 0) {
            LOG_ERROR("[TCP Server] Error processing server operations");
            break;
        }

        // Give other tasks a chance to run
        osDelay(1);  // Reduced from 100ms to 10ms for better responsiveness

        // Periodically log server status
        static uint32_t last_status_log = 0;
        uint32_t now = HAL_GetTick();
        if (now - last_status_log > 30000) { // Every 30 seconds
            tcp_server_state_t state;
            uint32_t active_clients, total_connections;
            wifi_tcp_server_get_status(&state, &active_clients, &total_connections);

            LOG_INFO("[TCP Server] Status: State=%d, Active=%lu, Total=%lu",
                     state, active_clients, total_connections);
            last_status_log = now;
        }

     }

error_exit:
    LOG_ERROR("[TCP Server] Task exiting due to error");
    wifi_tcp_server_stop();

    // Task should not exit, but if it does, suspend itself
    for (;;) {
        osDelay(10000);
     }
}

// ============================================================================
// Private Helper Functions
// ============================================================================

/**
 * @brief Wait for WiFi driver initialization
 * @details Blocks until wifiDriverInit flag is set to true.
 *          Polls every 1 second to check initialization status.
 */
static void waitWifiInit(void)
{
    do {
        osDelay(1000);
    } while (wifiDriverInit != true);
}
