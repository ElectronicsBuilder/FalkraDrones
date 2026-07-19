
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
 * @file    main_cpp.cpp
 * @brief   Main C++ entry point
 */
#include "main_cpp_entry.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f767xx.h"
#include "main.h"

#include "log.hpp"
#include "uart.hpp"
#include "data_transport.h"
#include "qspi_flash.hpp"
#include "test_uart.hpp"
#include "display_driver.hpp"
#include "app_touchgfx.h"

#include "test_nvram.hpp"
#include "test_spi_flash.hpp"
#include "test_peripherals.hpp"

#include "status.hpp"

#include "test_max98357.hpp"
#include "main_app.h"
#include "wifi_tcpClient.h"
#include "wifi_tcpServer.h"
#include "tcp_server.h"
#include "queue.h"

#include "nvram.hpp"
#include "nvram_wrapper.h"
#include "user_config.h"
#include "BatteryMonitor.hpp"

#include "BatteryMonitorTask.hpp"
#include "RadioReceiverTask.hpp"
#include "ToFTask.hpp"
#include "tof_speed_opts.h"
#include "MotorControlTask.hpp"
// DriverManager for centralized driver initialization
#include "driver_manager.hpp"
#include "driver_status.hpp"
#include "audio_manager.hpp"


#include <string.h>

// ============================================================================
// DIAGNOSTIC SWITCH (see .claude/wifi_issue.md): 0 = boot without TouchGFX /
// QSPI memory-mapped mode to test whether the WiFi NCP link survives.
// Task gating still works (TouchGFX_init is still set). Restore to 1 after test.
#define ENABLE_TOUCHGFX 1
// ============================================================================

void heartbeatTask(void *argument);
void UserDriverTask(void *argument);
void UART_Task(void *argument);
void GUI_task(void *argument);
void DISPLAY_task(void *argument);
void wifiTask(void *argument);
void tcpClientTask(void *argument);
void tcpServerTask(void *argument);


static void waitLogInit(void);
static void waitWifiInit(void);


extern volatile bool _intFlag;
extern "C" {
    #include "sh2.h"
    void touchgfx_signalVSyncTimer(void);  // C-linkage for C code
    }

extern bool log_initialized;
extern bool wifiDriverInit;

extern bool PeripheralsTestComplete;
extern bool  qspi_dma_tx_done;
extern QSPI_HandleTypeDef hqspi;
extern ADC_HandleTypeDef hadc1;
// TCP Client Queue for user messages
QueueHandle_t tcpClientQueue;
#define TCP_MSG_MAX_LEN 256


osThreadId_t UserDriverTask_TaskHandle;
const osThreadAttr_t UserDriverTask_attributes = {
	.name = "UserDriver TASK",
	.attr_bits = 0,
	.cb_mem = NULL,
	.cb_size = 0,
	.stack_mem = NULL,
	.stack_size = 4096 * 2,
	.priority = (osPriority_t)osPriorityNormal,
	.tz_module = 0,
	.reserved = 0
};

osThreadId_t heartbeatTask_TaskHandle;
const osThreadAttr_t heartbeatTask_attributes = {
	.name = "heartbeat TASK",
	.attr_bits = 0,
	.cb_mem = NULL,
	.cb_size = 0,
	.stack_mem = NULL,
	.stack_size = 1024,
	.priority = (osPriority_t)osPriorityNormal,
	.tz_module = 0,
	.reserved = 0
};




osThreadId_t UARTTask_TaskHandle;
const osThreadAttr_t UARTTask_attributes = {
	.name = "UART TASK",
	.attr_bits = 0,
	.cb_mem = NULL,
	.cb_size = 0,
	.stack_mem = NULL,
	.stack_size = 4096 * 2,
	.priority = (osPriority_t)osPriorityNormal,
	.tz_module = 0,
	.reserved = 0
};

osThreadId_t test_peripheralsTask_TaskHandle;
const osThreadAttr_t test_peripheralsTask_attributes = {
	.name = "Test Peripherals TASK",
	.attr_bits = 0,
	.cb_mem = NULL,
	.cb_size = 0,
	.stack_mem = NULL,
	.stack_size = 4096 * 1,
	.priority = (osPriority_t)osPriorityNormal,
	.tz_module = 0,
	.reserved = 0
};

osThreadId_t GUI_TaskHandle;
const osThreadAttr_t GUITask_attributes = {
	.name = "GUI TASK",
	.attr_bits = 0,
	.cb_mem = NULL,
	.cb_size = 0,
	.stack_mem = NULL,
	.stack_size = 2048 * 1,
	.priority = (osPriority_t)osPriorityNormal,
	.tz_module = 0,
	.reserved = 0
};

osThreadId_t DISPLAY_TaskHandle;
const osThreadAttr_t DISPLAYTask_attributes = {
	.name = "DISPLAY TASK",
	.attr_bits = 0,
	.cb_mem = NULL,
	.cb_size = 0,
	.stack_mem = NULL,
	.stack_size = 2048 * 1,
	.priority = (osPriority_t)osPriorityNormal,
	.tz_module = 0,
	.reserved = 0
};

osThreadId_t statusTaskHandle;
const osThreadAttr_t statusTask_attributes = {
    .name = "STATUS TASK",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0,
    .stack_mem = NULL,
    .stack_size = 4096,  // was 1024: overflowed (float printf + BMP581 lib + fs logging)
    .priority = (osPriority_t)osPriorityNormal,
    .tz_module = 0,
    .reserved = 0
};

osThreadId_t wifiTaskHandle;
const osThreadAttr_t wifiTask_attributes = {
    .name = "WIFI TASK",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0,
    .stack_mem = NULL,
    .stack_size = 4096 * 2,
    .priority = (osPriority_t)osPriorityNormal,
    .tz_module = 0,
    .reserved = 0
};

osThreadId_t tcpClientTaskHandle;
const osThreadAttr_t tcpClientTask_attributes = {
    .name = "TCP CLIENT TASK",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0,
    .stack_mem = NULL,
    .stack_size = 4096 * 1,
    .priority = (osPriority_t)osPriorityNormal,
    .tz_module = 0,
    .reserved = 0
};

osThreadId_t tcpServerTaskHandle;
const osThreadAttr_t tcpServerTask_attributes = {
    .name = "TCP SERVER TASK",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0,
    .stack_mem = NULL,
    .stack_size = 4096 * 2,
    .priority = (osPriority_t)osPriorityNormal,
    .tz_module = 0,
    .reserved = 0
};


osThreadId_t batteryMonitorTaskHandle;
const osThreadAttr_t batteryMonitorTask_attributes = {
    .name = "BATTERY MONITOR TASK",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0,
    .stack_mem = NULL,
    .stack_size = 2048 * 1,
    .priority = (osPriority_t)osPriorityNormal,
    .tz_module = 0,
    .reserved = 0
};


osThreadId_t RadioReceiverTaskHandle;
const osThreadAttr_t RadioReceiverTask_attributes = {
    .name = "RADIO RECEIVER TASK",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0,
    .stack_mem = NULL,
    .stack_size = 2048 * 1,
    .priority = (osPriority_t)osPriorityNormal,
    .tz_module = 0,
    .reserved = 0
};

osThreadId_t ToFTaskHandle;
const osThreadAttr_t ToFTask_attributes = {
    .name = "TOF TASK",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0,
    .stack_mem = NULL,
    .stack_size = 2048 * 2,
#if TOF_OPT_TASK_PRIORITY
#if TOF_OPT_I2C_ASYNC_MODE
    .priority = (osPriority_t)osPriorityHigh,
#else
    .priority = (osPriority_t)osPriorityAboveNormal,
#endif
#else
    .priority = (osPriority_t)osPriorityNormal,
#endif
    .tz_module = 0,
    .reserved = 0
};

osThreadId_t ToFDistanceTaskHandle;
const osThreadAttr_t ToFDistanceTask_attributes = {
    .name = "TOF DIST TASK",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0,
    .stack_mem = NULL,
#if TOF_OPT_TASK_PRIORITY
    .stack_size = 2048,
#if TOF_OPT_I2C_ASYNC_MODE
    .priority = (osPriority_t)osPriorityAboveNormal,
#else
    .priority = (osPriority_t)osPriorityNormal,
#endif
#else
    .stack_size = 1024 * 1,
    .priority = (osPriority_t)osPriorityNormal,
#endif
    .tz_module = 0,
    .reserved = 0
};

osThreadId_t MotorControlTaskHandle;
const osThreadAttr_t MotorControlTask_attributes = {
    .name = "MOTOR CONTROL",
    .attr_bits = 0,
    .cb_mem = NULL,
    .cb_size = 0,
    .stack_mem = NULL,
    .stack_size = 2048 * 1,  // Increased from 1024
    .priority = (osPriority_t)osPriorityAboveNormal,
    .tz_module = 0,
    .reserved = 0
};







bool TouchGFX_init = false;
 int tickCounter;
 int digitalHours;
 int digitalMinutes;
 int digitalSeconds;
 uint16_t digitalDays;

uint32_t g_tickCounter;
int g_digitalSeconds;

// Global instances now managed by DriverManager in Phase 2+
// QspiFlash and NVRAM/SpiFlash are created by DriverManager::initializeCore()

// Global UserConfig instance (pure C implementation)
// Will be set by DriverManager::initializeCore()
userconfig_t* g_userConfig = nullptr;

// Global BatteryMonitor instance
// Will be set by DriverManager::initializeCore()
BatteryMonitor* g_batteryMonitor = nullptr;






// FreeRTOS stack overflow hook (configCHECK_FOR_STACK_OVERFLOW=2).
// Names the offending task on the console, then halts like configASSERT.
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    LOG_ERROR("[FAULT] Stack overflow in task: %s", pcTaskName ? pcTaskName : "?");
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

// extern "C" wrapper for main_cpp
void main_cpp(void)
{


    LOG_INFO("🛸🎮 Falkra Controller Application Started!");

    HAL_Delay(100);

    // Initialize drivers via centralized DriverManager
    auto& driver_manager = DriverManager::getInstance();

    LOG_INFO("[MAIN] Initializing critical drivers (NVRAM -> UserConfig -> Display -> BatteryMonitor)...");
    if (!driver_manager.initializeCore()) {
        LOG_ERROR("[MAIN] Critical driver initialization failed!");
        Error_Handler();
    }

    // Maintain backward compatibility with global pointers
    g_userConfig = driver_manager.getUserConfig();
    g_batteryMonitor = driver_manager.getBatteryMonitor();

    LOG_INFO("[MAIN] Core drivers initialized successfully");

    // Initialize all remaining drivers (audio, sensors, peripherals, etc.)
    LOG_INFO("[MAIN] Initializing all drivers...");
    if (!driver_manager.initializeAll()) {
        LOG_WARN("[MAIN] Some non-critical drivers failed to initialize (continuing)");
    }

    // Initialize DriverStatus for thread-safe sensor polling
    if (!DriverStatus::init()) {
        LOG_ERROR("[MAIN] DriverStatus initialization failed!");
        Error_Handler();
    }

    // Initialize ToF distance reading task
    // if (!tof_distance_init()) {
    //     LOG_WARN("[MAIN] ToF distance task initialization failed (continuing)");
    // }

    // Log UserConfig WiFi credentials if loaded
    if (g_userConfig && userconfig_has_wifi_credentials(g_userConfig)) {
        LOG_INFO("[UserConfig] Loaded WiFi credentials from NVRAM: SSID=%s",
                 userconfig_get_wifi_ssid(g_userConfig));
    }

    // NOTE: Startup tone is deferred to UserDriverTask after kernel starts
    // because HAL_Delay() in MAX98357::enable() requires FreeRTOS scheduler to be running

    osKernelInitialize();

    //// Create TCP client message queue
   //// tcpClientQueue = xQueueCreate(10, TCP_MSG_MAX_LEN);

    UserDriverTask_TaskHandle           = osThreadNew(UserDriverTask, NULL, &UserDriverTask_attributes);
    heartbeatTask_TaskHandle            = osThreadNew(heartbeatTask, NULL, &heartbeatTask_attributes);
    UARTTask_TaskHandle                 = osThreadNew(UART_Task, NULL, &UARTTask_attributes);
    test_peripheralsTask_TaskHandle     = osThreadNew(test_peripheralsTask, NULL, &test_peripheralsTask_attributes);

    GUI_TaskHandle                      = osThreadNew(GUI_task, NULL, &GUITask_attributes);
    DISPLAY_TaskHandle                  = osThreadNew(DISPLAY_task, NULL, &DISPLAYTask_attributes);
    statusTaskHandle                    = osThreadNew(status_task, NULL, &statusTask_attributes);
    ////wifiTaskHandle                      = osThreadNew(wifiTask, NULL, &wifiTask_attributes);
    ////tcpClientTaskHandle                 = osThreadNew(tcpClientTask, NULL, &tcpClientTask_attributes);
    batteryMonitorTaskHandle            = osThreadNew(batteryMonitorTask, NULL, &batteryMonitorTask_attributes);
   // RadioReceiverTaskHandle             = osThreadNew(RadioReceiverTask, NULL, &RadioReceiverTask_attributes);
    ToFTaskHandle                       = osThreadNew(tof_task, NULL, &ToFTask_attributes);
    ToFDistanceTaskHandle               = osThreadNew(tof_detection_task, NULL, &ToFDistanceTask_attributes);
  //  MotorControlTaskHandle              = osThreadNew(MotorControlTask, NULL, &MotorControlTask_attributes);
   // tcpServerTaskHandle                 = osThreadNew(tcpServerTask, NULL, &tcpServerTask_attributes);

    if (!heartbeatTask_TaskHandle) {
        LOG_ERROR("[MAIN] Failed to create heartbeat task");
    }
    if (!UARTTask_TaskHandle) {
        LOG_ERROR("[MAIN] Failed to create UART task");
    }
    if (!GUI_TaskHandle) {
        LOG_ERROR("[MAIN] Failed to create GUI task");
    }
    if (!DISPLAY_TaskHandle) {
        LOG_ERROR("[MAIN] Failed to create display task");
    }
    if (!statusTaskHandle) {
        LOG_ERROR("[MAIN] Failed to create status task");
    }
    if (!ToFTaskHandle || !ToFDistanceTaskHandle) {
        LOG_ERROR("[MAIN] Failed to create ToF task(s)");
    }

    
    LOG_INFO("About to start FreeRTOS kernel...");
       
    osKernelStart();
    LOG_ERROR("osKernelStart() returned unexpectedly!\r\n");

    while (1)
    {
    }
}


void UserDriverTask(void *argument)
{
     (void)argument; // Mark argument as unused

    log_init();

    // Play startup tone after kernel has started and log is ready
    osDelay(100);  // Give other tasks time to initialize
    LOG_INFO("[UserDriverTask] Playing startup tone via AudioManager...");
    auto& dm = DriverManager::getInstance();
    AudioManager* audio_manager = dm.getAudioManager();
    if (audio_manager && audio_manager->isInitialized()) {
        audio_manager->playStartupTone();
        LOG_INFO("[UserDriverTask] Startup tone complete");
    } else {
        LOG_WARN("[UserDriverTask] AudioManager not available - skipping startup tone");
    }

    while(1)
    {
        //Todo:: Handle runtime issue with drivers here
        osDelay(1000);
    }
}


void heartbeatTask(void *argument)
{   
    (void)argument; // Mark argument as unused
    for (;;) {
  
        osDelay(500);  // Run every ~500 ms
        HAL_GPIO_TogglePin(LED_HB_GPIO_Port, LED_HB_Pin);  
    
    }
}
 

void wifiTask(void *argument)
{
     (void)argument; // Mark argument as unused
    while (!TouchGFX_init || !PeripheralsTestComplete) {
    osDelay(10);
}
    waitLogInit();
    main_app();

    while(1)
    {
        osDelay(10);
    }
}

void UART_Task(void *argument)
{
    (void)argument; // Mark argument as unused
   osDelay(2000);
  // uart_init_rx_dma(); 
   waitLogInit();
   data_transport_init();


    for (;;) {
  
        osDelay(1);  // Run every ~1 ms
       // uart_dma_poll(); 
        data_transport_poll();
    }
}



void DISPLAY_task(void *argument)
{
    (void)argument; // Mark argument as unused
    uint32_t tick = 0;

    waitLogInit();

    while (!TouchGFX_init || !PeripheralsTestComplete) {
        osDelay(10);
    }
   osDelay(1000);
    // Get ST7789 display instance from DriverManager (already initialized in initializeCore)
    auto& dm = DriverManager::getInstance();
    ST7789* display = dm.getDisplay();

    if (!display) {
        LOG_ERROR("[DISPLAY_TASK] Display driver not available via DriverManager");
        return;
    }

    LOG_INFO("[ST7789] Display Driver ready for TouchGFX");

    for (;;) {
        tick++;
        if (tick >= 16) {
#if ENABLE_TOUCHGFX
            touchgfx_signalVSyncTimer();
#endif
            tick = 0;
        }
        osDelay(1);
    }
}

void GUI_task(void *argument)
{
    (void)argument; // Mark argument as unused

    while (!PeripheralsTestComplete) {
        osDelay(10);
    }

    // Enable Quad Memory Mapped Mode for QSPI Flash (used by TouchGFX assets)
    LOG_INFO("[HEAP] free=%u min_ever=%u (before TouchGFX/QSPI-mmap start)",
             (unsigned int)xPortGetFreeHeapSize(),
             (unsigned int)xPortGetMinimumEverFreeHeapSize());
#if ENABLE_TOUCHGFX
    auto* qspi_flash = DriverManager::getInstance().getQspiFlash();
    if (qspi_flash) {
        qspi_flash->enableQuadMemoryMappedMode();
    }

        MX_TouchGFX_Init();
	    /* Call PreOsInit function */
		MX_TouchGFX_PreOSInit();
        TouchGFX_init = true;
        LOG_INFO("[HEAP] free=%u min_ever=%u (after TouchGFX init)",
                 (unsigned int)xPortGetFreeHeapSize(),
                 (unsigned int)xPortGetMinimumEverFreeHeapSize());
		MX_TouchGFX_Process();
        
#else
    // DIAGNOSTIC: TouchGFX and QSPI-mmap disabled — release the task gate and idle.
    LOG_INFO("[DIAG] TouchGFX/QSPI-mmap DISABLED for wifi isolation test");
    TouchGFX_init = true;
#endif


    for (;;) {
        // Reached only when TouchGFX is disabled (MX_TouchGFX_Process never
        // returns). Must yield — an empty spin at Normal priority starves the
        // FreeRTOS timer daemon and kills WiFi event delivery.
        osDelay(1000);
    }
}

void tcpClientTask(void *argument)
{
    (void)argument; // Mark argument as unused
    char messageBuffer[TCP_MSG_MAX_LEN];

    // Wait for Wi-Fi to be initialized
    while (!TouchGFX_init || !PeripheralsTestComplete) {
        osDelay(100);
    }

    // Additional delay to ensure Wi-Fi is fully connected
    //osDelay(5000);
    waitWifiInit();
    LOG_INFO("[TCP Client] Task started, waiting for messages...");

    for (;;) {
        // Wait for messages from UART log input
        if (xQueueReceive(tcpClientQueue, messageBuffer, portMAX_DELAY) == pdTRUE) {
            LOG_INFO("[TCP Client] Sending message: %s", messageBuffer);

            // Send message to Python server
            int32_t result = wifi_tcp_test("192.168.1.94", 8080,
                                          (const uint8_t*)messageBuffer,
                                          strlen(messageBuffer));

            if (result == 0) {
                LOG_INFO("[TCP Client] Message sent successfully");
            } else {
                LOG_ERROR("[TCP Client] Failed to send message");
            }
        }
    }
}

// void tcpServerTask(void *argument)
// {
//     // Wait for Wi-Fi to be initialized
//     while (!TouchGFX_init || !PeripheralsTestComplete) {
//         osDelay(100);
//     }

//     // Additional delay to ensure Wi-Fi is fully connected
//    // osDelay(10000);
//     waitWifiInit();

//     LOG_INFO("[TCP Server] Task started, initializing server...");

//     // Initialize TCP server on default port (8080)
//     if (wifi_tcp_server_init(0) != 0) {
//         LOG_ERROR("[TCP Server] Failed to initialize server");
//         goto error_exit;
//     }

//     // Start TCP server
//     if (wifi_tcp_server_start() != 0) {
//         LOG_ERROR("[TCP Server] Failed to start server");
//         goto error_exit;
//     }

//     LOG_INFO("[TCP Server] Server started successfully");

//     // Main server loop
//     for (;;) {
//         // Process server operations
//         int32_t clients_served = wifi_tcp_server_process();

//         if (clients_served < 0) {
//             LOG_ERROR("[TCP Server] Error processing server operations");
//             break;
//         }

//         // Give other tasks a chance to run
//         osDelay(10);  // Reduced from 100ms to 10ms for better responsiveness

//         // Periodically log server status
//         static uint32_t last_status_log = 0;
//         uint32_t now = HAL_GetTick();
//         if (now - last_status_log > 30000) { // Every 30 seconds
//             tcp_server_state_t state;
//             uint32_t active_clients, total_connections;
//             wifi_tcp_server_get_status(&state, &active_clients, &total_connections);

//             LOG_INFO("[TCP Server] Status: State=%d, Active=%lu, Total=%lu",
//                      state, active_clients, total_connections);
//             last_status_log = now;
//         }
//     }
 
// error_exit:
//     LOG_ERROR("[TCP Server] Task exiting due to error");   
//     wifi_tcp_server_stop();

//     // Task should not exit, but if it does, suspend itself  
//     for (;;) {
//         osDelay(10000);
//     }
// }

static void waitLogInit(void)
{
    do
    {
        /* code */
        osDelay(1000);

    } while(log_initialized != true);
    
}

static void waitWifiInit(void)
{
    do
    {
        /* code */
            osDelay(1000);
    } while (wifiDriverInit != true);
    
}
