/**
 * MIT License
 *
 * Copyright (c) 2025 ElectronicsBuilder
 *
 * @file    gpio_interrupts.h
 * @brief   Centralized GPIO EXTI Interrupt Handler
 * @details Routes HAL_GPIO_EXTI_Callback to registered sensor callbacks.
 *          Each sensor driver registers its callback for specific GPIO pins.
 *
 * Usage:
 * 1. Call GPIO_Interrupts_Init() during system initialization
 * 2. Each sensor driver calls GPIO_Interrupts_RegisterCallback() for its pins
 * 3. HAL_GPIO_EXTI_Callback() calls GPIO_Interrupts_HandleCallback()
 */

#ifndef GPIO_INTERRUPTS_H
#define GPIO_INTERRUPTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Callback function type for GPIO interrupt handlers
 * @param gpio_pin The GPIO pin that triggered the interrupt
 */
typedef void (*GPIO_InterruptCallback)(uint16_t gpio_pin);

/**
 * @brief Initialize the GPIO interrupt system
 */
void GPIO_Interrupts_Init(void);

/**
 * @brief Register a callback for a specific GPIO pin
 * @param gpio_pin The GPIO pin mask (e.g., GPIO_PIN_2)
 * @param callback Function to call when this pin triggers
 * @return 0 on success, -1 if no slots available
 */
int GPIO_Interrupts_RegisterCallback(uint16_t gpio_pin, GPIO_InterruptCallback callback);

/**
 * @brief Unregister a callback for a specific GPIO pin
 * @param gpio_pin The GPIO pin mask
 */
void GPIO_Interrupts_UnregisterCallback(uint16_t gpio_pin);

/**
 * @brief Handle GPIO EXTI callback - call from HAL_GPIO_EXTI_Callback
 * @param gpio_pin The GPIO pin that triggered
 *
 * This function dispatches to all registered callbacks for the given pin.
 */
void GPIO_Interrupts_HandleCallback(uint16_t gpio_pin);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_INTERRUPTS_H */
