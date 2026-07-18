#include "spi1_bus_lock.hpp"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#ifndef SPI1_BUS_LOCK_ENABLED
#define SPI1_BUS_LOCK_ENABLED 1
#endif

static SemaphoreHandle_t spi1_bus_mutex = nullptr;
static StaticSemaphore_t spi1_bus_mutex_storage;

void spi1_bus_lock_init() {
#if SPI1_BUS_LOCK_ENABLED
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }

    if (spi1_bus_mutex == nullptr) {
        taskENTER_CRITICAL();
        if (spi1_bus_mutex == nullptr) {
            spi1_bus_mutex = xSemaphoreCreateRecursiveMutexStatic(&spi1_bus_mutex_storage);
        }
        taskEXIT_CRITICAL();
    }
#endif
}

Spi1BusGuard::Spi1BusGuard() {
#if SPI1_BUS_LOCK_ENABLED
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }

    spi1_bus_lock_init();
    if (spi1_bus_mutex == nullptr) {
        return;
    }

    locked_ = (xSemaphoreTakeRecursive(spi1_bus_mutex, portMAX_DELAY) == pdTRUE);
#endif
}

Spi1BusGuard::~Spi1BusGuard() {
    if (locked_) {
        (void)xSemaphoreGiveRecursive(spi1_bus_mutex);
    }
}
