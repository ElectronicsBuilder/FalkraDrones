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
#include <inttypes.h>

#include "wifi_tcpClient.h"
#include "main.h"
#include "app_config.h"

#include "w6x_api.h"
#include "common_parser.h"
#include "shell.h"

/* Private variables ---------------------------------------------------------*/
static uint8_t tcp_buffer[TCP_TEST_BUFFER_SIZE];

/* Private function prototypes -----------------------------------------------*/

/* Functions Definition ------------------------------------------------------*/
int32_t wifi_tcp_test(const char* server_ip, uint16_t server_port, const uint8_t* test_data, uint32_t data_len)
{
    struct sockaddr_in addr_t = {0};
    uint8_t server_addr[4] = {0};
    uint32_t tstart, tstop;
    uint32_t transferred_bytes = 0;
    int32_t net_ret = 0;
    int32_t sock;

    LogInfo("Starting TCP test to %s:%d\n", server_ip, server_port);

    /* Create TCP socket */
    LogInfo("Creating TCP socket\n");
    sock = W6X_Net_Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
        LogError("Socket creation failed\n");
        return -1;
    }

    /* Note: ST67W6X API doesn't support W6X_Net_SetSockOpt - sockets are non-blocking by default */

    /* Resolve IP Address */
    net_ret = W6X_Net_GetHostAddress((char*)server_ip, server_addr);
    if (net_ret != W6X_STATUS_OK)
    {
        LogError("Failed to resolve IP address for %s\n", server_ip);
        goto error_exit;
    }
    LogInfo("Resolved IP: " IPSTR "\n", IP2STR(server_addr));

    /* Setup address structure */
    addr_t.sin_family = AF_INET;
    addr_t.sin_port = PP_HTONS(server_port);
    addr_t.sin_addr_t.s_addr = ATON_R(server_addr);

    /* Connect to server */
    LogInfo("Connecting to server...\n");
    net_ret = W6X_Net_Connect(sock, (struct sockaddr *)&addr_t, sizeof(addr_t));
    if (net_ret != 0)
    {
        LogError("Connection failed\n");
        goto error_exit;
    }
    LogInfo("Connected successfully\n");

    /* Send test data */
    tstart = HAL_GetTick();
    transferred_bytes = 0;

    do
    {
        int32_t count_done = W6X_Net_Send(sock, (struct sockaddr *)&test_data[transferred_bytes],
                                          data_len - transferred_bytes, 0);
        if (count_done < 0)
        {
            LogError("Send failed (%" PRId32 ")\n", count_done);
            goto error_exit;
        }
        transferred_bytes += count_done;
    } while (transferred_bytes < data_len);

    LogInfo("Sent %" PRIu32 " bytes\n", transferred_bytes);

    /* Ensure data is actually transmitted before expecting response */
    LogInfo("Waiting for server to process and respond...\n");
    osDelay(500);  // Increased delay to 500ms

    /* Receive response */
    memset(tcp_buffer, 0, sizeof(tcp_buffer));
    transferred_bytes = 0;

    // Wait for and receive response with timeout
    uint32_t timeout_start = HAL_GetTick();
    uint32_t timeout_ms = 3000;  // 3 second timeout (server responds immediately)
    uint32_t attempt_count = 0;

    LogInfo("Starting receive loop...\n");
    while (transferred_bytes < sizeof(tcp_buffer) - 1)
    {
        attempt_count++;
        int32_t count_done = W6X_Net_Recv(sock, &tcp_buffer[transferred_bytes],
                                          sizeof(tcp_buffer) - transferred_bytes - 1, 0);

        if (attempt_count % 50 == 1)  // Log every 50th attempt to see progress
        {
            LogInfo("Receive attempt %" PRIu32 ", returned %" PRId32 ", elapsed: %" PRIu32 "ms\n",
                    attempt_count, count_done, HAL_GetTick() - timeout_start);
        }

        if (count_done < 0)
        {
            LogError("Receive failed (%" PRId32 ") after %" PRIu32 " attempts\n", count_done, attempt_count);
            break;
        }
        if (count_done > 0)
        {
            // Data received successfully
            transferred_bytes += count_done;
            LogInfo("SUCCESS: Received %" PRId32 " bytes in attempt %" PRIu32 "\n", count_done, attempt_count);
            break;  // Got some data, that's enough for now
        }

        // count_done == 0 means no data available yet, check timeout
        if (HAL_GetTick() - timeout_start > timeout_ms)
        {
            LogError("Receive timeout after %" PRIu32 " ms (%" PRIu32 " attempts)\n", timeout_ms, attempt_count);
            break;
        }

        // Small delay before next receive attempt (but check once immediately)
        if (attempt_count > 1) {
            osDelay(10);
        }
    }

    tstop = HAL_GetTick();

    LogInfo("Received %" PRIu32 " bytes in %" PRIu32 " ms\n", transferred_bytes, tstop - tstart);

    if (transferred_bytes > 0)
    {
        tcp_buffer[transferred_bytes] = '\0';
        LogInfo("Response: %s\n", (char*)tcp_buffer);
    }

    /* Close socket */
    W6X_Net_Close(sock);
    LogInfo("TCP test completed successfully\n");
    return 0;

error_exit:
    W6X_Net_Close(sock);
    return -1;
}

int32_t wifi_tcp_simple_test(int32_t argc, char **argv)
{
    const char* default_server = "192.168.1.250";  // Default Python server IP - UPDATE WITH YOUR PC'S IP
    //TODO: cleanup, update to make this user settable
    uint16_t default_port = 8080;                   // Default Python server port
    const char* test_message = "Hello from STM32 FalkraDrones!\n";

    char* server_ip = (char*)default_server;
    uint16_t server_port = default_port;

    /* Parse command line arguments */
    if (argc >= 2)
    {
        server_ip = argv[1];
    }
    if (argc >= 3)
    {
        server_port = (uint16_t)atoi(argv[2]);
    }

    LogInfo("TCP Simple Test - Server: %s:%d\n", server_ip, server_port);

    return wifi_tcp_test(server_ip, server_port, (const uint8_t*)test_message, strlen(test_message));
}

SHELL_CMD_EXPORT_ALIAS(wifi_tcp_simple_test, tcptest, tcptest [server_ip] [port]);