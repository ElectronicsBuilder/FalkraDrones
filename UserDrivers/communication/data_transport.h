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
 * @file    data_transport.h
 * @brief   Data transport runtime-selection interface
 */
#ifndef __DATA_TRANSPORT_H
#define __DATA_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Transport backend types
typedef enum {
    DATA_TRANSPORT_UNKNOWN = 0,
    DATA_TRANSPORT_UART,
    DATA_TRANSPORT_WIFI
} DataTransportType;

// Transport status codes
typedef enum {
    APP_TRANSPORT_STATUS_OK = 0,
    APP_TRANSPORT_STATUS_NOT_INITIALIZED,
    APP_TRANSPORT_STATUS_NOT_AVAILABLE,
    APP_TRANSPORT_STATUS_ERROR
} AppTransportStatus;

// Transport mode for data reception
typedef enum {
    APP_TRANSPORT_MODE_CONSOLE = 0,
    APP_TRANSPORT_MODE_COMMAND,    // Receiving commands
    APP_TRANSPORT_MODE_DATA,         // Receiving Binary chunks
    APP_TRANSPORT_MODE_EXTMEM,          // Receiving external memory chunks
    APP_TRANSPORT_MODE_FILESYSTEM_COMMAND, // Filesystem command mode
    APP_TRANSPORT_MODE_FILESYSTEM_DATA // Filesystem data mode
} AppTransportMode;


// Extended transport driver structure with runtime selection support
typedef struct {
    DataTransportType type;
    const char* name;
    bool (*is_available)(void);
    void (*init)(void);
    bool (*poll)(void);
    int  (*read)(uint8_t *buf, size_t len);
    int  (*write)(const uint8_t *buf, size_t len);
    void (*flush)(void);
    void (*set_mode)(AppTransportMode mode);
} AppTransportDriver;

// Runtime selection API
bool data_transport_init_all(void);
int data_transport_select(DataTransportType type);
DataTransportType data_transport_get_active(void);
bool data_transport_is_available(DataTransportType type);
const char* data_transport_get_name(DataTransportType type);

// Legacy API compatibility
bool data_transport_init(void);
bool data_transport_poll(void);
int  data_transport_read(uint8_t *buf, size_t len);
int  data_transport_write(const uint8_t *buf, size_t len);
void data_transport_flush(void);
void data_transport_set_mode(AppTransportMode mode);

#ifdef __cplusplus
}
#endif

#endif /* __DATA_TRANSPORT_H */
