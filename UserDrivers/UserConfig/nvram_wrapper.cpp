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
 * @file    nvram_wrapper.cpp
 * @brief   C-Compatible Wrapper Implementation for C++ NVRAM Driver
 */

#include "nvram_wrapper.h"
#include "nvram.hpp"
#include <stdlib.h>

// ============================================================================
// Internal Structure to Hold NVRAM Instance and Function Pointers
// ============================================================================

typedef struct {
    nvram_interface_t interface;  // C function pointer interface
    NVRAM* nvram_instance;        // C++ NVRAM class instance
} nvram_wrapper_context_t;

// ============================================================================
// C-Callable Static Wrapper Functions
// ============================================================================

/**
 * @brief C-callable wrapper for NVRAM::writeArray()
 * @param address NVRAM address
 * @param data Data buffer to write
 * @param size Number of bytes to write
 */
static void nvram_write_array_wrapper(uint32_t address, uint8_t* data, uint16_t size) {
    // Context is stored in a thread-local or global variable
    // For simplicity, we'll use a different approach: store context pointer in the function pointer struct
    // This is accomplished by using the nvram_wrapper_context_t structure
}



// ============================================================================
// Trampoline Functions with Context
// ============================================================================

// Forward declaration of context retrieval
static nvram_wrapper_context_t* get_context_from_interface(nvram_interface_t* iface);

/**
 * @brief Write array wrapper (trampoline)
 */
extern "C" void nvram_write_array_trampoline(uint32_t address, uint8_t* data, uint16_t size);
extern "C" void nvram_read_array_trampoline(uint32_t address, uint8_t* buffer, uint16_t size);
extern "C" uint8_t nvram_calculate_crc8_trampoline(uint8_t* buffer, uint32_t length);

// Global context storage (thread-safe alternative: use TLS or pass via first parameter)
static nvram_wrapper_context_t* g_active_context = nullptr;

void nvram_write_array_trampoline(uint32_t address, uint8_t* data, uint16_t size) {
    if (g_active_context && g_active_context->nvram_instance) {
        g_active_context->nvram_instance->writeArray(address, data, size);
    }
}
 
void nvram_read_array_trampoline(uint32_t address, uint8_t* buffer, uint16_t size) {
    if (g_active_context && g_active_context->nvram_instance) {
        g_active_context->nvram_instance->readArray(address, buffer, size);
    }
}

uint8_t nvram_calculate_crc8_trampoline(uint8_t* buffer, uint32_t length) {
    if (g_active_context && g_active_context->nvram_instance) {
        return g_active_context->nvram_instance->CalculateCRC8(buffer, (long)length);
    }
    return 0;
}

// ============================================================================
// Public API Implementation
// ============================================================================

extern "C" nvram_interface_t* nvram_interface_create(void* nvram_instance) {
    if (!nvram_instance) {
        return nullptr;
    }

    // Allocate wrapper context
    nvram_wrapper_context_t* context = (nvram_wrapper_context_t*)malloc(sizeof(nvram_wrapper_context_t));
    if (!context) {
        return nullptr;
    }

    // Store C++ NVRAM instance
    context->nvram_instance = static_cast<NVRAM*>(nvram_instance);

    // Set up function pointers
    context->interface.write_array = nvram_write_array_trampoline;
    context->interface.read_array = nvram_read_array_trampoline;
    context->interface.calculate_crc8 = nvram_calculate_crc8_trampoline;

    // Store context globally (NOTE: This limits to single instance; for multi-instance support,
    // consider passing context as first parameter or using thread-local storage)
    g_active_context = context;

    // Return pointer to the interface part
    return &context->interface;
}

extern "C" void nvram_interface_destroy(nvram_interface_t* nvram_if) {
    if (!nvram_if) {
        return;
    }

    // Calculate offset to get back to wrapper context
    // The interface is the first member of nvram_wrapper_context_t, so we can cast directly
    nvram_wrapper_context_t* context = (nvram_wrapper_context_t*)((char*)nvram_if - offsetof(nvram_wrapper_context_t, interface));

    // Clear global context if it matches
    if (g_active_context == context) {
        g_active_context = nullptr;
    }

    // Free context
    free(context);
}
