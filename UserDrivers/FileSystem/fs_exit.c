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
 * @file    fs_exit.c
 * @brief   Filesystem exit command handler implementation
 */

#include "fs_exit.h"
#include "app_reply.h"
#include "log.hpp"
#include "uart.hpp"
#include "app_defs.hpp"
#include "data_transport.h"

void fs_cmd_handle_exit(const uint8_t* args, uint8_t len) {
    LOG_INFO("[FS_EXIT] Exit filesystem command received - returning to normal shell mode");

    send_framed_response(RESP_TYPE_INFO, "Exiting filesystem mode, returning to shell");

    // Wait for all pending UART transmissions to complete before mode switch
    // This ensures framed responses are fully sent before switching to console mode
    HAL_Delay(100);  // Increased delay to allow TX buffer to clear
    uart_wait_tx_complete();  // Wait for UART transmission to finish

    // Switch mode back to normal console
    data_transport_select(DATA_TRANSPORT_UART);             // on exit switch to uart console log.
    data_transport_set_mode(APP_TRANSPORT_MODE_CONSOLE);    //todo work on this later
    clear_ring_buffer(); // Clear any residual data in UART RX buffer

    LOG_INFO("[FS_EXIT] UART mode switched back to CONSOLE - shell ready");
}