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
 * @file    data_transport.c
 * @brief   Data transport implementation
 */
#include "data_transport.h"
#include "app_defs.hpp"
#include "log.hpp"

// Include available transport drivers for the application
#include "transport_uart.h"
#include "transport_wifi.h"

// Registry of drivers (compile-time inclusion of available drivers)
static const AppTransportDriver* transport_drivers[] = {
    &uart_transport_driver,
    &wifi_transport_driver,
    NULL
};

// Active driver (default to UART)
static const AppTransportDriver* active_driver = &uart_transport_driver;

bool data_transport_init_all(void) {
    // Initialize default driver (UART)
    active_driver = &uart_transport_driver;
    if (active_driver->init) {
        active_driver->init();
        LOG_INFO("[Transport] Default UART transport initialized");
    }

    return true;
}

int data_transport_select(DataTransportType type) {
    for (int i = 0; transport_drivers[i] != NULL; i++) {
        if (transport_drivers[i]->type == type) {
            const AppTransportDriver* new_driver = transport_drivers[i];

            if (new_driver->is_available && !new_driver->is_available()) {
                LOG_WARN("[Transport] %s not available", new_driver->name);
                return -1;
            }

            // Flush and switch
            if (active_driver && active_driver->flush) {
                active_driver->flush();
            }

            active_driver = new_driver;
            if (active_driver->init) active_driver->init();
            LOG_INFO("[Transport] Switched to %s", active_driver->name);
            return 0;
        }
    }
    return -1;
}

DataTransportType data_transport_get_active(void) {
    return (active_driver != NULL) ? active_driver->type : DATA_TRANSPORT_UNKNOWN;
}

bool data_transport_is_available(DataTransportType type) {
    for (int i = 0; transport_drivers[i] != NULL; i++) {
        if (transport_drivers[i]->type == type) {
            const AppTransportDriver* driver = transport_drivers[i];
            if (driver->is_available) return driver->is_available();
            return true;
        }
    }
    return false;
}

const char* data_transport_get_name(DataTransportType type) {
    for (int i = 0; transport_drivers[i] != NULL; i++) {
        if (transport_drivers[i]->type == type) return transport_drivers[i]->name;
    }
    return "Unknown";
}

// Compatibility layer (legacy names)
bool data_transport_init(void) {
    return data_transport_init_all();
}

bool data_transport_poll(void) {
    return (active_driver && active_driver->poll) ? active_driver->poll() : false;
}

int data_transport_read(uint8_t *buf, size_t len) {
    return (active_driver && active_driver->read) ? active_driver->read(buf, len) : -1;
}

int data_transport_write(const uint8_t *buf, size_t len) {
    return (active_driver && active_driver->write) ? active_driver->write(buf, len) : -1;
}

void data_transport_flush(void) {
    if (active_driver && active_driver->flush) active_driver->flush();
}

void data_transport_set_mode(AppTransportMode mode) {
    if (active_driver && active_driver->set_mode) active_driver->set_mode(mode);
}
