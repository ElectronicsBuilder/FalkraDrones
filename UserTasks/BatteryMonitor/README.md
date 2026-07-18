# Battery Monitor FreeRTOS Task

## Overview

FreeRTOS task for continuous battery current monitoring using the ACS758-50A Hall Effect current sensor.

## Features

- **Task-based sampling** at 10 Hz (configurable)
- **Automatic calibration** on startup with controlled sampling
- **Thread-safe access** to current readings from other tasks
- **Smart logging** - only logs significant changes (>50mA)
- **Periodic statistics** - reports min/max/average every 10 seconds
- **Configurable parameters** via namespace constants

## Hardware

- **Sensor:** ACS758LCB-050B (±50A bidirectional)
- **ADC:** STM32F767 ADC1, 12-bit
  - Channel 0 (PA0): VOUT1 (raw output)
  - Channel 1 (PA1): VOUT2 (buffered output)
- **DMA:** DMA2 Stream 4, HALFWORD alignment

## Task Configuration

Located in `BatteryMonitorConfig` namespace:

```cpp
constexpr uint32_t CALIBRATION_DELAY_MS = 3000;     // Wait before calibration
constexpr uint16_t CALIBRATION_SAMPLES = 100;       // Samples for calibration
constexpr uint32_t CALIBRATION_SAMPLE_INTERVAL = 10;// ms between cal samples
constexpr uint32_t SAMPLING_INTERVAL_MS = 100;      // 10 Hz sampling rate
constexpr float LOG_THRESHOLD_A = 0.05f;            // Log if change > 50mA
constexpr float IDLE_THRESHOLD_A = 0.5f;            // Current below this = idle
constexpr float VREF = 3.29f;                       // ADC reference voltage
```

## Usage

### 1. Create Task in main_cpp.cpp

```cpp
#include "BatteryMonitorTask.hpp"

// Task attributes
const osThreadAttr_t batteryMonitor_attributes = {
    .name = "batteryMonitor",
    .stack_size = 512 * 4,  // 2KB stack
    .priority = (osPriority_t) osPriorityNormal,
};

// In task creation section:
osThreadNew(batteryMonitorTask, NULL, &batteryMonitor_attributes);
```

### 2. Access Current from Other Tasks

```cpp
#include "BatteryMonitorTask.hpp"

void someOtherTask(void *arg) {
    while (1) {
        // Get latest current reading (thread-safe)
        float current = getBatteryCurrent();

        if (current > 5.0f) {
            LOG_WARN("High charging current: %.2fA", current);
        }

        osDelay(1000);
    }
}
```

### 3. Advanced Access

```cpp
#include "BatteryMonitorTask.hpp"

void advancedTask(void *arg) {
    // Get monitor instance for detailed access
    BatteryMonitor* monitor = getBatteryMonitor();

    if (monitor != nullptr) {
        BatteryMonitorData data = monitor->getData();
        BatteryMonitorStats stats = monitor->getStats();

        LOG_INFO("Raw ADC: %u, Buffered ADC: %u", data.adc_raw, data.adc_buffered);
        LOG_INFO("Min/Max: %.3fA / %.3fA", stats.current_min, stats.current_max);
    }
}
```

## Calibration

The task automatically performs zero-current calibration on startup:

1. Waits 3 seconds after initialization
2. Logs warning to disconnect all loads
3. Takes 100 samples over 1 second (10ms intervals)
4. Calculates average zero offset
5. Sets calibration values

**Important:** Ensure zero current flow during calibration!

## Expected Output

### Startup Sequence

```
[BATMON_TASK] Starting Battery Monitor Task
[BATMON] Initializing Battery Monitor (ACS758-50A)
[BATMON] - VREF: 3.29V
[BATMON_TASK] Waiting for system stabilization...
[BATMON_TASK] ========================================
[BATMON_TASK] CALIBRATION STARTING
[BATMON_TASK] ========================================
[BATMON_TASK] Disconnect all loads from battery!
[BATMON_TASK] Ensure ZERO current flow.
[BATMON_TASK] Calibration starts in 3 seconds...
[BATMON_TASK] Calibrating with 100 samples...
[BATMON_TASK] ========================================
[BATMON_TASK] Calibration Complete
[BATMON_TASK] ========================================
[BATMON_TASK] Samples collected: 100/100
[BATMON_TASK] Zero offset (Raw):      1.6695V (ADC: 2081)
[BATMON_TASK] Zero offset (Buffered): 1.6687V (ADC: 2080)
[BATMON_TASK] VREF: 3.2900V
[BATMON_TASK] ========================================
[BATMON_TASK] Starting continuous monitoring at 10 Hz
```

### Normal Operation

```
[BATTERY] Current: +0.447A [CHARGING]
[BATTERY] Current: +0.523A [CHARGING]
[BATTERY] Current: +0.012A [IDLE]
[BATTERY] Current: -1.234A [DISCHARGING]
[BATMON_TASK] Status: Samples=1000, Min=-1.250A, Max=0.530A, Avg=0.120A
```

## API Reference

### batteryMonitorTask()
```cpp
void batteryMonitorTask(void *argument);
```
Main FreeRTOS task - creates monitor, calibrates, and continuously samples.

### getBatteryCurrent()
```cpp
float getBatteryCurrent(void);
```
**Returns:** Current battery current in Amperes
- Positive = charging
- Negative = discharging
- Thread-safe, can be called from any task

### getBatteryMonitor()
```cpp
BatteryMonitor* getBatteryMonitor(void);
```
**Returns:** Pointer to BatteryMonitor instance or nullptr if not initialized
- For advanced access to raw ADC values, statistics, etc.
- Do not delete this pointer!

## Troubleshooting

### No Calibration Log
**Cause:** Task not created or initialization failed
**Fix:** Check task creation in main_cpp.cpp

### Calibration Shows Large Offset
**Cause:** Current flowing during calibration
**Fix:** Disconnect all loads before calibration starts

### Current Readings Always Zero
**Cause:** DMA configuration issue (WORD vs HALFWORD)
**Fix:** Check stm32f7xx_hal_msp.c - DMA must be HALFWORD

### Wild Current Fluctuations
**Cause:** ADC continuous mode enabled
**Fix:** Disable continuous mode in CubeMX (should already be disabled for this task)

## Files

- `BatteryMonitorTask.hpp` - Header with API declarations
- `BatteryMonitorTask.cpp` - Implementation
- `README.md` - This file

## Dependencies

- `BatteryMonitor.hpp/.cpp` - Driver
- `log.hpp` - Logging functions
- `cmsis_os2.h` - FreeRTOS CMSIS-RTOS v2 API
- `hadc1` - External ADC handle from main.c

## License

MIT License - See file headers for full license text.

Copyright (c) 2025 ElectronicsBuilder
