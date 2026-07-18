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
 * @file    user_config.h
 * @brief   User Configuration Manager - Pure C Implementation
 * @details NVRAM-backed persistent configuration with CRC32 protection.
 *          Pure C implementation for compatibility with C and C++ codebases.
 *
 * Usage Example:
 * @code
 * // Initialize NVRAM interface (see nvram_wrapper.h)
 * nvram_interface_t* nvram_if = nvram_interface_create(&nvram_cpp_instance);
 *
 * // Create and initialize UserConfig
 * userconfig_t* config = userconfig_create(nvram_if);
 * userconfig_init(config);
 *
 * // Set WiFi credentials
 * userconfig_set_wifi_ssid(config, "MyNetwork");
 * userconfig_set_wifi_password(config, "SecurePass123");
 * userconfig_save_wifi(config);
 *
 * // Read credentials
 * const char* ssid = userconfig_get_wifi_ssid(config);
 * if (userconfig_has_wifi_credentials(config)) {
 *     // Use NVRAM credentials
 * }
 * @endcode
 */

#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// NVRAM Address Layout
// ============================================================================

#define USERCONFIG_BASE_ADDR                0x10000     // Start of system configuration region

// Configuration Block Addresses
#define USERCONFIG_WIFI_BLOCK_ADDR          (USERCONFIG_BASE_ADDR + 0x0000)    // 0x10000
#define USERCONFIG_NETWORK_BLOCK_ADDR       (USERCONFIG_BASE_ADDR + 0x0100)    // 0x10100
#define USERCONFIG_SYSTEM_BLOCK_ADDR        (USERCONFIG_BASE_ADDR + 0x0200)    // 0x10200
#define USERCONFIG_BATMON_BLOCK_ADDR        (USERCONFIG_BASE_ADDR + 0x0300)    // 0x10300

// Block Sizes
#define USERCONFIG_WIFI_BLOCK_SIZE          256
#define USERCONFIG_NETWORK_BLOCK_SIZE       256
#define USERCONFIG_SYSTEM_BLOCK_SIZE        256
#define USERCONFIG_BATMON_BLOCK_SIZE        64          // Battery Monitor calibration

// Magic Numbers (for block validation)
#define USERCONFIG_WIFI_MAGIC               0x57494649  // "WIFI" in ASCII
#define USERCONFIG_NETWORK_MAGIC            0x4E455457  // "NETW" in ASCII
#define USERCONFIG_SYSTEM_MAGIC             0x53595354  // "SYST" in ASCII
#define USERCONFIG_BATMON_MAGIC             0x42415443  // "BATC" in ASCII

// Version Numbers (allows format evolution)
#define USERCONFIG_WIFI_VERSION             2  // v2: Password encryption with AES-128-CBC
#define USERCONFIG_NETWORK_VERSION          1
#define USERCONFIG_SYSTEM_VERSION           1
#define USERCONFIG_BATMON_VERSION           1  // v1: ACS758 calibration data

// WiFi Configuration Limits
#define USERCONFIG_WIFI_MAX_SSID_LEN        32
#define USERCONFIG_WIFI_MAX_PASSWORD_LEN    64

// Encryption Configuration
#define USERCONFIG_AES_KEY_SIZE             16  // AES-128 key size in bytes
#define USERCONFIG_AES_IV_SIZE              16  // AES IV size in bytes (128 bits)
#define USERCONFIG_AES_BLOCK_SIZE           16  // AES block size in bytes
// Password + null terminator (65) padded to AES block size = 80 bytes (5 blocks)
#define USERCONFIG_ENCRYPTED_PW_SIZE        80  // Must be multiple of 16

// Network Configuration Limits
#define USERCONFIG_IP_ADDR_LEN              16  // "255.255.255.255\0"
#define USERCONFIG_HOSTNAME_LEN             32

// ============================================================================
// Configuration Block Structures
// ============================================================================

/**
 * @brief WiFi Configuration Block Structure (Version 2 - Encrypted)
 * Stored at NVRAM address 0x10000
 *
 * Structure Layout:
 * - Version 1: Plaintext password (legacy, auto-migrated on first save)
 * - Version 2: AES-128-CBC encrypted password with device-unique key
 *
 * Security Notes:
 * - SSID stored in plaintext (not sensitive data)
 * - Password encrypted with AES-128-CBC using device UID-derived key
 * - Each device has unique encryption key (cannot transfer passwords)
 * - IV is deterministic but device-unique (acceptable for config data)
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;                                         // 0x57494649 "WIFI"
    uint32_t version;                                       // Version 2 (encrypted)
    char ssid[USERCONFIG_WIFI_MAX_SSID_LEN + 1];           // WiFi SSID (plaintext)
    uint8_t encrypted_password[USERCONFIG_ENCRYPTED_PW_SIZE];  // AES-128-CBC encrypted password
    uint8_t iv[USERCONFIG_AES_IV_SIZE];                    // Initialization vector
    bool auto_connect;                                      // Auto-connect flag
    uint8_t reserved[2];                                    // Reserved for alignment
    uint32_t crc32;                                         // CRC32 of entire block
} userconfig_wifi_t;  // Total: 136 bytes

/**
 * @brief Network Configuration Block Structure
 * Stored at NVRAM address 0x10100
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;                                 // Magic number
    uint32_t version;                               // Version number
    bool use_dhcp;                                  // Use DHCP
    char static_ip[USERCONFIG_IP_ADDR_LEN];        // Static IP address
    char gateway[USERCONFIG_IP_ADDR_LEN];          // Gateway address
    char subnet_mask[USERCONFIG_IP_ADDR_LEN];      // Subnet mask
    char dns_server[USERCONFIG_IP_ADDR_LEN];       // DNS server address
    char hostname[USERCONFIG_HOSTNAME_LEN];        // Device hostname
    uint8_t reserved[8];                            // Reserved
    uint32_t crc32;                                 // CRC32 checksum
} userconfig_network_t;

/**
 * @brief System Preferences Block Structure
 * Stored at NVRAM address 0x10200
 *
 * PWM Voltage Configuration:
 * - VPWM is the output voltage for ESC PWM signals (PWM[1..8])
 * - Controlled by TPS2115 power multiplexer (D1 pin)
 * - Level shifted by TXS0108 when enabled (VPWM_OE)
 * - User configurable for ESC compatibility (3.3V or 5V)
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;                     // Magic number
    uint32_t version;                   // Version number
    bool telemetry_enabled;             // Enable telemetry streaming
    uint32_t telemetry_rate_hz;         // Telemetry rate (Hz)
    bool logging_enabled;               // Enable flight data logging
    uint32_t logging_rate_hz;           // Logging rate (Hz)
    bool audio_enabled;                 // Enable audio feedback
    uint8_t led_brightness;             // LED brightness (0-100%)
    uint8_t pwm_voltage;                // PWM output voltage: 0=5V, 1=3.3V (default 3.3V)
    uint8_t reserved[31];               // Reserved
    uint32_t crc32;                     // CRC32 checksum
} userconfig_system_t;

/**
 * @brief Battery Monitor Calibration Block Structure
 * Stored at NVRAM address 0x10300
 *
 * Calibration data for ACS758-50A Hall Effect current sensor (WCMCU-758).
 * Must be calibrated with NO current flowing through sensor.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;                     // 0x42415443 "BATC"
    uint32_t version;                   // Version 1
    float vref;                         // ADC reference voltage (typically 3.3V)
    float zero_current_raw;             // Voltage at 0A (raw channel, VOUT1)
    float zero_current_buffered;        // Voltage at 0A (buffered channel, VOUT2)
    float sensitivity_raw;              // Sensitivity coefficient for raw channel (V/A)
    float sensitivity_buffered;         // Sensitivity coefficient for buffered channel (V/A)
    uint8_t invert_polarity;            // 1 = invert sign (sensor on GND/return path), 0 = normal (sensor on VCC+ path)
    uint8_t raw_channel_enabled;        // 1 = raw channel enabled in current calculation, 0 = disabled
    uint8_t buffered_channel_enabled;   // 1 = buffered channel enabled in current calculation, 0 = disabled
    uint8_t reserved[5];                // Reserved for future expansion
    uint32_t crc32;                     // CRC32 checksum
} userconfig_batmon_t;  // Total: 44 bytes

// ============================================================================
// NVRAM Interface (Function Pointer Abstraction)
// ============================================================================

/**
 * @brief NVRAM interface function pointers
 * Abstracts NVRAM hardware access for pure C compatibility
 */
typedef struct {
    void (*write_array)(uint32_t address, uint8_t* data, uint16_t size);
    void (*read_array)(uint32_t address, uint8_t* buffer, uint16_t size);
    uint8_t (*calculate_crc8)(uint8_t* buffer, uint32_t length);
} nvram_interface_t;

// ============================================================================
// UserConfig Handle (Opaque Type)
// ============================================================================

/**
 * @brief Opaque UserConfig handle
 * Internal structure hidden from users (implementation detail)
 */
typedef struct userconfig_handle userconfig_t;

// ============================================================================
// UserConfig API - Initialization & Lifecycle
// ============================================================================

/**
 * @brief Create UserConfig instance
 * @param nvram_if Pointer to NVRAM interface
 * @return UserConfig handle, or NULL on failure
 */
userconfig_t* userconfig_create(nvram_interface_t* nvram_if);

/**
 * @brief Destroy UserConfig instance and free resources
 * @param config UserConfig handle
 */
void userconfig_destroy(userconfig_t* config);

/**
 * @brief Initialize configuration system
 * @details Loads all configuration blocks from NVRAM.
 *          If any block is invalid/corrupted, initializes with defaults.
 * @param config UserConfig handle
 * @return true if successful, false if NVRAM access failed
 */
bool userconfig_init(userconfig_t* config);

// ============================================================================
// WiFi Configuration API
// ============================================================================

/**
 * @brief Check if WiFi credentials are stored in NVRAM
 * @param config UserConfig handle
 * @return true if valid credentials exist in NVRAM
 */
bool userconfig_has_wifi_credentials(userconfig_t* config);

/**
 * @brief Get WiFi SSID from configuration
 * @param config UserConfig handle
 * @return Pointer to SSID string (null-terminated), or NULL if config is NULL
 */
const char* userconfig_get_wifi_ssid(userconfig_t* config);

/**
 * @brief Get WiFi password from configuration
 * @param config UserConfig handle
 * @return Pointer to password string (null-terminated), or NULL if config is NULL
 */
const char* userconfig_get_wifi_password(userconfig_t* config);

/**
 * @brief Get WiFi auto-connect setting
 * @param config UserConfig handle
 * @return true if auto-connect enabled
 */
bool userconfig_get_wifi_autoconnect(userconfig_t* config);

/**
 * @brief Set WiFi SSID
 * @param config UserConfig handle
 * @param ssid SSID string (max 32 characters)
 * @return true if valid and set, false if invalid length or NULL config
 */
bool userconfig_set_wifi_ssid(userconfig_t* config, const char* ssid);

/**
 * @brief Set WiFi password
 * @param config UserConfig handle
 * @param password Password string (max 64 characters)
 * @return true if valid and set, false if invalid length or NULL config
 */
bool userconfig_set_wifi_password(userconfig_t* config, const char* password);

/**
 * @brief Set WiFi auto-connect
 * @param config UserConfig handle
 * @param enable true to enable auto-connect on boot
 */
void userconfig_set_wifi_autoconnect(userconfig_t* config, bool enable);

/**
 * @brief Save WiFi configuration to NVRAM
 * @param config UserConfig handle
 * @return true if successfully saved, false if write failed
 */
bool userconfig_save_wifi(userconfig_t* config);

/**
 * @brief Load WiFi configuration from NVRAM
 * @param config UserConfig handle
 * @return true if valid config loaded, false if invalid/corrupted
 */
bool userconfig_load_wifi(userconfig_t* config);

/**
 * @brief Reset WiFi configuration to defaults
 * @param config UserConfig handle
 */
void userconfig_reset_wifi(userconfig_t* config);

// ============================================================================
// Network Configuration API
// ============================================================================

bool userconfig_get_use_dhcp(userconfig_t* config);
const char* userconfig_get_static_ip(userconfig_t* config);
const char* userconfig_get_gateway(userconfig_t* config);
const char* userconfig_get_subnet_mask(userconfig_t* config);
const char* userconfig_get_dns_server(userconfig_t* config);
const char* userconfig_get_hostname(userconfig_t* config);

void userconfig_set_use_dhcp(userconfig_t* config, bool enable);
bool userconfig_set_static_ip(userconfig_t* config, const char* ip);
bool userconfig_set_gateway(userconfig_t* config, const char* gateway);
bool userconfig_set_subnet_mask(userconfig_t* config, const char* mask);
bool userconfig_set_dns_server(userconfig_t* config, const char* dns);
bool userconfig_set_hostname(userconfig_t* config, const char* hostname);

bool userconfig_save_network(userconfig_t* config);
bool userconfig_load_network(userconfig_t* config);
void userconfig_reset_network(userconfig_t* config);

// ============================================================================
// System Preferences API
// ============================================================================

bool userconfig_get_telemetry_enabled(userconfig_t* config);
uint32_t userconfig_get_telemetry_rate(userconfig_t* config);
bool userconfig_get_logging_enabled(userconfig_t* config);
uint32_t userconfig_get_logging_rate(userconfig_t* config);
bool userconfig_get_audio_enabled(userconfig_t* config);
uint8_t userconfig_get_led_brightness(userconfig_t* config);

/**
 * @brief Get PWM output voltage setting
 * @param config UserConfig handle
 * @return 0 = 5V, 1 = 3.3V (default)
 */
uint8_t userconfig_get_pwm_voltage(userconfig_t* config);

void userconfig_set_telemetry_enabled(userconfig_t* config, bool enable);
void userconfig_set_telemetry_rate(userconfig_t* config, uint32_t rate_hz);
void userconfig_set_logging_enabled(userconfig_t* config, bool enable);
void userconfig_set_logging_rate(userconfig_t* config, uint32_t rate_hz);
void userconfig_set_audio_enabled(userconfig_t* config, bool enable);
void userconfig_set_led_brightness(userconfig_t* config, uint8_t brightness);

/**
 * @brief Set PWM output voltage
 * @param config UserConfig handle
 * @param voltage 0 = 5V, 1 = 3.3V (default)
 */
void userconfig_set_pwm_voltage(userconfig_t* config, uint8_t voltage);

bool userconfig_save_system(userconfig_t* config);
bool userconfig_load_system(userconfig_t* config);
void userconfig_reset_system(userconfig_t* config);

// ============================================================================
// Battery Monitor Calibration API
// ============================================================================

/**
 * @brief Check if battery monitor calibration exists in NVRAM
 * @param config UserConfig handle
 * @return true if valid calibration exists
 */
bool userconfig_has_batmon_calibration(userconfig_t* config);

/**
 * @brief Get battery monitor calibration block (read-only)
 * @param config UserConfig handle
 * @return Pointer to calibration structure, or NULL if config is NULL or invalid
 */
const userconfig_batmon_t* userconfig_get_batmon_block(userconfig_t* config);

/**
 * @brief Set battery monitor calibration data
 * @param config UserConfig handle
 * @param vref ADC reference voltage (typically 3.3V)
 * @param zero_raw Zero current voltage for raw channel (V)
 * @param zero_buffered Zero current voltage for buffered channel (V)
 * @param sens_raw Sensitivity for raw channel (V/A, typically 0.040)
 * @param sens_buffered Sensitivity for buffered channel (V/A, typically 0.040)
 * @param invert_polarity 1 = sensor on GND/return path (invert sign), 0 = sensor on VCC+ path (normal)
 * @return true if valid parameters
 */
bool userconfig_set_batmon_calibration(userconfig_t* config, float vref,
                                        float zero_raw, float zero_buffered,
                                        float sens_raw, float sens_buffered,
                                        uint8_t invert_polarity);

/**
 * @brief Save battery monitor calibration to NVRAM
 * @param config UserConfig handle
 * @return true if successfully saved
 */
bool userconfig_save_batmon(userconfig_t* config);

/**
 * @brief Load battery monitor calibration from NVRAM
 * @param config UserConfig handle
 * @return true if valid calibration loaded
 */
bool userconfig_load_batmon(userconfig_t* config);

/**
 * @brief Reset battery monitor calibration to defaults (uncalibrated state)
 * @param config UserConfig handle
 */
void userconfig_reset_batmon(userconfig_t* config);

/**
 * @brief Set battery monitor channel enable flags and save to NVRAM
 * @param config UserConfig handle
 * @param raw_enabled 1 = raw channel enabled, 0 = disabled
 * @param buffered_enabled 1 = buffered channel enabled, 0 = disabled
 * @return true if successfully saved to NVRAM
 */
bool userconfig_set_batmon_channels(userconfig_t* config, uint8_t raw_enabled, uint8_t buffered_enabled);

// ============================================================================
// Diagnostic & Utility API
// ============================================================================

/**
 * @brief Get WiFi configuration block (read-only)
 * @param config UserConfig handle
 * @return Pointer to WiFi configuration structure, or NULL if config is NULL
 */
const userconfig_wifi_t* userconfig_get_wifi_block(userconfig_t* config);

/**
 * @brief Get network configuration block (read-only)
 * @param config UserConfig handle
 * @return Pointer to network configuration structure, or NULL if config is NULL
 */
const userconfig_network_t* userconfig_get_network_block(userconfig_t* config);

/**
 * @brief Get system configuration block (read-only)
 * @param config UserConfig handle
 * @return Pointer to system configuration structure, or NULL if config is NULL
 */
const userconfig_system_t* userconfig_get_system_block(userconfig_t* config);

/**
 * @brief Validate all configuration blocks
 * @param config UserConfig handle
 * @return true if all blocks are valid
 */
bool userconfig_validate_all(userconfig_t* config);

/**
 * @brief Erase all user configuration from NVRAM
 * @param config UserConfig handle
 * @return true if successfully erased
 */
bool userconfig_erase_all(userconfig_t* config);

#ifdef __cplusplus
}
#endif

#endif // USER_CONFIG_H
