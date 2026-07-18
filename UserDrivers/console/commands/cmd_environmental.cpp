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
 * @file    cmd_environmental.cpp
 * @brief   Environmental sensor console commands (ENV_*)
 *          Temperature, humidity, pressure, and altitude readings
 */

#include "console_internal.h"
#include "driver_status.hpp"
#include "status.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

// ============================================================================
// Command Handlers
// ============================================================================

static bool cmd_env_status(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Get latest environmental data
    auto env_data = DriverStatus::getEnvironmentalData();

    char msg[512];
    int offset = 0;
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "ENVIRONMENTAL SENSORS STATUS\r\n"
            "========================================\r\n");

    // Get full status snapshot to check sensor health
    auto status = DriverStatus::getSnapshot();

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "Temperature (SHT4x):      %s\r\n"
            "  Value: %.2f°C\r\n",
            status.sht4xOk ? "OK" : "FAILED",
            env_data.temperatureC);

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "Humidity (SHT4x):         %s\r\n"
            "  Value: %.2f%%\r\n",
            status.sht4xOk ? "OK" : "FAILED",
            env_data.humidityPct);

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "Pressure (BMP581):        %s\r\n"
            "  Value: %.2f Pa\r\n",
            status.bmp581Ok ? "OK" : "FAILED",
            env_data.pressurePa);

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "Altitude (BMP581):        %s\r\n"
            "  Value: %.2f m (above sea level)\r\n",
            status.bmp581Ok ? "OK" : "FAILED",
            env_data.altitudeM);

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "----------------------------------------\r\n"
            "Last Update: %lu ms\r\n"
            "========================================\r\n",
            env_data.timestamp_ms);

    console_send(huart, msg);
    return true;
}

static bool cmd_env_temp_humidity(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Get latest environmental data (temperature and humidity only)
    auto env_data = DriverStatus::getEnvironmentalData();
    auto status = DriverStatus::getSnapshot();

    char msg[256];
    int offset = 0;
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "TEMPERATURE & HUMIDITY (SHT4x)\r\n"
            "========================================\r\n"
            "Status: %s\r\n",
            status.sht4xOk ? "OK Passed" : "FAILED Failed");

    if (status.sht4xOk) {
        offset += snprintf(msg + offset, sizeof(msg) - offset,
                "Temperature: %.2f°C\r\n"
                "Humidity:    %.2f%%\r\n"
                "Dew Point:   %.2f°C (estimated)\r\n",
                env_data.temperatureC,
                env_data.humidityPct,
                env_data.temperatureC - ((100.0f - env_data.humidityPct) / 5.0f));
    } else {
        offset += snprintf(msg + offset, sizeof(msg) - offset,
                "Temperature: N/A\r\n"
                "Humidity:    N/A\r\n");
    }

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "========================================\r\n");

    console_send(huart, msg);
    return true;
}

static bool cmd_env_pressure_altitude(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Get latest environmental data (pressure and altitude only)
    auto env_data = DriverStatus::getEnvironmentalData();
    auto status = DriverStatus::getSnapshot();

    char msg[256];
    int offset = 0;
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "PRESSURE & ALTITUDE (BMP581)\r\n"
            "========================================\r\n"
            "Status: %s\r\n",
            status.bmp581Ok ? "OK Passed" : "FAILED Failed");

    if (status.bmp581Ok) {
        offset += snprintf(msg + offset, sizeof(msg) - offset,
                "Pressure:    %.2f Pa (%.4f hPa)\r\n"
                "Altitude:    %.2f meters\r\n"
                "QNH (Sea):   %.2f hPa (standard: 1013.25)\r\n",
                env_data.pressurePa,
                env_data.pressurePa / 100.0f,
                env_data.altitudeM,
                (env_data.pressurePa * powf((44330.77f / (44330.77f - env_data.altitudeM)), 5.255f)) / 100.0f);
    } else {
        offset += snprintf(msg + offset, sizeof(msg) - offset,
                "Pressure:    N/A\r\n"
                "Altitude:    N/A\r\n");
    }

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "========================================\r\n");

    console_send(huart, msg);
    return true;
}

static bool cmd_env_all(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    // Display all environmental data in compact format
    auto env_data = DriverStatus::getEnvironmentalData();
    auto status = DriverStatus::getSnapshot();

    char msg[512];
    int offset = 0;
    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "\r\n========================================\r\n"
            "ALL ENVIRONMENTAL SENSORS\r\n"
            "========================================\r\n");

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "Temperature: %s %.2f°C\r\n",
            status.sht4xOk ? "Passed" : "Failed",
            env_data.temperatureC);

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "Humidity:    %s %.2f%%\r\n",
            status.sht4xOk ? "Passed" : "Failed",
            env_data.humidityPct);

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "Pressure:    %s %.2f Pa\r\n",
            status.bmp581Ok ? "Passed" : "Failed",
            env_data.pressurePa);

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "Altitude:    %s %.2f m\r\n",
            status.bmp581Ok ? "Passed" : "Failed",
            env_data.altitudeM);

    offset += snprintf(msg + offset, sizeof(msg) - offset,
            "========================================\r\n");

    console_send(huart, msg);
    return true;
}

// ============================================================================
// Command Registration
// ============================================================================

const console_command_t environmental_commands[] = {
    {
        .command = "ENV_STATUS",
        .handler = cmd_env_status,
        .help_text = "Show all environmental sensor status and readings"
    },
    {
        .command = "ENV_TEMP_HUMIDITY",
        .handler = cmd_env_temp_humidity,
        .help_text = "Show temperature and humidity from SHT4x sensor"
    },
    {
        .command = "ENV_PRESSURE_ALTITUDE",
        .handler = cmd_env_pressure_altitude,
        .help_text = "Show pressure and altitude from BMP581 sensor"
    },
    {
        .command = "ENV_ALL",
        .handler = cmd_env_all,
        .help_text = "Show all environmental data in compact format"
    }
};

const size_t environmental_commands_count = sizeof(environmental_commands) / sizeof(environmental_commands[0]);
