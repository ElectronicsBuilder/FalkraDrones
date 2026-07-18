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
 * @file    driver_health.cpp
 * @brief   Driver health monitoring and diagnostics implementation
 */
#include "driver_health.hpp"
#include "driver_manager.hpp"
#include "log.hpp"
#include <cstring>

// Global health tracking array
static DriverHealth health_registry[static_cast<size_t>(DriverId::COUNT)] = {};

// === Health Queries ===

const DriverHealth& DriverHealthMonitor::getHealth(DriverId id) {
    if (id >= DriverId::COUNT) {
        static const DriverHealth null_health = {};
        return null_health;
    }
    return health_registry[static_cast<size_t>(id)];
}

uint8_t DriverHealthMonitor::getSystemHealthScore(void) {
    uint32_t total_score = 0;
    uint8_t critical_count = 0;

    // Calculate weighted average of critical drivers
    for (uint8_t i = 0; i < getDriverCount(); i++) {
        auto id = static_cast<DriverId>(i);
        const auto* meta = getDriverMetadata(id);

        if (!meta || !meta->required) {
            continue;  // Skip optional drivers
        }

        critical_count++;
        total_score += health_registry[i].health_score;
    }

    if (critical_count == 0) {
        return 100;  // No critical drivers
    }

    return static_cast<uint8_t>(total_score / critical_count);
}

// === Error/Warning Reporting ===

void DriverHealthMonitor::reportError(DriverId id, const char* error_msg) {
    if (id >= DriverId::COUNT) {
        return;
    }

    auto& health = health_registry[static_cast<size_t>(id)];
    health.error_count++;

    // Update health score: each error reduces by ~2 points (max 50 errors for 100 points)
    if (health.health_score > 0) {
        health.health_score--;
    }

    // Store error message (max 64 chars)
    if (error_msg) {
        std::strncpy(health.last_error, error_msg, sizeof(health.last_error) - 1);
        health.last_error[sizeof(health.last_error) - 1] = '\0';
    }

    LOG_ERROR("[HEALTH] %s: %s (error_count=%lu)",
              getDriverMetadata(id)->name, error_msg, health.error_count);
}

void DriverHealthMonitor::reportWarning(DriverId id, const char* warning_msg) {
    if (id >= DriverId::COUNT) {
        return;
    }

    auto& health = health_registry[static_cast<size_t>(id)];
    health.warning_count++;

    LOG_WARN("[HEALTH] %s: %s (warning_count=%lu)",
             getDriverMetadata(id)->name, warning_msg, health.warning_count);
}

// === System Health Checks ===

bool DriverHealthMonitor::isSystemHealthy(void) {
    auto& dm = DriverManager::getInstance();

    // Check if all critical drivers are READY
    for (uint8_t i = 0; i < getDriverCount(); i++) {
        auto id = static_cast<DriverId>(i);
        const auto* meta = getDriverMetadata(id);

        if (!meta || !meta->required) {
            continue;  // Skip optional drivers
        }

        auto state = dm.getDriverState(id);
        if (state != DriverState::READY) {
            LOG_WARN("[HEALTH] Critical driver %s not ready: state=%d",
                    meta->name, static_cast<int>(state));
            return false;
        }
    }

    // Check health scores (>50% is acceptable)
    if (getSystemHealthScore() < 50) {
        LOG_WARN("[HEALTH] System health score low: %u%%", getSystemHealthScore());
        return false;
    }

    return true;
}

// === Diagnostics ===

void DriverHealthMonitor::printHealthReport(void) {
    auto& dm = DriverManager::getInstance();

    LOG_INFO("========== DRIVER HEALTH REPORT ==========");
    LOG_INFO("System Health: %u%%", getSystemHealthScore());
    LOG_INFO("==========================================");

    for (uint8_t i = 0; i < getDriverCount(); i++) {
        auto id = static_cast<DriverId>(i);
        const auto* meta = getDriverMetadata(id);
        const auto& health = health_registry[i];
        auto state = dm.getDriverState(id);

        const char* state_str = "UNKNOWN";
        switch (state) {
            case DriverState::UNINITIALIZED: state_str = "UNINIT"; break;
            case DriverState::INITIALIZING: state_str = "INIT"; break;
            case DriverState::READY: state_str = "READY"; break;
            case DriverState::ERROR: state_str = "ERROR"; break;
            case DriverState::DEGRADED: state_str = "DEGRADED"; break;
            case DriverState::DISABLED: state_str = "DISABLED"; break;
        }

        LOG_INFO("[%2d] %-25s [%-7s] Health:%3u%% Err:%3lu Warn:%3lu",
                i, meta->name, state_str, health.health_score,
                health.error_count, health.warning_count);

        if (health.error_count > 0 && health.last_error[0] != '\0') {
            LOG_INFO("     Last Error: %s", health.last_error);
        }
    }

    LOG_INFO("==========================================");
}
