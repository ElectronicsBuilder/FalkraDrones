/**
  ******************************************************************************
  * @file          : app_tof.c
  * @author        : IMG SW Application Team
  * @brief         : This file provides code for the configuration
  *                  of the STMicroelectronics.X-CUBE-TOF1.3.1.0 instances.
  ******************************************************************************
  *
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "app_tof.hpp"
#include "main.h"
#include <stdio.h>

#include "53l5a1_ranging_sensor.h"
#include "app_tof_pin_conf.h"

#include "stm32f7xx_custom.h"

#include "tof_proximity.hpp"
#include "FreeRTOS.h"
#include "cmsis_os.h"

#include "status.hpp"
#include "log.hpp"
#include "tof_speed_opts.h"



/* Private typedef -----------------------------------------------------------*/

extern uint16_t proximity_devices[MAX_TOF_SENSOR][MAX_ZONE_PER_SENSOR];
extern tof_sensor_t sensors[MAX_TOF_SENSOR];
extern uint8_t TOF_DEVC_INIT_DONE;

RANGING_SENSOR_Result_t Result;

#define TOF_ADDR_BASE  0x54  // first sensor gets 0x54, then +2 each

/* Private define ------------------------------------------------------------*/
#define TIMING_BUDGET (5U)  //(15U) /* 15 ms timing budget (balanced accuracy/speed) */
#define RANGING_FREQUENCY (60U) /* 60 Hz ranging frequency (VL53L5CX max at 4x4) */
#define POLLING_PERIOD (1000U/RANGING_FREQUENCY) /* refresh rate for polling mode (milliseconds) */

/* Private variables ---------------------------------------------------------*/
static RANGING_SENSOR_ProfileConfig_t Profile;
static int32_t status = 0;
static uint8_t ToF_Present[RANGING_SENSOR_INSTANCES_NBR] = {0};
volatile uint8_t ToF_EventDetected = 0;

/**
 * @brief Bidirectional mapping between logical sensor indices and BSP active indices
 * Built during initialization, read-only afterward
 *
 * - logical_to_bsp[logical_idx] = BSP active_index (0xFF if sensor inactive)
 * - bsp_to_logical[active_idx] = logical sensor index
 * - active_count = number of sensors successfully initialized
 */
typedef struct {
    uint8_t logical_to_bsp[MAX_TOF_SENSOR];  // Maps logical idx (0-5) -> BSP idx
    uint8_t bsp_to_logical[MAX_TOF_SENSOR];  // Maps BSP idx -> logical idx
    uint8_t active_count;                     // Number of active sensors
} ToFIndexMap_t;

static ToFIndexMap_t g_tof_index_map = {
    .logical_to_bsp = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // Invalid by default
    .bsp_to_logical = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    .active_count = 0
};


static const char* tofDevStr[] = {
    "TOP",     // index 0
    "BOTTOM",  // index 1
    "FRONT",   // index 2
    "BACK",    // index 3
    "LEFT",    // index 4
    "RIGHT"    // index 5
};


static const tof_sensor_id_t sensor_init_order[] = {
    tof_top,
    tof_bottom,
    tof_front,
    tof_back,
    tof_left,
    tof_right
};




/* Private function prototypes -----------------------------------------------*/
static void MX_53L5A1_MultiSensorRanging_Init(void);
static void MX_53L5A1_MultiSensorRanging_Process(void);
static void MX_53L5A1_MultiSensorRanging_Start(void);


static void print_result(RANGING_SENSOR_Result_t *Result);
static void write_lowpower_pin(uint8_t device, GPIO_PinState pin_state);
static void reset_all_sensors(void);
static void copy_distance_val(uint16_t TOF_DEV, RANGING_SENSOR_Result_t *Result);
//void enable_sensor_power(uint8_t device, GPIO_PinState state);
static void enable_sensor_power(uint8_t device_index, GPIO_PinState pin_state);


static RANGING_SENSOR_Capabilities_t Cap;
//static RANGING_SENSOR_ProfileConfig_t Profile;

void MX_TOF_Init(void)
{

  MX_53L5A1_MultiSensorRanging_Init();

}

/*
 * LM background task
 */
void MX_TOF_Process(void)
{
  /* USER CODE BEGIN TOF_Process_PreTreatment */

  /* USER CODE END TOF_Process_PreTreatment */

  MX_53L5A1_MultiSensorRanging_Process();

  /* USER CODE BEGIN TOF_Process_PostTreatment */

  /* USER CODE END TOF_Process_PostTreatment */
}



static void MX_53L5A1_MultiSensorRanging_Init(void)
{
    LOG_INFO("VL53L5A1 Multi Sensor Init");

    uint16_t i2c_addr;
    uint32_t id;
    int32_t status;
    uint8_t active_index = 0;

    // 1. Reset all sensors (power + LPn low)
    for (uint8_t i = 0; i < MAX_TOF_SENSOR; i++) {
        enable_sensor_power(i, GPIO_PIN_RESET); osDelay(10);
        write_lowpower_pin(i, GPIO_PIN_RESET); osDelay(10);
    }
    osDelay(100);

    // 2. Initialize only active sensors
    for (uint8_t logical_idx = 0; logical_idx < MAX_TOF_SENSOR; logical_idx++) {

        // if (sensors[logical_idx].status != sensor_active)
        //     continue;

        osDelay(250);  // Ensure bus is quiet before powering
        enable_sensor_power(logical_idx, GPIO_PIN_SET);
        osDelay(10);
        write_lowpower_pin(logical_idx, GPIO_PIN_SET);
        osDelay(10);

        // Init sensor at default address
        status = VL53L5A1_RANGING_SENSOR_Init(active_index);
        if (status != BSP_ERROR_NONE) {
            LOG_WARN("Init failed for sensor %s", sensors[logical_idx].name);
            ToF_Present[logical_idx] = 0;
            sensors[logical_idx].status = sensor_inactive;
            continue;
        }

        // Assign unique I2C address (e.g., 0x54, 0x56, ...)
        i2c_addr = TOF_ADDR_BASE + active_index * 2;
        status = VL53L5A1_RANGING_SENSOR_SetAddress(active_index, i2c_addr);
        osDelay(100);
        if (status != BSP_ERROR_NONE) {
            LOG_WARN("SetAddress failed for sensor %s", sensors[logical_idx].name);
            ToF_Present[logical_idx] = 0;
            sensors[logical_idx].status = sensor_inactive;
            continue;
        }

        // Patch platform address manually
        VL53L5CX_Object_t *pObj = (VL53L5CX_Object_t *)VL53L5A1_RANGING_SENSOR_CompObj[active_index];
        pObj->Dev.platform.address = i2c_addr;

        // Confirm device ID
        uint8_t retry = 2;
        do {
            status = VL53L5A1_RANGING_SENSOR_ReadID(active_index, &id);
            osDelay(50);
        } while ((status != BSP_ERROR_NONE || id == 0x0000) && --retry > 0);

        if (status == BSP_ERROR_NONE && id != 0x0000) {
            LOG_INFO("ToF sensor %s - ID: %04lX @ Addr: 0x%02X", sensors[logical_idx].name, (unsigned long)id, i2c_addr);
            ToF_Present[logical_idx] = 1;
            sensors[logical_idx].status = sensor_active;
            // Build bidirectional index mapping
            g_tof_index_map.logical_to_bsp[logical_idx] = active_index;
            g_tof_index_map.bsp_to_logical[active_index] = logical_idx;
            g_tof_index_map.active_count++;

            LOG_INFO("[TOF_INIT] %s: logical_idx=%u -> BSP_idx=%u, I2C_addr=0x%02X",
                sensors[logical_idx].name, logical_idx, active_index, i2c_addr);
        } else {
            LOG_WARN("ID read failed for sensor %s", sensors[logical_idx].name);
            ToF_Present[logical_idx] = 0;
            sensors[logical_idx].status = sensor_inactive;
        }

        active_index++;  // Move to next available BSP slot
    }

    // Optional: scan for confirmation
    for (uint8_t addr = 0x52; addr <= 0x5E; addr += 2) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, addr, 1, 10) == HAL_OK) {
            LOG_INFO("Device found at 0x%02X", addr);
        }
    }

    LOG_INFO("All sensors initialized");
}

/**
 * @brief Synchronize sensor status based on I2C detection results
 * Called after MX_TOF_Init() completes to update runtime status
 *
 * Updates sensors[].status based on ToF_Present[] array:
 * - ToF_Present[i] == 1 → sensor_active
 * - ToF_Present[i] == 0 → sensor_inactive
 */
void MX_TOF_SyncSensorStatus(void) {
    LOG_INFO("[TOF_SYNC] Synchronizing sensor status from I2C detection...");

    for (uint8_t i = 0; i < MAX_TOF_SENSOR; i++) {
        tof_sensor_status_t previous_status = sensors[i].status;

        // Update status based on runtime detection
        if (ToF_Present[i] == 1) {
            sensors[i].status = sensor_active;
        } else {
            sensors[i].status = sensor_inactive;
        }

        // Log status changes
        if (previous_status != sensors[i].status) {
            LOG_INFO("[TOF_SYNC] %s: %s -> %s",
                sensors[i].name,
                previous_status == sensor_active ? "ACTIVE" : "INACTIVE",
                sensors[i].status == sensor_active ? "ACTIVE" : "INACTIVE");
        }

        // Update global status structure
        g_status.tofDeviceStatus[i] = sensors[i].status;
    }

    LOG_INFO("[TOF_SYNC] Active sensors: %u/%u", g_tof_index_map.active_count, MAX_TOF_SENSOR);
}

void MX_TOF_Start(void)
{
	MX_53L5A1_MultiSensorRanging_Start();
}



static void MX_53L5A1_MultiSensorRanging_Start(void)
{
    RANGING_SENSOR_ProfileConfig_t Profile;

    Profile.RangingProfile = RS_PROFILE_4x4_AUTONOMOUS;
    Profile.TimingBudget = TIMING_BUDGET;         // 5–100 ms
    Profile.Frequency     = RANGING_FREQUENCY;    // Hz
    Profile.EnableAmbient = 0;
    Profile.EnableSignal  = 0;

    uint8_t active_index = 0;

    for (uint8_t logical_idx = 0; logical_idx < MAX_TOF_SENSOR; logical_idx++)
    {
        if (sensors[logical_idx].status != sensor_active)
            continue;

        if (ToF_Present[logical_idx] != 1)
            continue;

        VL53L5A1_RANGING_SENSOR_ConfigProfile(active_index, &Profile);

        int32_t status = VL53L5A1_RANGING_SENSOR_Start(
            active_index,
#if TOF_OPT_ASYNC_MODE
            RS_MODE_ASYNC_CONTINUOUS
#else
            RS_MODE_BLOCKING_CONTINUOUS
#endif
        );
        if (status != BSP_ERROR_NONE)
        {
            LOG_WARN("VL53L5A1_RANGING_SENSOR_Start failed for sensor %s (idx %d)", sensors[logical_idx].name, active_index);
            while (1);  // You can replace with error handler
        }

        active_index++;
    }
}


static void MX_53L5A1_MultiSensorRanging_Process(void)
{
  uint8_t i;

  //Profile.RangingProfile = RS_PROFILE_4x4_CONTINUOUS;
  Profile.RangingProfile =   RS_PROFILE_4x4_AUTONOMOUS;


  Profile.TimingBudget = TIMING_BUDGET; /* 5 ms < TimingBudget < 100 ms */
  Profile.Frequency = RANGING_FREQUENCY; /* Ranging frequency Hz (shall be consistent with TimingBudget value) */
  Profile.EnableAmbient = 0; /* Enable: 1, Disable: 0 */
  Profile.EnableSignal = 0; /* Enable: 1, Disable: 0 */

  for (i = 0; i < RANGING_SENSOR_INSTANCES_NBR; i++)
  {
    /* skip this device if not detected */
    if (ToF_Present[i] != 1) continue;

    VL53L5A1_RANGING_SENSOR_ConfigProfile(i, &Profile);
    //status = VL53L5A1_RANGING_SENSOR_Start(i, RS_MODE_ASYNC_CONTINUOUS);
    status = VL53L5A1_RANGING_SENSOR_Start(i, RS_MODE_BLOCKING_CONTINUOUS);


    if (status != BSP_ERROR_NONE)
    {
      LOG_INFO("VL53L3A2_RANGING_SENSOR_Start %d failed\r\n", i);
      while(1);
    }
  }

  while (1)
  {
	
    /* polling mode */
    for (i = 0; i < RANGING_SENSOR_INSTANCES_NBR; i++)
    {
       if (!ToF_Present[i]) continue;

      status = VL53L5A1_RANGING_SENSOR_GetDistance(i, &Result); 

      if (status == BSP_ERROR_NONE)
      {
    		copy_distance_val(i, &Result);
    		osDelay(POLLING_PERIOD);
        	if(g_status.tofCmdStatus == tof_system_state_t::sensors_paused)		// stop all measurement
        	{
        		break;
        	}
      }
    }


if (g_status.tofCmdStatus == tof_system_state_t::sensors_paused)		// stop all measurement
	{
		VL53L5A1_RANGING_SENSOR_Stop(i);
		break;
	}

    break; //todo added here for testing. need  to remove 
  }

}


void Return_distance(void)
{
    // Loop through logical sensor indices (0-5 for locations)
    for (uint8_t logical_idx = 0; logical_idx < MAX_TOF_SENSOR; logical_idx++)
    {
        // OPTIMIZATION 1: Skip sensors not detected on I2C bus
        if (!ToF_Present[logical_idx]) {
            continue;  // Save I2C bandwidth - sensor not present
        }

        // OPTIMIZATION 2: Skip sensors marked inactive after sync
        if (sensors[logical_idx].status != sensor_active) {
            continue;  // Respect dynamic status updates
        }

        // INDEX TRANSLATION: Convert logical index to BSP index
        uint8_t bsp_idx = g_tof_index_map.logical_to_bsp[logical_idx];
        if (bsp_idx == 0xFF) {
            // Invalid mapping - should never happen after proper init
            LOG_ERROR("[TOF_POLL] Invalid mapping for sensor %u", logical_idx);
            continue;
        }

        // Poll sensor using BSP index (for I2C communication)
        status = VL53L5A1_RANGING_SENSOR_GetDistance(bsp_idx, &Result);

        if (status == BSP_ERROR_NONE)
        {
            // Store distance data using LOGICAL index (maintains location semantics)
            copy_distance_val(logical_idx, &Result);
        }
        else if(g_status.tofCmdStatus == tof_system_state_t::sensors_paused)
        {
            // Handle pause command (existing logic)
        }
    }
}




static void copy_distance_val(uint16_t TOF_DEV, RANGING_SENSOR_Result_t *Result)
{


	for(uint16_t i = 0; i < MAX_ZONE_PER_SENSOR; i++)
	{
        uint32_t distanceMm = Result->ZoneResult[i].Distance[0];
#if TOF_OPT_STATUS_FILTER
        if (Result->ZoneResult[i].Status[0] != 0U) {
            distanceMm = 0;
        }
#endif
#if TOF_OPT_MM_RESOLUTION
        if (distanceMm > UINT16_MAX) {
            distanceMm = UINT16_MAX;
        }
        proximity_devices[TOF_DEV][i] = (uint16_t)distanceMm;
#else
        proximity_devices[TOF_DEV][i] = (long)distanceMm / 10;   // distance in cm
#endif

	}



}




static void write_lowpower_pin(uint8_t device_index, GPIO_PinState pin_state)
{
    static const tof_sensor_id_t sensor_id_map[] = {
        tof_top,     // index 0
        tof_bottom,  // index 1
        tof_front,   // index 2
        tof_back,    // index 3
        tof_left,    // index 4
        tof_right    // index 5
    };

    if (device_index >= sizeof(sensor_id_map) / sizeof(sensor_id_map[0])) {
        LOG_WARN("Invalid sensor index: %d", device_index);
        return;
    }

    tof_sensor_id_t sensor_id = sensor_id_map[device_index];

    switch (sensor_id)
    {
        case tof_top:
            HAL_GPIO_WritePin(VL53L5A1_LPn_TOP_PORT, VL53L5A1_LPn_TOP_PIN, pin_state);
            break;
        case tof_bottom:
            HAL_GPIO_WritePin(VL53L5A1_LPn_BOTTOM_PORT, VL53L5A1_LPn_BOTTOM_PIN, pin_state);
            break;
        case tof_front:
            HAL_GPIO_WritePin(VL53L5A1_LPn_FRONT_PORT, VL53L5A1_LPn_FRONT_PIN, pin_state);
            break;
        case tof_back:
            HAL_GPIO_WritePin(VL53L5A1_LPn_BACK_PORT, VL53L5A1_LPn_BACK_PIN, pin_state);
            break;
        case tof_left:
            HAL_GPIO_WritePin(VL53L5A1_LPn_LEFT_PORT, VL53L5A1_LPn_LEFT_PIN, pin_state);
            break;
        case tof_right:
            HAL_GPIO_WritePin(VL53L5A1_LPn_RIGHT_PORT, VL53L5A1_LPn_RIGHT_PIN, pin_state);
            break;
        default:
            LOG_WARN("Unknown sensor ID: %d", sensor_id);
            break;
    }

    LOG_INFO("LPn[%s] %s", 
        (sensor_id == tof_top ? "TOP" :
         sensor_id == tof_bottom ? "BOTTOM" :
         sensor_id == tof_front ? "FRONT" :
         sensor_id == tof_back ? "BACK" :
         sensor_id == tof_left ? "LEFT" : "RIGHT"),
        pin_state == GPIO_PIN_SET ? "ON" : "OFF"
    );

    osDelay(100);
}


static void reset_all_sensors(void)
{

    HAL_GPIO_WritePin(VL53L5A1_PWR_EN_TOP_PORT, VL53L5A1_PWR_EN_TOP_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(VL53L5A1_PWR_EN_BOTTOM_PORT, VL53L5A1_PWR_EN_BOTTOM_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(VL53L5A1_PWR_EN_FRONT_PORT, VL53L5A1_PWR_EN_FRONT_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(VL53L5A1_PWR_EN_BACK_PORT, VL53L5A1_PWR_EN_BACK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(VL53L5A1_PWR_EN_LEFT_PORT, VL53L5A1_PWR_EN_LEFT_PIN, GPIO_PIN_RESET);
    osDelay(100);

    HAL_GPIO_WritePin(VL53L5A1_PWR_EN_TOP_PORT, VL53L5A1_PWR_EN_TOP_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(VL53L5A1_PWR_EN_BOTTOM_PORT, VL53L5A1_PWR_EN_BOTTOM_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(VL53L5A1_PWR_EN_FRONT_PORT, VL53L5A1_PWR_EN_FRONT_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(VL53L5A1_PWR_EN_BACK_PORT, VL53L5A1_PWR_EN_BACK_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(VL53L5A1_PWR_EN_LEFT_PORT, VL53L5A1_PWR_EN_LEFT_PIN, GPIO_PIN_SET);
    osDelay(100);

    HAL_GPIO_WritePin(VL53L5A1_LPn_TOP_PORT, VL53L5A1_LPn_TOP_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(VL53L5A1_LPn_BOTTOM_PORT, VL53L5A1_LPn_BOTTOM_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(VL53L5A1_LPn_FRONT_PORT, VL53L5A1_LPn_FRONT_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(VL53L5A1_LPn_BACK_PORT, VL53L5A1_LPn_BACK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(VL53L5A1_LPn_LEFT_PORT, VL53L5A1_LPn_LEFT_PIN, GPIO_PIN_RESET);
    osDelay(100);

    HAL_GPIO_WritePin(VL53L5A1_LPn_TOP_PORT, VL53L5A1_LPn_TOP_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(VL53L5A1_LPn_BOTTOM_PORT, VL53L5A1_LPn_BOTTOM_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(VL53L5A1_LPn_FRONT_PORT, VL53L5A1_LPn_FRONT_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(VL53L5A1_LPn_BACK_PORT, VL53L5A1_LPn_BACK_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(VL53L5A1_LPn_LEFT_PORT, VL53L5A1_LPn_LEFT_PIN, GPIO_PIN_SET);
    osDelay(100);

}


static void enable_sensor_power(uint8_t device_index, GPIO_PinState pin_state)
{
    // Map logical index (0–5) to sensor ID
    static const tof_sensor_id_t sensor_id_map[] = {
        tof_top,     // 0
        tof_bottom,  // 1
        tof_front,   // 2
        tof_back,    // 3
        tof_left,    // 4
        tof_right    // 5
    };

    if (device_index >= sizeof(sensor_id_map) / sizeof(sensor_id_map[0])) {
        LOG_WARN("Invalid sensor index: %d", device_index);
        return;
    }

    tof_sensor_id_t sensor_id = sensor_id_map[device_index];

    switch (sensor_id)
    {
        case tof_top:
            HAL_GPIO_WritePin(VL53L5A1_PWR_EN_TOP_PORT, VL53L5A1_PWR_EN_TOP_PIN, pin_state);
            break;
        case tof_bottom:
            HAL_GPIO_WritePin(VL53L5A1_PWR_EN_BOTTOM_PORT, VL53L5A1_PWR_EN_BOTTOM_PIN, pin_state);
            break;
        case tof_front:
            HAL_GPIO_WritePin(VL53L5A1_PWR_EN_FRONT_PORT, VL53L5A1_PWR_EN_FRONT_PIN, pin_state);
            break;
        case tof_back:
            HAL_GPIO_WritePin(VL53L5A1_PWR_EN_BACK_PORT, VL53L5A1_PWR_EN_BACK_PIN, pin_state);
            break;
        case tof_left:
            HAL_GPIO_WritePin(VL53L5A1_PWR_EN_LEFT_PORT, VL53L5A1_PWR_EN_LEFT_PIN, pin_state);
            break;
        case tof_right:
            HAL_GPIO_WritePin(VL53L5A1_PWR_EN_RIGHT_PORT, VL53L5A1_PWR_EN_RIGHT_PIN, pin_state);
            break;
        default:
            LOG_WARN("Unknown sensor ID for power: %d", sensor_id);
            break;
    }

    LOG_INFO("PWR_EN[%s] %s",
        (sensor_id == tof_top ? "TOP" :
         sensor_id == tof_bottom ? "BOTTOM" :
         sensor_id == tof_front ? "FRONT" :
         sensor_id == tof_back ? "BACK" :
         sensor_id == tof_left ? "LEFT" : "RIGHT"),
        pin_state == GPIO_PIN_SET ? "ON" : "OFF"
    );

    osDelay(100);
}

/**
 * @brief Check if a ToF sensor is physically present on the bus
 * @param sensor_index Sensor index (0-5 for TOP, BOTTOM, FRONT, BACK, LEFT, RIGHT)
 * @return 1 if sensor detected, 0 if not present or invalid index
 */
uint8_t MX_TOF_IsSensorPresent(uint8_t sensor_index) {
    if (sensor_index >= RANGING_SENSOR_INSTANCES_NBR) {
        return 0;  // Invalid index
    }
    return ToF_Present[sensor_index];
}

/**
 * @brief Read distance from a single sensor (interrupt-driven use)
 * @param sensor_index Logical sensor index (0-5 for TOP, BOTTOM, FRONT, BACK, LEFT, RIGHT)
 * @return 1 if distance read successfully, 0 on error
 *
 * Call this when interrupt signals data ready for a specific sensor.
 * Updates proximity_devices[sensor_index][] with zone distances.
 */
uint8_t MX_TOF_ReadSensorDistance(uint8_t sensor_index) {
    if (sensor_index >= MAX_TOF_SENSOR) {
        return 0;
    }

    if (!ToF_Present[sensor_index]) {
        return 0;
    }

    if (sensors[sensor_index].status != sensor_active) {
        return 0;
    }

    uint8_t bsp_idx = g_tof_index_map.logical_to_bsp[sensor_index];
    if (bsp_idx == 0xFF) {
        return 0;
    }

    int32_t result_status = VL53L5A1_RANGING_SENSOR_GetDistance(bsp_idx, &Result);
    if (result_status == BSP_ERROR_NONE) {
        copy_distance_val(sensor_index, &Result);
        return 1;
    }

    return 0;
}

uint8_t MX_TOF_GetBspIndex(uint8_t sensor_index) {
    if (sensor_index >= MAX_TOF_SENSOR) {
        return 0xFF;
    }
    return g_tof_index_map.logical_to_bsp[sensor_index];
}

#ifdef __cplusplus
}
#endif
