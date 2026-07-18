/**
 * MIT License
 *
 * Copyright (c) 2025 ElectronicsBuilder
 *
 * @file    gpio_interrupts.c
 * @brief   Centralized GPIO EXTI Interrupt Handler Implementation
 */

#include "gpio_interrupts.h"
#include <string.h>

#define MAX_GPIO_CALLBACKS 16

typedef struct {
    uint16_t gpio_pin;
    GPIO_InterruptCallback callback;
} GPIO_CallbackEntry;

static GPIO_CallbackEntry g_callbacks[MAX_GPIO_CALLBACKS];
static uint8_t g_callback_count = 0;
static uint8_t g_initialized = 0;

void GPIO_Interrupts_Init(void) {
    if (g_initialized) {
        return;
    }

    memset(g_callbacks, 0, sizeof(g_callbacks));
    g_callback_count = 0;
    g_initialized = 1;
}

int GPIO_Interrupts_RegisterCallback(uint16_t gpio_pin, GPIO_InterruptCallback callback) {
    if (!g_initialized) {
        GPIO_Interrupts_Init();
    }

    if (callback == NULL) {
        return -1;
    }

    // Check if already registered for this pin
    for (uint8_t i = 0; i < g_callback_count; i++) {
        if (g_callbacks[i].gpio_pin == gpio_pin && g_callbacks[i].callback == callback) {
            return 0;  // Already registered
        }
    }

    // Find empty slot
    if (g_callback_count >= MAX_GPIO_CALLBACKS) {
        return -1;  // No slots available
    }

    g_callbacks[g_callback_count].gpio_pin = gpio_pin;
    g_callbacks[g_callback_count].callback = callback;
    g_callback_count++;

    return 0;
}

void GPIO_Interrupts_UnregisterCallback(uint16_t gpio_pin) {
    for (uint8_t i = 0; i < g_callback_count; i++) {
        if (g_callbacks[i].gpio_pin == gpio_pin) {
            // Shift remaining entries
            for (uint8_t j = i; j < g_callback_count - 1; j++) {
                g_callbacks[j] = g_callbacks[j + 1];
            }
            g_callback_count--;
            i--;  // Re-check this index
        }
    }
}

void GPIO_Interrupts_HandleCallback(uint16_t gpio_pin) {
    for (uint8_t i = 0; i < g_callback_count; i++) {
        if (g_callbacks[i].gpio_pin == gpio_pin && g_callbacks[i].callback != NULL) {
            g_callbacks[i].callback(gpio_pin);
        }
    }
}
