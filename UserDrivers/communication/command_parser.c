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
 * @file    command_parser.c
 * @brief   Bootloader Implementation
 */

#include "command_parser.h"
#include "log.hpp"
#include "app_defs.hpp"
#include "app_reply.h"
#include "boot_fuse.hpp"
#include "data_transport.h"
#include "boot_fuse_qspiFlash.hpp"


#include <string.h>



// Packet: [STX][LEN][CMD][ARGS...][CRC][ETX]
typedef enum {
    CMD_STATE_WAIT_START,
    CMD_STATE_LEN,
    CMD_STATE_DATA,
    CMD_STATE_WAIT_END
} CmdParserState;

static CmdParserState state = CMD_STATE_WAIT_START;
static uint8_t buffer[128];  // Increased for safety against UART corruption
static uint8_t cmd_index = 0;
static uint8_t expected_len = 0;

void reset_command_parser(void)
{
    state = CMD_STATE_WAIT_START;
    cmd_index = 0;
    expected_len = 0;
    LOG_DEBUG("Command parser reset due to corruption");
}

void process_packet_byte(uint8_t byte)
{
    switch (state) {
        case CMD_STATE_WAIT_START:
            if (byte == APP_CMD_STX) {
                cmd_index = 0;
                state = CMD_STATE_LEN;
            }
            break;

        case CMD_STATE_LEN:
            expected_len = byte;
            if (expected_len < 2 || expected_len >= sizeof(buffer)) {
                LOG_ERROR("Invalid length: %u (max: %u) - possible UART corruption", expected_len, sizeof(buffer)-1);
                // Reset parser state and clear any corrupted data
                cmd_index = 0;
                expected_len = 0;
                state = CMD_STATE_WAIT_START;
                break;
            }
            buffer[cmd_index++] = byte; // save LEN
            state = CMD_STATE_DATA;
            break;

        case CMD_STATE_DATA:
            buffer[cmd_index++] = byte;
            if (cmd_index >= expected_len + 1) {  // +1 for LEN byte
                state = CMD_STATE_WAIT_END;
            }
            break;

        case CMD_STATE_WAIT_END:
            if (byte != APP_CMD_ETX) {
                LOG_WARN("Packet discarded (bad end byte)");
                state = CMD_STATE_WAIT_START;
                break;
            }

           

            uint8_t received_crc = buffer[expected_len]; // last byte of payload
            uint8_t computed_crc = app_crc8(&buffer[1], expected_len - 1); // [CMD + ARGS]

            if (received_crc != computed_crc) {
                LOG_WARN("Invalid CRC: expected 0x%02X, got 0x%02X", computed_crc, received_crc);
                state = CMD_STATE_WAIT_START;
                break;
            } else {
                LOG_INFO("Valid packet received: cmd=0x%02X, len=%u bytes", buffer[1], expected_len);
                uint8_t cmd = buffer[1];
                const uint8_t* args = &buffer[2];
                uint8_t args_len = expected_len - 2;

                
                switch (cmd) {
                    case APP_CMD_GET_VERSION:
                        //uart_send_string("Bootloader v1.0\r\n");
                        send_framed_response(RESP_TYPE_REPLY, "(App) FalkraDrones v1.0");       //todo add this to nvram
                        break;
                    case APP_CMD_JUMP_TO_APP:
                        //uart_send_string("Jumping to app\r\n");
                        send_framed_response(RESP_TYPE_REPLY, "Resetting Application");
                        // handle here. 
                            // fuse_clear();

                        	NVIC_SystemReset(); 
                        break;
                    case APP_CMD_JUMP_TO_BOOTLOADER:
                        //uart_send_string("Jumping to bootloader\r\n");
                        send_framed_response(RESP_TYPE_REPLY, "Jumping to bootloader");
                        // handle here. 
                            qspiFlash_set_fuse();  //todo integrate other backend drivers later
                        	NVIC_SystemReset();
                        break;

                    case APP_CMD_RESET:
                        LOG_INFO("[RESET] System reset requested");
                        send_framed_response(RESP_TYPE_REPLY, "RESET_OK");
                        HAL_Delay(100);  // Give UART time to send response
                        NVIC_SystemReset();
                        break;

                    case APP_CMD_TRANSPORT_SWITCH:
                        if (args_len < 1) {
                            send_framed_response(RESP_TYPE_ERROR, "NO_TRANSPORT");
                            break;
                        }

                        uint8_t transport_type = args[0];  // 1=UART, 2=WiFi
                        AppTransportStatus status;

                        if (transport_type == DATA_TRANSPORT_UART) {
                            LOG_INFO("[TRANSPORT_SWITCH] Switching to UART");
                            status = data_transport_select(DATA_TRANSPORT_UART);
                        } else if (transport_type == DATA_TRANSPORT_WIFI) {
                            LOG_INFO("[TRANSPORT_SWITCH] Switching to WiFi");
                            status = data_transport_select(DATA_TRANSPORT_WIFI);
                        } else {
                            send_framed_response(RESP_TYPE_ERROR, "INVALID_TRANSPORT");
                            break;
                        }

                        if (status == APP_TRANSPORT_STATUS_OK) {
                            const char* name = data_transport_get_name(transport_type);
                            LOG_INFO("[TRANSPORT_SWITCH] Switched to %s", name);
                            send_framed_response(RESP_TYPE_REPLY, "TRANSPORT_OK");
                        } else if (status == APP_TRANSPORT_STATUS_NOT_AVAILABLE) {
                            send_framed_response(RESP_TYPE_ERROR, "NOT_AVAILABLE");
                        } else {
                            send_framed_response(RESP_TYPE_ERROR, "SWITCH_FAIL");
                        }
                        break;
                    case APP_CMD_TRANSPORT_MODE_SWITCH:
                        if (args_len < 1) {
                            send_framed_response(RESP_TYPE_ERROR, "NO_MODE");
                            break;
                        }

                        uint8_t mode_type = args[0];  // 0=Console, 1=Command, 2=Data, etc.
                        if (mode_type > APP_TRANSPORT_MODE_FILESYSTEM_DATA) {
                            send_framed_response(RESP_TYPE_ERROR, "INVALID_MODE");
                            break;
                        }

                        data_transport_set_mode((AppTransportMode)mode_type);
                        LOG_INFO("[TRANSPORT_MODE_SWITCH] Switched to mode %u", mode_type);
                        send_framed_response(RESP_TYPE_REPLY, "MODE_SWITCH_OK");
                        break;   

                    case APP_CMD_WIFI_DISCONNECT:
                        LOG_INFO("[WIFI_DISCONNECT] WiFi disconnect command received");
                        send_framed_response(RESP_TYPE_REPLY, "WIFI_DISCONNECT_OK");
                        // Implement WiFi disconnect logic here
                        data_transport_select(DATA_TRANSPORT_UART);  // Switch back to UART after WiFi disconnect
                        data_transport_set_mode(APP_TRANSPORT_MODE_COMMAND);
                        break;


                    case APP_CMD_SERIAL_DISCONNECT:
                        LOG_INFO("[SERIAL_DISCONNECT] Serial disconnect command received"); 
                        send_framed_response(RESP_TYPE_REPLY, "SERIAL_DISCONNECT_OK");
                        data_transport_select(DATA_TRANSPORT_UART); 
                         data_transport_set_mode(APP_TRANSPORT_MODE_CONSOLE);
                        // Implement Serial disconnect logic here   
                        break;    
                    default:
                        uart_send_string("Unknown command\r\n");
                        break;
                }
            }

            state = CMD_STATE_WAIT_START;
            break;
    }
}
