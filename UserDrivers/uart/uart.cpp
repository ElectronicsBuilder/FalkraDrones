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
 * @file    uart.cpp
 * @brief   UART Communication Driver for STM32F767
 * @details Implements UART/USART communication interface for telemetry,
 *          debugging, and command processing. Supports DMA-based transfers,
 *          circular buffering, and optional RTOS integration for command queuing.
 */

#include "uart.hpp"
#include "log.hpp"
#include "stm32f7xx_hal.h"
#include "app_defs.hpp"
#include "transport_uart.h"
#include <string.h>
#include <stdbool.h>



extern UART_HandleTypeDef huart1;

UART_RxMode uart_rx_mode = UART_TRANSPORT_MODE_COMMAND;
static UART_RxMode current_rx_mode = UART_TRANSPORT_MODE_CONSOLE;
static app_uart_mode_t uart_mode = UART_MODE_DMA;

static const char* prompt = "\r\n\033[36m[Falkra >>] \033[0m";

#define UART_RING_SIZE 2048

// === Ring Buffer ===
static uint8_t uart_ring_data[UART_RING_SIZE];
static size_t ring_head = 0;
static size_t ring_tail = 0;

static bool ring_buffer_empty(void) {
    return ring_head == ring_tail;
}

static bool ring_buffer_write(uint8_t byte) {
    size_t next_head = (ring_head + 1) % UART_RING_SIZE;
    if (next_head == ring_tail) return false;  // overflow
    uart_ring_data[ring_head] = byte;
    ring_head = next_head;
    return true;
}

static bool ring_buffer_read(uint8_t *out) {
    if (ring_buffer_empty()) return false;
    *out = uart_ring_data[ring_tail];
    ring_tail = (ring_tail + 1) % UART_RING_SIZE;
    return true;
}

#define UART_DMA_BUFFER_SIZE 2048
static uint8_t uart_dma_rx_buf[UART_DMA_BUFFER_SIZE];
static volatile uint16_t last_pos = 0;

void uart_set_mode(UART_RxMode mode) {
    current_rx_mode = mode;
    uart_rx_mode = mode;
}

void uart_set_transport_mode(app_uart_mode_t mode) {
    uart_mode = mode;
    if (mode == UART_MODE_IRQ) {
        ring_head = ring_tail = 0;
        uart_init_rx();
    } else if (mode == UART_MODE_DMA) {
        uart_init_rx_dma();
    }
}

bool uart_data_available(void) {
    switch (uart_mode) {
        case UART_MODE_IRQ:
            return !ring_buffer_empty();
        case UART_MODE_DMA:
            return true;
        case UART_MODE_SIMPLE:
            return true;
        default:
            return false;
    }
}

bool uart_rx_pending(void) {
    // Non-consuming check for unread RX bytes. Unlike uart_data_available(),
    // the DMA case compares the actual ring position, so this is usable as an
    // "operator typed something" signal inside long-running console commands.
    switch (uart_mode) {
        case UART_MODE_IRQ:
            return !ring_buffer_empty();
        case UART_MODE_DMA: {
            uint16_t current_pos = UART_DMA_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx);
            return last_pos != current_pos;
        }
        default:
            return false;
    }
}

int uart_read_buffer(uint8_t *buf, size_t len) {
    if (!buf || len == 0) return 0;
    int read = 0;

    switch (uart_mode) {
        case UART_MODE_IRQ:
            while (read < len && ring_buffer_read(&buf[read]))
                read++;
            return read;

        case UART_MODE_DMA: {
            uint16_t current_pos = UART_DMA_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx);
            while (last_pos != current_pos && read < len) {
                buf[read++] = uart_dma_rx_buf[last_pos++];
                if (last_pos >= UART_DMA_BUFFER_SIZE)
                    last_pos = 0;
            }
            return read;
        }

        case UART_MODE_SIMPLE:
            if (HAL_UART_Receive(&huart1, buf, len, HAL_MAX_DELAY) == HAL_OK)
                return len;
            else
                return 0;

        default:
            return 0;
    }
}

void uart_send_string(const char *msg) {
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

void uart_send_bytes(const uint8_t *data, size_t len) {
    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, HAL_MAX_DELAY);
}

void uart_wait_tx_complete(void) {
    // Wait until UART transmission is complete
    // This ensures the TX buffer is empty before proceeding
    uint32_t timeout = 1000; 
    uint32_t start = HAL_GetTick();

    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET) {
        if ((HAL_GetTick() - start) > timeout) {
            break; // Timeout - exit to prevent infinite loop
        }
    }
}

static uint8_t rx_byte = 0;
void uart_init_rx() {
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1 && uart_mode == UART_MODE_IRQ) {
        ring_buffer_write(rx_byte);
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

void uart_init_rx_dma() {
    last_pos = 0;
    HAL_UART_Receive_DMA(&huart1, uart_dma_rx_buf, UART_DMA_BUFFER_SIZE);
    HAL_UART_Transmit(&huart1, (uint8_t*)prompt, strlen(prompt), HAL_MAX_DELAY);
}

void clear_ring_buffer() {

    ring_head = ring_tail = 0;
    memset(uart_dma_rx_buf, 0, UART_DMA_BUFFER_SIZE);
}

void uart_dma_poll() {
    // Not used in new design; handled by uart_read_buffer
}

void uart_command_queue_init(void) {
#if BOOTLOADER_USE_RTOS
    uart_cmd_queue = xQueueCreate(64, sizeof(uint8_t));
#endif
}









