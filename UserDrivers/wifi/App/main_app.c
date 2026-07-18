/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main_app.c
  * @author  GPM Application Team
  * @brief   main_app program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Application */
#include "main.h"
#include "main_app.h"
#include "app_config.h"
#include "echo.h"
#include "wifi_tcpClient.h"

#if (LOW_POWER_MODE > LOW_POWER_DISABLE)
#include "utilities_conf.h"
#include "stm32_lpm.h"
#endif /* LOW_POWER_MODE */

#include "w6x_api.h"
#include "common_parser.h" /* Common Parser functions */
#include "spi_iface.h" /* SPI falling/rising_callback */
#include "logging.h"
#include "shell.h"
#include "logshell_ctrl.h"
#include "gpio_interrupts.h"

#ifndef REDEFINE_FREERTOS_INTERFACE
/* Depending on the version of FreeRTOS the inclusion might need to be redefined in app_config.h */
//#include "app_freertos.h"

#include "cmsis_os.h"
#include "FreeRTOS.h"

#include "queue.h"
#include "event_groups.h"
#endif /* REDEFINE_FREERTOS_INTERFACE */

#if (LOW_POWER_MODE == LOW_POWER_STDBY_ENABLE)
#error "low power standby mode not supported"
#endif /* LOW_POWER_MODE */

/* USER CODE BEGIN Includes */
#ifdef USE_NVRAM_WIFI_CREDENTIALS
#include "user_config.h"
extern userconfig_t* g_userConfig;
#endif
/* USER CODE END Includes */

/* Global variables ----------------------------------------------------------*/
/* USER CODE BEGIN GV */

/* USER CODE END GV */

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  char *version;                  /*!< Version of the application */
  char *name;                     /*!< Name of the application */
} APP_Info_t;

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private defines -----------------------------------------------------------*/
#define EVENT_FLAG_SCAN_DONE   (1<<1)             /*!< Scan done event bitmask */

#define WIFI_SCAN_TIMEOUT      10000              /*!< Delay before to declare the scan in failure */

/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macros ------------------------------------------------------------*/
/** Stringify version */
#define XSTR(x) #x

/** Macro to stringify version */
#define MSTR(x) XSTR(x)

/** Application version */
#define HOST_APP_VERSION_STR      \
  MSTR(HOST_APP_VERSION_MAIN) "." \
  MSTR(HOST_APP_VERSION_SUB1) "." \
  MSTR(HOST_APP_VERSION_SUB2)

/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* Event bitmask flag used for asynchronous execution */
static EventGroupHandle_t scan_event_flags = NULL; /*!< Wi-Fi scan event flags */

#if (SHELL_ENABLE == 1)
static uint8_t quit_msg = 0;
#endif /* SHELL_ENABLE */

/** Application information */
static const APP_Info_t app_info =
{
  .name = "ST67W6X Wi-Fi Echo",
  .version = HOST_APP_VERSION_STR
};

/* USER CODE BEGIN PV */
bool wifiDriverInit = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/**
  * @brief  Wi-Fi event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_wifi_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  Network event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_net_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  MQTT event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_mqtt_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  BLE event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_ble_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  W6X error callback
  * @param  ret_w6x: W6X status
  * @param  func_name: function name
  */
static void APP_error_cb(W6X_Status_t ret_w6x, char const *func_name);

/**
  * @brief  Wi-Fi scan callback
  * @param  status: Scan status
  * @param  Scan_results: Scan results
  */
static void APP_wifi_scan_cb(int32_t status, W6X_WiFi_Scan_Result_t *Scan_results);

#if (SHELL_ENABLE == 1)
/**
  * @brief  Shell command to quit the application
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval ::SHELL_STATUS_OK on success
  */
int32_t APP_shell_quit(int32_t argc, char **argv);
#endif /* SHELL_ENABLE */

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void  main_app(void)
{
  int32_t ret = 0;

  /* USER CODE BEGIN main_app_1 */

  /* USER CODE END main_app_1 */

  /* Wi-Fi variables */
  W6X_WiFi_Scan_Opts_t Opts = {0};
  W6X_WiFi_Connect_Opts_t ConnectOpts = {0};

  /* Network ping variables */
  uint16_t ping_count = 4;
  uint32_t average_ping = 0;
  uint16_t ping_received_response = 0;

  /* Initialize the logging utilities */
  LoggingInit();
  /* Initialize the shell utilities on UART instance */
  ShellInit();

  LogInfo("#### Welcome to %s Application #####\n", app_info.name);
  LogInfo("# build: %s %s\n", __TIME__, __DATE__);
  LogInfo("--------------- Host info ---------------\n");
  LogInfo("Host FW Version:          %s\n", app_info.version);

  /* USER CODE BEGIN main_app_2 */
  /* Heap telemetry: every wifi SPI transaction allocates ~6KB from this heap
     (spi_iface.c spi_buffer_alloc). If free/min_ever is near 6KB, transfers
     fail silently and surface as W61_STATUS_TIMEOUT. */
  LogInfo("[HEAP] free=%u min_ever=%u (before W6X_Init)\n",
          (unsigned int)xPortGetFreeHeapSize(),
          (unsigned int)xPortGetMinimumEverFreeHeapSize());
  /* SPI_RDY diagnostics: RDY = raw PF5 level right now.
     EXTICR2 bits [7:4] route EXTI line 5; must read 0x5 (= port F).
     IMR5 = EXTI line-5 interrupt unmasked (must be 1).
     RDY=1 with the engine still timing out => edge was missed / EXTI dead.
     RDY=0 => NCP is not signalling at all (power/reset/boot problem). */
  LogInfo("[WIFI DIAG] RDY=%d EXTICR2=0x%08lX IMR5=%d\n",
          (int)HAL_GPIO_ReadPin(SPI_RDY_GPIO_Port, SPI_RDY_Pin),
          (unsigned long)SYSCFG->EXTICR[1],
          (int)((EXTI->IMR >> 5) & 1U));
  /* USER CODE END main_app_2 */

  /* Register the application callback to received events from ST67W6X Driver */
  W6X_App_Cb_t App_cb = {0};
  App_cb.APP_wifi_cb = APP_wifi_cb;
  App_cb.APP_net_cb = APP_net_cb;
  App_cb.APP_ble_cb = APP_ble_cb;
  App_cb.APP_mqtt_cb = APP_mqtt_cb;
  App_cb.APP_error_cb = APP_error_cb;
  W6X_RegisterAppCb(&App_cb);

  /* Initialize the ST67W6X Driver */
  ret = W6X_Init();
  if (ret)
  {
    LogError("failed to initialize ST67W6X Driver, %" PRIi32 "\n", ret);
    goto _err;
  }

  /* Initialize the ST67W6X Wi-Fi module */
  ret = W6X_WiFi_Init();
  if (ret)
  {
    LogError("failed to initialize ST67W6X Wi-Fi component, %" PRIi32 "\n", ret);
    goto _err;
  }
  LogInfo("Wi-Fi init is done\n");

  /* Set DTIM value (dtim * 100ms). 0: Disabled, 1: 100ms, 10: 1s */
  ret = W6X_WiFi_SetDTIM(0);
  if (ret)
  {
    LogError("failed to initialize the DTIM, %" PRIi32 "\n", ret);
    goto _err;
  }

  /* Initialize the ST67W6X Network module */
  ret = W6X_Net_Init();
  if (ret)
  {
    LogError("failed to initialize ST67W6X Net component, %" PRIi32 "\n", ret);
    goto _err;
  }
  LogInfo("Net init is done\n");

  /* USER CODE BEGIN main_app_3 */

  /* USER CODE END main_app_3 */
  /* Run a Wi-Fi scan to retrieve the list of all nearby Access Points */
  scan_event_flags = xEventGroupCreate();
  W6X_WiFi_Scan(&Opts, &APP_wifi_scan_cb);

  /* Wait to receive the EVENT_FLAG_SCAN_DONE event. The scan is declared as failed after 'ScanTimeout' delay */
  if ((int32_t)xEventGroupWaitBits(scan_event_flags, EVENT_FLAG_SCAN_DONE, pdTRUE, pdFALSE,
                                   pdMS_TO_TICKS(WIFI_SCAN_TIMEOUT)) != EVENT_FLAG_SCAN_DONE)
  {
    LogError("Scan Failed\n");
    goto _err;
  }

  /* Connect the device to the pre-defined Access Point */
  LogInfo("\nConnecting to Local Access Point\n");

  // Try to use NVRAM credentials if available, otherwise use hardcoded defaults
#ifdef USE_NVRAM_WIFI_CREDENTIALS
  const char* ssid = WIFI_SSID;  // Default fallback
  const char* password = WIFI_PASSWORD;  // Default fallback

  if (g_userConfig && userconfig_has_wifi_credentials(g_userConfig)) {
    ssid = userconfig_get_wifi_ssid(g_userConfig);
    password = userconfig_get_wifi_password(g_userConfig);
    LogInfo("Using WiFi credentials from NVRAM: %s\n", ssid);
  } else {
    LogInfo("Using default WiFi credentials from app_config.h\n");
  }

  strncpy((char *)ConnectOpts.SSID, ssid, W6X_WIFI_MAX_SSID_SIZE);
  strncpy((char *)ConnectOpts.Password, password, W6X_WIFI_MAX_PASSWORD_SIZE);
#else
  // Use hardcoded credentials from app_config.h
  strncpy((char *)ConnectOpts.SSID, WIFI_SSID, W6X_WIFI_MAX_SSID_SIZE);
  strncpy((char *)ConnectOpts.Password, WIFI_PASSWORD, W6X_WIFI_MAX_PASSWORD_SIZE);
  LogInfo("Using hardcoded WiFi credentials\n");
#endif

  ret = W6X_WiFi_Connect(&ConnectOpts);
  if (ret)
  {
    LogError("failed to connect, %" PRIi32 "\n", ret);
    goto _err;
  }
  LogInfo("App connected\n");

  #if WIFI_AP_PING_TEST
  /* Execute a ICMP request (ping) on remote url */
  LogInfo("\nPinging Google\n");
  ret = W6X_Net_Ping((uint8_t *)"www.google.com", 64, ping_count, 1000, &average_ping, &ping_received_response);
  if (ret == W6X_STATUS_OK)
  {
    if (ping_received_response == 0)
    {
      /* No response or ping in timeout */
      LogError("No ping received\n");
      goto _err;
    }
    else
    {
      /* Print the ping statistic with latency and packet loss */
      LogInfo("%" PRIu16" packets transmitted, %" PRIu16 " received, %" PRIu16 "%% packet loss, time %" PRIu32 "ms\n",
              ping_count, ping_received_response,
              100 * (ping_count - ping_received_response) / ping_count, average_ping);
    }
  }
  else
  {
    LogError("Ping failed\n");
    goto _err;
  }
#endif 

  // /* Execute ECHO test */
  // if (echo_sizes_loop(1, NULL) != 0)
  // {
  //   LogError("Echo failed\n");
  //   goto _err;
  // }

  // LogInfo("Successful Echo Test\n");

  /* Execute TCP test with Python server */
  // LogInfo("\nTesting TCP communication with Python server\n");
  // if (wifi_tcp_simple_test(0, NULL) != 0)
  // {
  //   LogError("TCP test failed\n");
  //   goto _err;
  // }

  LogInfo("Successful TCP Test\n");

  /* USER CODE BEGIN main_app_Last_1 */

  /* USER CODE END main_app_Last_1 */

#if (SHELL_ENABLE == 1)
  LogInfo("\nApplication runs in CLI mode. Type help or quit to exit.\n");
  while (quit_msg == 0)
  {
    vTaskDelay(1000);
  }
#endif /* SHELL_ENABLE */

  /* Disconnect the device from the Access Point */
  // ret = W6X_WiFi_Disconnect(1);
  // if (ret == W6X_STATUS_OK)
  // {
  //   LogInfo("Wi-Fi Disconnect success\n");
  // }
  // else
  // {
  //   LogError("Wi-Fi Disconnect failed\n");
  // }

  // LogInfo("##### Quitting the application\n");

  /* USER CODE BEGIN main_app_Last */

  /* USER CODE END main_app_Last */

_err:
  /* USER CODE BEGIN main_app_Err_1 */
  wifiDriverInit = false;
  LogInfo("[WIFI DIAG] at app end: RDY=%d EXTICR2=0x%08lX IMR5=%d\n",
          (int)HAL_GPIO_ReadPin(SPI_RDY_GPIO_Port, SPI_RDY_Pin),
          (unsigned long)SYSCFG->EXTICR[1],
          (int)((EXTI->IMR >> 5) & 1U));
  /* USER CODE END main_app_Err_1 */
  /* De-initialize the ST67W6X Network module */
  //W6X_Net_DeInit();

  /* De-initialize the ST67W6X Wi-Fi module */
  //W6X_WiFi_DeInit();

  /* De-initialize the ST67W6X Driver */
 // W6X_DeInit();

  /* USER CODE BEGIN main_app_Err_2 */

  /* USER CODE END main_app_Err_2 */
  LogInfo("##### Application end\n");
  wifiDriverInit = true;
}


void wifi_test(void)
{
  int32_t ret = 0;

  /* USER CODE BEGIN main_app_1 */

  /* USER CODE END main_app_1 */

  /* Wi-Fi variables */
  W6X_WiFi_Scan_Opts_t Opts = {0};
  W6X_WiFi_Connect_Opts_t ConnectOpts = {0};

  /* Network ping variables */
  uint16_t ping_count = 4;
  uint32_t average_ping = 0;
  uint16_t ping_received_response = 0;

  /* Initialize the logging utilities */
  LoggingInit();
  /* Initialize the shell utilities on UART instance */
  ShellInit();

  LogInfo("#### Welcome to %s Application #####\n", app_info.name);
  LogInfo("# build: %s %s\n", __TIME__, __DATE__);
  LogInfo("--------------- Host info ---------------\n");
  LogInfo("Host FW Version:          %s\n", app_info.version);

  /* USER CODE BEGIN main_app_2 */
  /* Heap telemetry: every wifi SPI transaction allocates ~6KB from this heap
     (spi_iface.c spi_buffer_alloc). If free/min_ever is near 6KB, transfers
     fail silently and surface as W61_STATUS_TIMEOUT. */
  LogInfo("[HEAP] free=%u min_ever=%u (before W6X_Init)\n",
          (unsigned int)xPortGetFreeHeapSize(),
          (unsigned int)xPortGetMinimumEverFreeHeapSize());
  /* SPI_RDY diagnostics: RDY = raw PF5 level right now.
     EXTICR2 bits [7:4] route EXTI line 5; must read 0x5 (= port F).
     IMR5 = EXTI line-5 interrupt unmasked (must be 1).
     RDY=1 with the engine still timing out => edge was missed / EXTI dead.
     RDY=0 => NCP is not signalling at all (power/reset/boot problem). */
  LogInfo("[WIFI DIAG] RDY=%d EXTICR2=0x%08lX IMR5=%d\n",
          (int)HAL_GPIO_ReadPin(SPI_RDY_GPIO_Port, SPI_RDY_Pin),
          (unsigned long)SYSCFG->EXTICR[1],
          (int)((EXTI->IMR >> 5) & 1U));
  /* USER CODE END main_app_2 */

  /* Register the application callback to received events from ST67W6X Driver */
  W6X_App_Cb_t App_cb = {0};
  App_cb.APP_wifi_cb = APP_wifi_cb;
  App_cb.APP_net_cb = APP_net_cb;
  App_cb.APP_ble_cb = APP_ble_cb;
  App_cb.APP_mqtt_cb = APP_mqtt_cb;
  App_cb.APP_error_cb = APP_error_cb;
  W6X_RegisterAppCb(&App_cb);

  /* Initialize the ST67W6X Driver */
  ret = W6X_Init();
  if (ret)
  {
    LogError("failed to initialize ST67W6X Driver, %" PRIi32 "\n", ret);
    goto _err;
  }

  /* Initialize the ST67W6X Wi-Fi module */
  ret = W6X_WiFi_Init();
  if (ret)
  {
    LogError("failed to initialize ST67W6X Wi-Fi component, %" PRIi32 "\n", ret);
    goto _err;
  }
  LogInfo("Wi-Fi init is done\n");

  /* Set DTIM value (dtim * 100ms). 0: Disabled, 1: 100ms, 10: 1s */
  ret = W6X_WiFi_SetDTIM(0);
  if (ret)
  {
    LogError("failed to initialize the DTIM, %" PRIi32 "\n", ret);
    goto _err;
  }

  /* Initialize the ST67W6X Network module */
  ret = W6X_Net_Init();
  if (ret)
  {
    LogError("failed to initialize ST67W6X Net component, %" PRIi32 "\n", ret);
    goto _err;
  }
  LogInfo("Net init is done\n");

  /* USER CODE BEGIN main_app_3 */

  /* USER CODE END main_app_3 */
  /* Run a Wi-Fi scan to retrieve the list of all nearby Access Points */
  scan_event_flags = xEventGroupCreate();
  W6X_WiFi_Scan(&Opts, &APP_wifi_scan_cb);

  /* Wait to receive the EVENT_FLAG_SCAN_DONE event. The scan is declared as failed after 'ScanTimeout' delay */
  if ((int32_t)xEventGroupWaitBits(scan_event_flags, EVENT_FLAG_SCAN_DONE, pdTRUE, pdFALSE,
                                   pdMS_TO_TICKS(WIFI_SCAN_TIMEOUT)) != EVENT_FLAG_SCAN_DONE)
  {
    LogError("Scan Failed\n");
    goto _err;
  }

  /* Connect the device to the pre-defined Access Point */
  LogInfo("\nConnecting to Local Access Point\n");

  // Try to use NVRAM credentials if available, otherwise use hardcoded defaults
#ifdef USE_NVRAM_WIFI_CREDENTIALS
  const char* ssid = WIFI_SSID;  // Default fallback
  const char* password = WIFI_PASSWORD;  // Default fallback

  if (g_userConfig && userconfig_has_wifi_credentials(g_userConfig)) {
    ssid = userconfig_get_wifi_ssid(g_userConfig);
    password = userconfig_get_wifi_password(g_userConfig);
    LogInfo("Using WiFi credentials from NVRAM: %s\n", ssid);
  } else {
    LogInfo("Using default WiFi credentials from app_config.h\n");
  }

  strncpy((char *)ConnectOpts.SSID, ssid, W6X_WIFI_MAX_SSID_SIZE);
  strncpy((char *)ConnectOpts.Password, password, W6X_WIFI_MAX_PASSWORD_SIZE);
#else
  // Use hardcoded credentials from app_config.h
  strncpy((char *)ConnectOpts.SSID, WIFI_SSID, W6X_WIFI_MAX_SSID_SIZE);
  strncpy((char *)ConnectOpts.Password, WIFI_PASSWORD, W6X_WIFI_MAX_PASSWORD_SIZE);
  LogInfo("Using hardcoded WiFi credentials\n");
#endif

  ret = W6X_WiFi_Connect(&ConnectOpts);
  if (ret)
  {
    LogError("failed to connect, %" PRIi32 "\n", ret);
    goto _err;
  }
  LogInfo("App connected\n");

  #if WIFI_TEST_AP_PING_TEST
  /* Execute a ICMP request (ping) on remote url */
  LogInfo("\nPinging electronicsbuilder\n");
  ret = W6X_Net_Ping((uint8_t *)"www.electronicsbuilder.com", 64, ping_count, 1000, &average_ping, &ping_received_response);
  if (ret == W6X_STATUS_OK)
  {
    if (ping_received_response == 0)
    {
      /* No response or ping in timeout */
      LogError("No ping received\n");
      goto _err;
    }
    else
    {
      /* Print the ping statistic with latency and packet loss */
      LogInfo("%" PRIu16" packets transmitted, %" PRIu16 " received, %" PRIu16 "%% packet loss, time %" PRIu32 "ms\n",
              ping_count, ping_received_response,
              100 * (ping_count - ping_received_response) / ping_count, average_ping);
    }
  }
  else
  {
    LogError("Ping failed\n");
    goto _err;
  }
#endif 

  // /* Execute ECHO test */
  if (echo_sizes_loop(1, NULL) != 0)
  {
    LogError("Echo failed\n");
    goto _err;
  }

  LogInfo("Successful Echo Test\n");

  /* USER CODE BEGIN main_app_Last_1 */

  /* USER CODE END main_app_Last_1 */

#if (SHELL_ENABLE == 1)
  LogInfo("\nApplication runs in CLI mode. Type help or quit to exit.\n");
  while (quit_msg == 0)
  {
    vTaskDelay(1000);
  }
#endif /* SHELL_ENABLE */

  /* Disconnect the device from the Access Point */
  ret = W6X_WiFi_Disconnect(1);
  if (ret == W6X_STATUS_OK)
  {
    LogInfo("Wi-Fi Disconnect success\n");
  }
  else
  {
    LogError("Wi-Fi Disconnect failed\n");
  }

  // LogInfo("##### Quitting the application\n");

  /* USER CODE BEGIN main_app_Last */

  /* USER CODE END main_app_Last */

_err:
  /* USER CODE BEGIN main_app_Err_1 */
  wifiDriverInit = false;
  LogInfo("[WIFI DIAG] at app end: RDY=%d EXTICR2=0x%08lX IMR5=%d\n",
          (int)HAL_GPIO_ReadPin(SPI_RDY_GPIO_Port, SPI_RDY_Pin),
          (unsigned long)SYSCFG->EXTICR[1],
          (int)((EXTI->IMR >> 5) & 1U));
  /* USER CODE END main_app_Err_1 */
  /* De-initialize the ST67W6X Network module */
  W6X_Net_DeInit();

  /* De-initialize the ST67W6X Wi-Fi module */
  W6X_WiFi_DeInit();

  /* De-initialize the ST67W6X Driver */
  // W6X_DeInit();

  /* USER CODE BEGIN main_app_Err_2 */

  /* USER CODE END main_app_Err_2 */
  LogInfo("##### Application end\n");
  wifiDriverInit = true;
}
/* USER CODE BEGIN MX_App_Init */
void MX_App_Echo_Init(void);
void MX_App_Echo_Init(void)
{
  /* This function is not supposed to be filled, created just for compilation purpose
     in case user forgets to uncheck the STM32CubeMX GUI box to avoid its call in main()
     The application initialization is done by the main_app() function on FreeRTOS task. */
  return;
}
/* USER CODE END MX_App_Init */

void HAL_GPIO_EXTI_Callback(uint16_t pin);
void HAL_GPIO_EXTI_Rising_Callback(uint16_t pin);
void HAL_GPIO_EXTI_Falling_Callback(uint16_t pin);

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
  /* USER CODE BEGIN HAL_GPIO_EXTI_Callback_1 */

  /* USER CODE END HAL_GPIO_EXTI_Callback_1 */
  /* Callback when data is available in Network CoProcessor to enable SPI Clock */
  if (pin == SPI_RDY_Pin)
  {
    if (HAL_GPIO_ReadPin(SPI_RDY_GPIO_Port, SPI_RDY_Pin) == GPIO_PIN_SET)
    {
      HAL_GPIO_EXTI_Rising_Callback(pin);
    }
    else
    {
      HAL_GPIO_EXTI_Falling_Callback(pin);
    }
  }

    /* Route to centralized GPIO interrupt handler for all sensors */
  GPIO_Interrupts_HandleCallback(pin);

  /* USER CODE END HAL_GPIO_EXTI_Callback_End */
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t pin)
{
  /* USER CODE BEGIN EXTI_Rising_Callback_1 */

  /* USER CODE END EXTI_Rising_Callback_1 */
  /* Callback when data is available in Network CoProcessor to enable SPI Clock */
  if (pin == SPI_RDY_Pin)
  {
    spi_on_txn_data_ready();
  }
  /* USER CODE BEGIN EXTI_Rising_Callback_End */
  /* USER CODE BEGIN HAL_GPIO_EXTI_Callback_End */

  /* USER CODE END EXTI_Rising_Callback_End */
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t pin)
{
  /* USER CODE BEGIN EXTI_Falling_Callback_1 */

  /* USER CODE END EXTI_Falling_Callback_1 */
  /* Callback when data is available in Network CoProcessor to enable SPI Clock */
  if (pin == SPI_RDY_Pin)
  {
    spi_on_header_ack();
  }

  /* Callback when user button is pressed */
  // if (pin == USER_BUTTON_Pin)
  // {
  // }
  /* USER CODE BEGIN EXTI_Falling_Callback_End */

  /* USER CODE END EXTI_Falling_Callback_End */
}

/* USER CODE BEGIN FD */

/* USER CODE END FD */

/* Private Functions Definition ----------------------------------------------*/
static void APP_wifi_scan_cb(int32_t status, W6X_WiFi_Scan_Result_t *Scan_results)
{
  /* USER CODE BEGIN APP_wifi_scan_cb_1 */

  /* USER CODE END APP_wifi_scan_cb_1 */
  LogInfo("SCAN DONE\n");
  LogInfo(" Cb informed APP that WIFI SCAN DONE.\n");
  W6X_WiFi_PrintScan(Scan_results);
  xEventGroupSetBits(scan_event_flags, EVENT_FLAG_SCAN_DONE);
  /* USER CODE BEGIN APP_wifi_scan_cb_End */

  /* USER CODE END APP_wifi_scan_cb_End */
}

static void APP_wifi_cb(W6X_event_id_t event_id, void *event_args)
{
  /* USER CODE BEGIN APP_wifi_cb_1 */
 
  /* USER CODE END APP_wifi_cb_1 */

  W6X_WiFi_Connect_t connectData = {0};
  W6X_WiFi_StaStateType_e state = W6X_WIFI_STATE_STA_OFF;

  switch (event_id)
  {
    case W6X_WIFI_EVT_CONNECTED_ID:
      if (W6X_WiFi_GetStaState(&state, &connectData) != W6X_STATUS_OK)
      {
        LogInfo("Connected to an Access Point\n");
        return;
      }

      LogInfo("Connected to following Access Point :\n");
      LogInfo("[" MACSTR "] Channel: %" PRIu32 " | RSSI: %" PRIi32 " | SSID: %s\n",
              MAC2STR(connectData.MAC),
              connectData.Channel,
              connectData.Rssi,
              connectData.SSID);
      break;
    case W6X_WIFI_EVT_DISCONNECTED_ID:
      LogInfo("Station disconnected from Access Point\n");
      break;

    case W6X_WIFI_EVT_REASON_ID:
      LogInfo("Reason: %s\n", W6X_WiFi_ReasonToStr(event_args));
      break;

    default:
      break;
  }
  /* USER CODE BEGIN APP_wifi_cb_End */

  /* USER CODE END APP_wifi_cb_End */
}

static void APP_net_cb(W6X_event_id_t event_id, void *event_args)
{
  /* USER CODE BEGIN APP_net_cb_1 */

  /* USER CODE END APP_net_cb_1 */

  W6X_Net_CbParamData_t *p_param_app_net_cb;

  switch (event_id)
  {
    case W6X_NET_EVT_SOCK_DATA_ID:
      p_param_app_net_cb = (W6X_Net_CbParamData_t *) event_args;
      LogInfo(" Cb informed app that Wi-Fi %" PRIu32 " bytes available on socket %" PRIu32 ".\n",
              p_param_app_net_cb->available_data_length, p_param_app_net_cb->socket_id);
      break;

    default:
      break;
  }
  /* USER CODE BEGIN APP_net_cb_End */

  /* USER CODE END APP_net_cb_End */
}

static void APP_mqtt_cb(W6X_event_id_t event_id, void *event_args)
{
  /* USER CODE BEGIN APP_mqtt_cb_1 */

  /* USER CODE END APP_mqtt_cb_1 */
}

static void APP_ble_cb(W6X_event_id_t event_id, void *event_args)
{
  /* USER CODE BEGIN APP_ble_cb_1 */

  /* USER CODE END APP_ble_cb_1 */
}

static void APP_error_cb(W6X_Status_t ret_w6x, char const *func_name)
{
  /* USER CODE BEGIN APP_error_cb_1 */

  /* USER CODE END APP_error_cb_1 */
  LogError("[%s] in %s API\n", W6X_StatusToStr(ret_w6x), func_name);
  /* USER CODE BEGIN APP_error_cb_2 */

  /* USER CODE END APP_error_cb_2 */
}

#if (SHELL_ENABLE == 1)
int32_t APP_shell_quit(int32_t argc, char **argv)
{
  quit_msg = 1;
  return SHELL_STATUS_OK;
}

SHELL_CMD_EXPORT_ALIAS(APP_shell_quit, quit, quit. Stop application execution);
#endif /* SHELL_ENABLE */

/* USER CODE BEGIN PFD */

/* USER CODE END PFD */
