#include "i2c2_bus_lock.hpp"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#ifndef I2C2_BUS_LOCK_ENABLED
#define I2C2_BUS_LOCK_ENABLED 1
#endif

static SemaphoreHandle_t i2c2_bus_mutex = nullptr;
static StaticSemaphore_t i2c2_bus_mutex_storage;

void i2c2_bus_lock_init() {
#if I2C2_BUS_LOCK_ENABLED
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }

    if (i2c2_bus_mutex == nullptr) {
        taskENTER_CRITICAL();
        if (i2c2_bus_mutex == nullptr) {
            i2c2_bus_mutex = xSemaphoreCreateRecursiveMutexStatic(&i2c2_bus_mutex_storage);
        }
        taskEXIT_CRITICAL();
    }
#endif
}

I2c2BusGuard::I2c2BusGuard() {
#if I2C2_BUS_LOCK_ENABLED
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }

    i2c2_bus_lock_init();
    if (i2c2_bus_mutex == nullptr) {
        return;
    }

    locked_ = (xSemaphoreTakeRecursive(i2c2_bus_mutex, portMAX_DELAY) == pdTRUE);
#endif
}

I2c2BusGuard::~I2c2BusGuard() {
    if (locked_) {
        (void)xSemaphoreGiveRecursive(i2c2_bus_mutex);
    }
}
