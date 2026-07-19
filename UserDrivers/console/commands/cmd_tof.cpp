/**
 * MIT License
 *
 * Copyright (c) 2025 ElectronicsBuilder
 *
 * @file    cmd_tof.cpp
 * @brief   TOF sensor console commands (TOF_*)
 *
 * Commands:
 *   TOF_STATUS          - One-shot snapshot of all 6 sensor distances
 *   TOF_WATCH [sec]     - Live one-line distance display, updates in place
 *   TOF_LOG ON          - Start continuous distance logging (every detection cycle)
 *   TOF_LOG OFF         - Stop continuous distance logging
 *   TOF_LOG <ms>        - Log at most once every <ms> milliseconds (e.g. TOF_LOG 5)
 */

#include "console_internal.h"
#include "TofProximityManager.hpp"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// ============================================================================
// Command Handlers
// ============================================================================

static bool cmd_tof_status(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    (void)cmd_buffer;

    auto& mgr = TofProximityManager::getInstance();
    TofDistanceSnapshot snap = {};
    mgr.getSnapshot(&snap);

    static const char* sensor_names[6] = {
        "TOP   ", "BOTTOM", "FRONT ", "BACK  ", "LEFT  ", "RIGHT "
    };

    char msg[512];
    int offset = 0;

    offset += snprintf(msg + offset, sizeof(msg) - offset,
        "\r\n========================================\r\n"
        "TOF SENSOR SNAPSHOT  t=%lums\r\n"
        "========================================\r\n"
        "Active: %u/6\r\n"
        "----------------------------------------\r\n",
        snap.timestamp_ms,
        snap.active_sensor_count);

    for (int i = 0; i < 6; i++) {
        bool det = mgr.isObstacleDetected(static_cast<TofSensorId>(i));
        if (snap.sensor_valid[i]) {
            offset += snprintf(msg + offset, sizeof(msg) - offset,
                "  %s  %4u mm  %s\r\n",
                sensor_names[i],
                snap.distance_mm[i],
                det ? "[ OBSTACLE ]" : "");
        } else {
            offset += snprintf(msg + offset, sizeof(msg) - offset,
                "  %s  --- (not present)\r\n",
                sensor_names[i]);
        }
    }

    offset += snprintf(msg + offset, sizeof(msg) - offset,
        "========================================\r\n");

    console_send(huart, msg);
    return true;
}

static bool cmd_tof_watch(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* arg = console_get_arg(cmd_buffer, "TOF_WATCH");
    int seconds = arg ? atoi(arg) : 10;
    if (seconds <= 0) seconds = 10;
    if (seconds > 120) seconds = 120;

    auto& mgr = TofProximityManager::getInstance();

    console_printf(huart,
        "\r\n[TOF] Live distances (mm) for %ds, '!' = obstacle. Press Enter to stop.\r\n",
        seconds);

    static const char* names[6] = { "TOP", "BOT", "FWD", "BCK", "LFT", "RGT" };

    uint32_t end_ms = HAL_GetTick() + (uint32_t)seconds * 1000u;
    char line[128];
    bool aborted = false;

    while ((int32_t)(end_ms - HAL_GetTick()) > 0) {
        if (console_abort_requested()) {
            aborted = true;
            break;
        }
        TofDistanceSnapshot snap = {};
        mgr.getSnapshot(&snap);

        // Fixed-width fields so each \r redraw fully overwrites the previous line
        int off = snprintf(line, sizeof(line), "\r");
        for (int i = 0; i < 6; i++) {
            if (snap.sensor_valid[i]) {
                off += snprintf(line + off, sizeof(line) - off, "%s:%5u%c ",
                    names[i],
                    snap.distance_mm[i],
                    mgr.isObstacleDetected(static_cast<TofSensorId>(i)) ? '!' : ' ');
            } else {
                off += snprintf(line + off, sizeof(line) - off, "%s:  ---  ", names[i]);
            }
        }
        console_send(huart, line);

        osDelay(100);
    }

    console_send(huart, aborted ? "\r\n[TOF] Watch aborted\r\n"
                                : "\r\n[TOF] Watch done\r\n");
    return true;
}

static bool cmd_tof_log(const char* cmd_buffer, UART_HandleTypeDef* huart) {
    const char* arg = console_get_arg(cmd_buffer, "TOF_LOG");

    if (!arg) {
        console_send(huart,
            "\r\n[ERROR] Usage:\r\n"
            "  TOF_LOG ON       - log every detection cycle (~17ms/sensor at 60Hz)\r\n"
            "  TOF_LOG OFF      - stop logging\r\n"
            "  TOF_LOG <ms>     - log every <ms> milliseconds (e.g. TOF_LOG 5)\r\n");
        return true;
    }

    auto& mgr = TofProximityManager::getInstance();

    if (strncasecmp(arg, "ON", 2) == 0 && (arg[2] == '\0' || arg[2] == ' ')) {
        mgr.setLoggingEnabled(true, 0);
        console_send(huart, "\r\n[TOF] Continuous logging ON (max rate)\r\n");
    } else if (strncasecmp(arg, "OFF", 3) == 0) {
        mgr.setLoggingEnabled(false, 0);
        console_send(huart, "\r\n[TOF] Logging OFF\r\n");
    } else {
        int ms = atoi(arg);
        if (ms <= 0) {
            console_send(huart, "\r\n[ERROR] Invalid argument. Use ON, OFF, or a positive integer (ms).\r\n");
            return true;
        }
        mgr.setLoggingEnabled(true, (uint32_t)ms);
        char reply[80];
        snprintf(reply, sizeof(reply), "\r\n[TOF] Logging ON (every %d ms)\r\n", ms);
        console_send(huart, reply);
    }

    return true;
}

// ============================================================================
// Command Registration — extern "C" so console.c (C linkage) can find the symbols
// ============================================================================


const console_command_t tof_commands[] = {
    {
        .command  = "TOF_STATUS",
        .handler  = cmd_tof_status,
        .help_text = "One-shot snapshot of all 6 TOF sensor distances"
    },
    {
        .command  = "TOF_WATCH",
        .handler  = cmd_tof_watch,
        .help_text = "TOF_WATCH [sec]  — live one-line distance display (updates in place)"
    },
    {
        .command  = "TOF_LOG",
        .handler  = cmd_tof_log,
        .help_text = "TOF_LOG ON|OFF|<ms>  — continuous distance logging"
    }
};

const size_t tof_commands_count = sizeof(tof_commands) / sizeof(tof_commands[0]);

