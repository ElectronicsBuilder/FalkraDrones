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
 * @file    transport_uart.h
 * @brief   UART transport driver
 */
#ifndef __TRANSPORT_UART_H
#define __TRANSPORT_UART_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "data_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi transport operating modes
 * @details Maps to BootTransportMode for consistency with UART transport
 */
typedef enum {
    UART_TRANSPORT_MODE_CONSOLE = 0,
    UART_TRANSPORT_MODE_COMMAND,    // Receiving commands
    UART_TRANSPORT_MODE_DATA,         // Receiving Binary chunks
    UART_TRANSPORT_MODE_EXTMEM,          // Receiving external memory chunks
    UART_TRANSPORT_MODE_FILESYSTEM_COMMAND, // Filesystem command mode
    UART_TRANSPORT_MODE_FILESYSTEM_DATA // Filesystem data mode       
} UART_RxMode;

/**
 * @brief WiFi transport state
 */
typedef enum {
    UART_TRANSPORT_DISCONNECTED = 0,
    UART_TRANSPORT_CONNECTING,
    UART_TRANSPORT_CONNECTED,
    UART_TRANSPORT_ERROR
} UARTTransportState;



extern const AppTransportDriver uart_transport_driver;



#ifdef __cplusplus
}
#endif

#endif /* __TRANSPORT_UART_H */


