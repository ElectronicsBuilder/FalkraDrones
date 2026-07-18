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
 * @file    transport_uart.c
 * @brief   UART transport driver
 */
#include "transport_uart.h"
#include "uart.hpp"
#include "app_defs.hpp"
#include "console.h"
#include "fs_command_parser.h"
#include "command_parser.h"
#include "fs_binary_upload.h"
#include "stm32f7xx_hal.h"
#include <string.h>


extern UART_HandleTypeDef huart1;


static app_uart_mode_t uart_mode = APP_UART_MODE_SELECTED;

uint8_t chunk_buf[APP_BINARY_CHUNK_SIZE];
static size_t chunk_pos = 0;

static void uart_transport_init(void) {
    uart_set_transport_mode(uart_mode);
    uart_set_mode(UART_TRANSPORT_MODE_CONSOLE);
}


static bool uart_is_available(void) {
    // UART is always available on this platform
    return true;
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

static void uart_set_mode_impl(AppTransportMode mode) {
    switch (mode) {
        case APP_TRANSPORT_MODE_COMMAND:
            uart_set_mode(UART_TRANSPORT_MODE_COMMAND);
            break;
        case APP_TRANSPORT_MODE_DATA:
            uart_set_mode(UART_TRANSPORT_MODE_DATA);
            chunk_pos = 0;
            break;
        case APP_TRANSPORT_MODE_FILESYSTEM_COMMAND:
            uart_set_mode(UART_TRANSPORT_MODE_FILESYSTEM_COMMAND);  
            break;
        case APP_TRANSPORT_MODE_FILESYSTEM_DATA:
            uart_set_mode(UART_TRANSPORT_MODE_FILESYSTEM_DATA);
            chunk_pos = 0;
            break;
        case APP_TRANSPORT_MODE_CONSOLE:
            uart_set_mode(UART_TRANSPORT_MODE_CONSOLE);
            break;
        default:
            uart_set_mode(UART_TRANSPORT_MODE_CONSOLE);
            break;
    }
}

static bool uart_transport_poll(void) {
    uint8_t byte;

    while (uart_read_buffer(&byte, 1) == 1) {
        switch (uart_rx_mode) {
            case UART_TRANSPORT_MODE_CONSOLE:
                uart_handle_CONSOLE(byte);
                break;
            case UART_TRANSPORT_MODE_COMMAND:
                process_packet_byte(byte);
                break;

            case UART_TRANSPORT_MODE_FILESYSTEM_COMMAND:
                fs_process_packet_byte(byte);
                
                break;

            case UART_TRANSPORT_MODE_FILESYSTEM_DATA:
                chunk_buf[chunk_pos++] = byte;
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

    return true;
}

static int uart_transport_read(uint8_t *buf, size_t len) {
    return uart_read_buffer(buf, len);
}

static int uart_transport_write(const uint8_t *buf, size_t len) {
    return (HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY) == HAL_OK) ? (int)len : -1;
}

static void uart_transport_flush(void) {
    // no-op for now
}

const AppTransportDriver uart_transport_driver = {
    .type = DATA_TRANSPORT_UART,
    .name = "UART",
    .is_available = uart_is_available,
    .init  = uart_transport_init,
    .poll  = uart_transport_poll,
    .read  = uart_transport_read,
    .write = uart_transport_write,
    .flush = uart_transport_flush,
    .set_mode = uart_set_mode_impl
};

