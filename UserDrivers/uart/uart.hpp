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
 * @file    uart.hpp
 * @brief   UART Communication Driver Interface for STM32F767
 * @details Interface for UART/USART communication including DMA transfers,
 *          circular buffering, and RTOS integration. Supports multiple modes
 *          for telemetry, debugging, and command processing on FalkraController.
 */

#ifndef __UART_HPP
#define __UART_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "app_defs.hpp"
#include "transport_uart.h"

extern UART_RxMode uart_rx_mode;

// Setup
void uart_set_mode(UART_RxMode mode);
void uart_set_transport_mode(app_uart_mode_t mode);

// Transport interface
bool uart_data_available(void);
int  uart_read_buffer(uint8_t *buf, size_t len);
void uart_send_string(const char *msg);
void uart_send_bytes(const uint8_t *data, size_t len);
void uart_wait_tx_complete(void);

// Internal
void uart_init_rx();
void uart_init_rx_dma();
void uart_dma_poll();
void clear_ring_buffer();
// Optional
void uart_command_queue_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __UART_HPP */
