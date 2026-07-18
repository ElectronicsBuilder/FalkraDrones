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
 * @file    nvram_wrapper.h
 * @brief   C-Compatible Wrapper for C++ NVRAM Driver
 * @details Provides C function pointers that bridge to the C++ NVRAM class,
 *          enabling pure C code to access NVRAM functionality.
 *
 * Architecture:
 * ```
 * C Code (user_config.c) → nvram_interface_t function pointers
 *                          ↓
 *                     nvram_wrapper.cpp (this file)
 *                          ↓
 *                     C++ NVRAM class → Hardware
 * ```
 *
 * Usage Example:
 * @code
 * // In C++ initialization code (main_cpp.cpp):
 * #include "nvram.hpp"
 * #include "nvram_wrapper.h"
 * #include "user_config.h"
 *
 * NVRAM nvram(&hspi1, NVRAM_CS_GPIO_Port, NVRAM_CS_Pin, ...);
 * nvram_interface_t* nvram_if = nvram_interface_create(&nvram);
 * userconfig_t* config = userconfig_create(nvram_if);
 *
 * // In pure C code (console.c, main_app.c):
 * extern userconfig_t* g_userConfig;
 * const char* ssid = userconfig_get_wifi_ssid(g_userConfig);
 * @endcode
 */

#ifndef NVRAM_WRAPPER_H
#define NVRAM_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "user_config.h"  // For nvram_interface_t definition

/**
 * @brief Create NVRAM interface from C++ NVRAM instance
 * @details This function is implemented in nvram_wrapper.cpp (C++).
 *          It creates function pointers that bridge to the C++ NVRAM class methods.
 * @param nvram_instance Pointer to C++ NVRAM class instance (void* for C compatibility)
 * @return Pointer to nvram_interface_t with function pointers set up,
 *         or NULL on failure
 *
 * Example (in C++ code):
 * @code
 * NVRAM nvram(&hspi1, ...);
 * nvram_interface_t* nvram_if = nvram_interface_create(&nvram);
 * @endcode
 */
nvram_interface_t* nvram_interface_create(void* nvram_instance);

/**
 * @brief Destroy NVRAM interface and free resources
 * @param nvram_if Pointer to nvram_interface_t created by nvram_interface_create()
 */
void nvram_interface_destroy(nvram_interface_t* nvram_if);

#ifdef __cplusplus
}
#endif

#endif // NVRAM_WRAPPER_H
