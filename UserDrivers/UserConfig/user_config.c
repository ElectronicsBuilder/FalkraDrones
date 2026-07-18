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
 * @file    user_config.c
 * @brief   User Configuration Manager - Pure C Implementation
 */

#include "user_config.h"
#include "tiny-aes/aes.h"
#include <string.h>
#include <stdlib.h>

// STM32F767 Unique ID register address (96-bit UID)
#define STM32_UID_BASE_ADDR     0x1FF0F420

// ============================================================================
// CRC32 Implementation (Ethernet polynomial 0x04C11DB7)
// ============================================================================

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

static uint32_t calculate_crc32(const uint8_t* data, uint32_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return ~crc;
}

// ============================================================================
// AES Encryption Helper Functions
// ============================================================================

/**
 * @brief Read STM32 Unique Device ID (96-bit)
 * @param uid_out Buffer to store 12-byte UID
 */
static void get_device_uid(uint8_t* uid_out) {
    uint32_t* uid_base = (uint32_t*)STM32_UID_BASE_ADDR;
    memcpy(uid_out, uid_base, 12);  // Copy 96 bits (12 bytes)
}

/**
 * @brief Derive AES-128 key from device UID
 * @details Uses CRC32-based key stretching for device-unique key
 * @param key_out Buffer to store 16-byte AES key
 */
static void derive_aes_key(uint8_t* key_out) {
    uint8_t uid[12];
    get_device_uid(uid);

    // Application-specific salt
    const char* salt = "FalkraDrones_WiFi_Key_v1";

    // Create input buffer: UID + Salt
    uint8_t input[64];
    memcpy(input, uid, 12);
    memcpy(input + 12, salt, strlen(salt));

    // Generate 16-byte key using iterative CRC32 (key stretching)
    for (int i = 0; i < 16; i += 4) {
        // Mix byte position into the calculation for uniqueness
        input[12 + strlen(salt)] = (uint8_t)i;
        uint32_t crc = calculate_crc32(input, 12 + strlen(salt) + 1);
        memcpy(key_out + i, &crc, 4);
    }
}

/**
 * @brief Derive AES IV from device UID
 * @details Uses different salt from key for IV uniqueness
 * @param iv_out Buffer to store 16-byte IV
 */
static void derive_aes_iv(uint8_t* iv_out) {
    uint8_t uid[12];
    get_device_uid(uid);

    // Different salt for IV (never reuse key material as IV)
    const char* salt = "FalkraDrones_WiFi_IV_v1";

    // Create input buffer
    uint8_t input[64];
    memcpy(input, uid, 12);
    memcpy(input + 12, salt, strlen(salt));

    // Generate 16-byte IV using iterative CRC32
    for (int i = 0; i < 16; i += 4) {
        input[12 + strlen(salt)] = (uint8_t)i;
        uint32_t crc = calculate_crc32(input, 12 + strlen(salt) + 1);
        memcpy(iv_out + i, &crc, 4);
    }
}

/**
 * @brief Apply PKCS#7 padding to data
 * @param data Input data
 * @param data_len Length of input data
 * @param padded_out Output buffer for padded data (must be multiple of 16)
 * @return Length of padded data
 */
static size_t pkcs7_pad(const uint8_t* data, size_t data_len, uint8_t* padded_out) {
    // Calculate padding needed to reach multiple of AES_BLOCKLEN (16)
    size_t padding_len = USERCONFIG_AES_BLOCK_SIZE - (data_len % USERCONFIG_AES_BLOCK_SIZE);
    size_t total_len = data_len + padding_len;

    // Copy original data
    memcpy(padded_out, data, data_len);

    // Add PKCS#7 padding (each padding byte = number of padding bytes)
    for (size_t i = 0; i < padding_len; i++) {
        padded_out[data_len + i] = (uint8_t)padding_len;
    }

    return total_len;
}

/**
 * @brief Remove PKCS#7 padding from data
 * @param padded_data Padded data
 * @param padded_len Length of padded data
 * @param data_out Output buffer for unpadded data
 * @return Length of unpadded data, or 0 if invalid padding
 */
static size_t pkcs7_unpad(const uint8_t* padded_data, size_t padded_len, uint8_t* data_out) {
    if (padded_len == 0 || padded_len % USERCONFIG_AES_BLOCK_SIZE != 0) {
        return 0;  // Invalid padding
    }

    // Read padding length from last byte
    uint8_t padding_len = padded_data[padded_len - 1];

    // Validate padding (must be 1-16)
    if (padding_len == 0 || padding_len > USERCONFIG_AES_BLOCK_SIZE) {
        return 0;  // Invalid padding
    }

    // Validate all padding bytes are correct
    for (size_t i = 0; i < padding_len; i++) {
        if (padded_data[padded_len - 1 - i] != padding_len) {
            return 0;  // Invalid padding
        }
    }

    // Copy unpadded data
    size_t data_len = padded_len - padding_len;
    memcpy(data_out, padded_data, data_len);

    return data_len;
}

/**
 * @brief Encrypt WiFi password using AES-128-CBC
 * @param plaintext Plaintext password (null-terminated string)
 * @param ciphertext_out Output buffer for encrypted password (80 bytes)
 * @param iv_out Output buffer for IV used (16 bytes)
 * @return true if successful
 */
static bool encrypt_password(const char* plaintext, uint8_t* ciphertext_out, uint8_t* iv_out) {
    if (!plaintext || !ciphertext_out || !iv_out) {
        return false;
    }

    // Derive device-unique key and IV
    uint8_t key[USERCONFIG_AES_KEY_SIZE];
    derive_aes_key(key);
    derive_aes_iv(iv_out);

    // Apply PKCS#7 padding to plaintext
    uint8_t padded[USERCONFIG_ENCRYPTED_PW_SIZE];
    memset(padded, 0, sizeof(padded));  // Zero entire buffer first

    size_t plaintext_len = strlen(plaintext) + 1;  // Include null terminator
    size_t padded_len = pkcs7_pad((const uint8_t*)plaintext, plaintext_len, padded);

    // Validate padded length is a multiple of 16 and fits in buffer
    if (padded_len == 0 || padded_len > USERCONFIG_ENCRYPTED_PW_SIZE ||
        padded_len % USERCONFIG_AES_BLOCK_SIZE != 0) {
        // Padding error - clear sensitive data before returning
        memset(key, 0, sizeof(key));
        memset(padded, 0, sizeof(padded));
        return false;
    }

    // Initialize AES context
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv_out);

    // Encrypt the entire fixed-size buffer (always 80 bytes)
    // This hides the password length and simplifies decryption
    memcpy(ciphertext_out, padded, USERCONFIG_ENCRYPTED_PW_SIZE);
    AES_CBC_encrypt_buffer(&ctx, ciphertext_out, USERCONFIG_ENCRYPTED_PW_SIZE);

    // Clear sensitive data from stack
    memset(key, 0, sizeof(key));
    memset(padded, 0, sizeof(padded));
    memset(&ctx, 0, sizeof(ctx));

    return true;
}

/**
 * @brief Decrypt WiFi password using AES-128-CBC
 * @param ciphertext Encrypted password (80 bytes)
 * @param iv Initialization vector used for encryption (16 bytes)
 * @param plaintext_out Output buffer for decrypted password (65 bytes min)
 * @return true if successful
 */
static bool decrypt_password(const uint8_t* ciphertext, const uint8_t* iv, char* plaintext_out) {
    if (!ciphertext || !iv || !plaintext_out) {
        return false;
    }

    // Derive device-unique key
    uint8_t key[USERCONFIG_AES_KEY_SIZE];
    derive_aes_key(key);

    // Initialize AES context
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);

    // Decrypt entire fixed-size buffer (always 80 bytes)
    uint8_t decrypted[USERCONFIG_ENCRYPTED_PW_SIZE];
    memcpy(decrypted, ciphertext, USERCONFIG_ENCRYPTED_PW_SIZE);
    AES_CBC_decrypt_buffer(&ctx, decrypted, USERCONFIG_ENCRYPTED_PW_SIZE);

    // Find the first null terminator (password was null-terminated before padding)
    // The decrypted buffer contains: [password\0][PKCS#7 padding][zeros...]
    size_t password_len = 0;
    for (size_t i = 0; i < USERCONFIG_ENCRYPTED_PW_SIZE; i++) {
        if (decrypted[i] == '\0') {
            password_len = i + 1;  // Include null terminator
            break;
        }
    }

    // Validate password length
    if (password_len == 0 || password_len > (USERCONFIG_WIFI_MAX_PASSWORD_LEN + 1)) {
        // Invalid: no null terminator found or password too long
        memset(key, 0, sizeof(key));
        memset(decrypted, 0, sizeof(decrypted));
        memset(&ctx, 0, sizeof(ctx));
        return false;
    }

    // Copy password to output (including null terminator)
    memcpy(plaintext_out, decrypted, password_len);

    // Clear sensitive data from stack
    memset(key, 0, sizeof(key));
    memset(decrypted, 0, sizeof(decrypted));
    memset(&ctx, 0, sizeof(ctx));

    return true;
}

// ============================================================================
// UserConfig Internal Structure
// ============================================================================

struct userconfig_handle {
    nvram_interface_t* nvram_if;     // NVRAM interface
    userconfig_wifi_t wifi_config;   // WiFi configuration (in RAM, password encrypted)
    char wifi_password_plaintext[USERCONFIG_WIFI_MAX_PASSWORD_LEN + 1];  // Plaintext password cache
    userconfig_network_t net_config; // Network configuration (in RAM)
    userconfig_system_t sys_config;  // System configuration (in RAM)
    userconfig_batmon_t batmon_config;  // Battery monitor calibration (in RAM)
};

// ============================================================================
// Private Helper Functions
// ============================================================================

static void init_default_wifi_config(userconfig_wifi_t* config) {
    memset(config, 0, sizeof(userconfig_wifi_t));
    config->magic = USERCONFIG_WIFI_MAGIC;
    config->version = USERCONFIG_WIFI_VERSION;  // Version 2 (encrypted)
    config->ssid[0] = '\0';
    // encrypted_password and iv initialized to zero by memset
    config->auto_connect = false;
    config->crc32 = 0;
}

static void init_default_network_config(userconfig_network_t* config) {
    memset(config, 0, sizeof(userconfig_network_t));
    config->magic = USERCONFIG_NETWORK_MAGIC;
    config->version = USERCONFIG_NETWORK_VERSION;
    config->use_dhcp = true;
    strncpy(config->static_ip, "192.168.1.100", USERCONFIG_IP_ADDR_LEN - 1);
    strncpy(config->gateway, "192.168.1.1", USERCONFIG_IP_ADDR_LEN - 1);
    strncpy(config->subnet_mask, "255.255.255.0", USERCONFIG_IP_ADDR_LEN - 1);
    strncpy(config->dns_server, "8.8.8.8", USERCONFIG_IP_ADDR_LEN - 1);
    strncpy(config->hostname, "falkradrones", USERCONFIG_HOSTNAME_LEN - 1);
    config->crc32 = 0;
}

static void init_default_system_config(userconfig_system_t* config) {
    memset(config, 0, sizeof(userconfig_system_t));
    config->magic = USERCONFIG_SYSTEM_MAGIC;
    config->version = USERCONFIG_SYSTEM_VERSION;
    config->telemetry_enabled = true;
    config->telemetry_rate_hz = 10;
    config->logging_enabled = true;
    config->logging_rate_hz = 100;
    config->audio_enabled = true;
    config->led_brightness = 50;
    config->pwm_voltage = 1;  // Default to 3.3V
    config->crc32 = 0;
}

static void init_default_batmon_config(userconfig_batmon_t* config) {
    memset(config, 0, sizeof(userconfig_batmon_t));
    config->magic = USERCONFIG_BATMON_MAGIC;
    config->version = USERCONFIG_BATMON_VERSION;
    config->vref = 3.3f;
    config->zero_current_raw = 1.65f;  // Default VCC/2
    config->zero_current_buffered = 1.65f;
    config->sensitivity_raw = 0.040f;  // 40mV/A
    config->sensitivity_buffered = 0.040f;
    config->raw_channel_enabled = 1;  // Both channels enabled by default
    config->buffered_channel_enabled = 1;
    config->crc32 = 0;
}

// ============================================================================
// Initialization & Lifecycle
// ============================================================================

userconfig_t* userconfig_create(nvram_interface_t* nvram_if) {
    if (!nvram_if) {
        return NULL;
    }

    userconfig_t* config = (userconfig_t*)malloc(sizeof(userconfig_t));
    if (!config) {
        return NULL;
    }

    config->nvram_if = nvram_if;

    // Initialize with defaults
    init_default_wifi_config(&config->wifi_config);
    init_default_network_config(&config->net_config);
    init_default_system_config(&config->sys_config);
    init_default_batmon_config(&config->batmon_config);

    // Initialize plaintext password cache to empty
    config->wifi_password_plaintext[0] = '\0';

    return config;
}

void userconfig_destroy(userconfig_t* config) {
    if (config) {
        free(config);
    }
}

bool userconfig_init(userconfig_t* config) {
    if (!config || !config->nvram_if) {
        return false;
    }

    // Load all configuration blocks from NVRAM
    // Note: Load functions return false if no valid data in NVRAM (first boot)
    // In that case, default values from userconfig_create() are used
    bool wifi_ok = userconfig_load_wifi(config);
    bool net_ok = userconfig_load_network(config);
    bool sys_ok = userconfig_load_system(config);
    userconfig_load_batmon(config);

    // Even if loading fails (no saved data), we still have valid defaults
    // So we return true to indicate the config is ready to use
    return true;  // Always return true (defaults are acceptable)
}

// ============================================================================
// WiFi Configuration Implementation
// ============================================================================

bool userconfig_has_wifi_credentials(userconfig_t* config) {
    if (!config) return false;
    // Check SSID and plaintext password cache (not encrypted version)
    return (config->wifi_config.ssid[0] != '\0' && config->wifi_password_plaintext[0] != '\0');
}

const char* userconfig_get_wifi_ssid(userconfig_t* config) {
    return config ? config->wifi_config.ssid : NULL;
}

const char* userconfig_get_wifi_password(userconfig_t* config) {
    // Return plaintext password from RAM cache (never return encrypted version)
    return config ? config->wifi_password_plaintext : NULL;
}

bool userconfig_get_wifi_autoconnect(userconfig_t* config) {
    return config ? config->wifi_config.auto_connect : false;
}

bool userconfig_set_wifi_ssid(userconfig_t* config, const char* ssid) {
    if (!config || !ssid || strlen(ssid) > USERCONFIG_WIFI_MAX_SSID_LEN) {
        return false;
    }
    strncpy(config->wifi_config.ssid, ssid, USERCONFIG_WIFI_MAX_SSID_LEN);
    config->wifi_config.ssid[USERCONFIG_WIFI_MAX_SSID_LEN] = '\0';
    return true;
}

bool userconfig_set_wifi_password(userconfig_t* config, const char* password) {
    if (!config || !password || strlen(password) > USERCONFIG_WIFI_MAX_PASSWORD_LEN) {
        return false;
    }
    // Store plaintext password in RAM cache (will be encrypted during save)
    strncpy(config->wifi_password_plaintext, password, USERCONFIG_WIFI_MAX_PASSWORD_LEN);
    config->wifi_password_plaintext[USERCONFIG_WIFI_MAX_PASSWORD_LEN] = '\0';
    return true;
}

void userconfig_set_wifi_autoconnect(userconfig_t* config, bool enable) {
    if (config) {
        config->wifi_config.auto_connect = enable;
    }
}

bool userconfig_save_wifi(userconfig_t* config) {
    if (!config || !config->nvram_if || !config->nvram_if->write_array) {
        return false;
    }

    // Set magic and version
    config->wifi_config.magic = USERCONFIG_WIFI_MAGIC;
    config->wifi_config.version = USERCONFIG_WIFI_VERSION;  // Version 2 (encrypted)

    // Encrypt password before saving to NVRAM
    if (config->wifi_password_plaintext[0] != '\0') {
        // Password exists - encrypt it
        if (!encrypt_password(config->wifi_password_plaintext,
                              config->wifi_config.encrypted_password,
                              config->wifi_config.iv)) {
            return false;  // Encryption failed
        }
    } else {
        // No password - zero out encrypted fields
        memset(config->wifi_config.encrypted_password, 0, USERCONFIG_ENCRYPTED_PW_SIZE);
        memset(config->wifi_config.iv, 0, USERCONFIG_AES_IV_SIZE);
    }

    // Calculate CRC32 (exclude the CRC field itself)
    uint32_t crc_size = sizeof(userconfig_wifi_t) - sizeof(uint32_t);
    config->wifi_config.crc32 = calculate_crc32((uint8_t*)&config->wifi_config, crc_size);

    // Write to NVRAM
    config->nvram_if->write_array(USERCONFIG_WIFI_BLOCK_ADDR,
                                   (uint8_t*)&config->wifi_config,
                                   sizeof(userconfig_wifi_t));

    return true;
}

bool userconfig_load_wifi(userconfig_t* config) {
    if (!config || !config->nvram_if || !config->nvram_if->read_array) {
        return false;
    }

    // Read from NVRAM (need to read enough bytes for either v1 or v2 structure)
    // Note: v2 structure is larger than v1, so allocate for v2
    userconfig_wifi_t temp_config;
    config->nvram_if->read_array(USERCONFIG_WIFI_BLOCK_ADDR,
                                  (uint8_t*)&temp_config,
                                  sizeof(userconfig_wifi_t));

    // Validate magic number
    if (temp_config.magic != USERCONFIG_WIFI_MAGIC) {
        return false;
    }

    // Handle version migration
    if (temp_config.version == 1) {
        // Legacy v1 format: plaintext password stored directly
        // Note: This requires knowing v1 structure layout
        // For now, we'll just return false to force re-entry
        // TODO: Implement v1→v2 migration if needed
        return false;
    } else if (temp_config.version == 2) {
        // Current v2 format: encrypted password

        // Validate CRC32
        uint32_t stored_crc = temp_config.crc32;
        uint32_t crc_size = sizeof(userconfig_wifi_t) - sizeof(uint32_t);
        uint32_t calculated_crc = calculate_crc32((uint8_t*)&temp_config, crc_size);

        if (stored_crc != calculated_crc) {
            return false;  // Corruption detected
        }

        // Valid v2 configuration - copy to RAM
        memcpy(&config->wifi_config, &temp_config, sizeof(userconfig_wifi_t));

        // Decrypt password from NVRAM into plaintext cache
        if (config->wifi_config.encrypted_password[0] != 0 ||
            config->wifi_config.encrypted_password[1] != 0) {
            // Encrypted password exists - decrypt it
            if (!decrypt_password(config->wifi_config.encrypted_password,
                                  config->wifi_config.iv,
                                  config->wifi_password_plaintext)) {
                // Decryption failed - clear password
                config->wifi_password_plaintext[0] = '\0';
                return false;
            }
        } else {
            // No encrypted password
            config->wifi_password_plaintext[0] = '\0';
        }

        return true;
    } else {
        // Unknown version
        return false;
    }
}

void userconfig_reset_wifi(userconfig_t* config) {
    if (config) {
        init_default_wifi_config(&config->wifi_config);
        config->wifi_password_plaintext[0] = '\0';  // Clear plaintext cache
        userconfig_save_wifi(config);
    }
}

// ============================================================================
// Network Configuration Implementation
// ============================================================================

bool userconfig_get_use_dhcp(userconfig_t* config) {
    return config ? config->net_config.use_dhcp : true;
}

const char* userconfig_get_static_ip(userconfig_t* config) {
    return config ? config->net_config.static_ip : NULL;
}

const char* userconfig_get_gateway(userconfig_t* config) {
    return config ? config->net_config.gateway : NULL;
}

const char* userconfig_get_subnet_mask(userconfig_t* config) {
    return config ? config->net_config.subnet_mask : NULL;
}

const char* userconfig_get_dns_server(userconfig_t* config) {
    return config ? config->net_config.dns_server : NULL;
}

const char* userconfig_get_hostname(userconfig_t* config) {
    return config ? config->net_config.hostname : NULL;
}

void userconfig_set_use_dhcp(userconfig_t* config, bool enable) {
    if (config) {
        config->net_config.use_dhcp = enable;
    }
}

bool userconfig_set_static_ip(userconfig_t* config, const char* ip) {
    if (!config || !ip || strlen(ip) >= USERCONFIG_IP_ADDR_LEN) {
        return false;
    }
    strncpy(config->net_config.static_ip, ip, USERCONFIG_IP_ADDR_LEN - 1);
    config->net_config.static_ip[USERCONFIG_IP_ADDR_LEN - 1] = '\0';
    return true;
}

bool userconfig_set_gateway(userconfig_t* config, const char* gateway) {
    if (!config || !gateway || strlen(gateway) >= USERCONFIG_IP_ADDR_LEN) {
        return false;
    }
    strncpy(config->net_config.gateway, gateway, USERCONFIG_IP_ADDR_LEN - 1);
    config->net_config.gateway[USERCONFIG_IP_ADDR_LEN - 1] = '\0';
    return true;
}

bool userconfig_set_subnet_mask(userconfig_t* config, const char* mask) {
    if (!config || !mask || strlen(mask) >= USERCONFIG_IP_ADDR_LEN) {
        return false;
    }
    strncpy(config->net_config.subnet_mask, mask, USERCONFIG_IP_ADDR_LEN - 1);
    config->net_config.subnet_mask[USERCONFIG_IP_ADDR_LEN - 1] = '\0';
    return true;
}

bool userconfig_set_dns_server(userconfig_t* config, const char* dns) {
    if (!config || !dns || strlen(dns) >= USERCONFIG_IP_ADDR_LEN) {
        return false;
    }
    strncpy(config->net_config.dns_server, dns, USERCONFIG_IP_ADDR_LEN - 1);
    config->net_config.dns_server[USERCONFIG_IP_ADDR_LEN - 1] = '\0';
    return true;
}

bool userconfig_set_hostname(userconfig_t* config, const char* hostname) {
    if (!config || !hostname || strlen(hostname) >= USERCONFIG_HOSTNAME_LEN) {
        return false;
    }
    strncpy(config->net_config.hostname, hostname, USERCONFIG_HOSTNAME_LEN - 1);
    config->net_config.hostname[USERCONFIG_HOSTNAME_LEN - 1] = '\0';
    return true;
}

bool userconfig_save_network(userconfig_t* config) {
    if (!config || !config->nvram_if || !config->nvram_if->write_array) {
        return false;
    }

    config->net_config.magic = USERCONFIG_NETWORK_MAGIC;
    config->net_config.version = USERCONFIG_NETWORK_VERSION;

    uint32_t crc_size = sizeof(userconfig_network_t) - sizeof(uint32_t);
    config->net_config.crc32 = calculate_crc32((uint8_t*)&config->net_config, crc_size);

    config->nvram_if->write_array(USERCONFIG_NETWORK_BLOCK_ADDR,
                                   (uint8_t*)&config->net_config,
                                   sizeof(userconfig_network_t));

    return true;
}

bool userconfig_load_network(userconfig_t* config) {
    if (!config || !config->nvram_if || !config->nvram_if->read_array) {
        return false;
    }

    userconfig_network_t temp_config;
    config->nvram_if->read_array(USERCONFIG_NETWORK_BLOCK_ADDR,
                                  (uint8_t*)&temp_config,
                                  sizeof(userconfig_network_t));

    if (temp_config.magic != USERCONFIG_NETWORK_MAGIC) {
        return false;
    }

    if (temp_config.version != USERCONFIG_NETWORK_VERSION) {
        return false;
    }

    uint32_t stored_crc = temp_config.crc32;
    uint32_t crc_size = sizeof(userconfig_network_t) - sizeof(uint32_t);
    uint32_t calculated_crc = calculate_crc32((uint8_t*)&temp_config, crc_size);

    if (stored_crc != calculated_crc) {
        return false;
    }

    memcpy(&config->net_config, &temp_config, sizeof(userconfig_network_t));
    return true;
}

void userconfig_reset_network(userconfig_t* config) {
    if (config) {
        init_default_network_config(&config->net_config);
        userconfig_save_network(config);
    }
}

// ============================================================================
// System Preferences Implementation
// ============================================================================

bool userconfig_get_telemetry_enabled(userconfig_t* config) {
    return config ? config->sys_config.telemetry_enabled : false;
}

uint32_t userconfig_get_telemetry_rate(userconfig_t* config) {
    return config ? config->sys_config.telemetry_rate_hz : 10;
}

bool userconfig_get_logging_enabled(userconfig_t* config) {
    return config ? config->sys_config.logging_enabled : false;
}

uint32_t userconfig_get_logging_rate(userconfig_t* config) {
    return config ? config->sys_config.logging_rate_hz : 100;
}

bool userconfig_get_audio_enabled(userconfig_t* config) {
    return config ? config->sys_config.audio_enabled : false;
}

uint8_t userconfig_get_led_brightness(userconfig_t* config) {
    return config ? config->sys_config.led_brightness : 50;
}

uint8_t userconfig_get_pwm_voltage(userconfig_t* config) {
    // Returns PWM output voltage: 0 = 5V (default), 1 = 3.3V
    return config ? config->sys_config.pwm_voltage : 0;
}

void userconfig_set_telemetry_enabled(userconfig_t* config, bool enable) {
    if (config) {
        config->sys_config.telemetry_enabled = enable;
    }
}

void userconfig_set_telemetry_rate(userconfig_t* config, uint32_t rate_hz) {
    if (config) {
        if (rate_hz < 1) rate_hz = 1;
        if (rate_hz > 100) rate_hz = 100;
        config->sys_config.telemetry_rate_hz = rate_hz;
    }
}

void userconfig_set_logging_enabled(userconfig_t* config, bool enable) {
    if (config) {
        config->sys_config.logging_enabled = enable;
    }
}

void userconfig_set_logging_rate(userconfig_t* config, uint32_t rate_hz) {
    if (config) {
        if (rate_hz < 1) rate_hz = 1;
        if (rate_hz > 1000) rate_hz = 1000;
        config->sys_config.logging_rate_hz = rate_hz;
    }
}

void userconfig_set_audio_enabled(userconfig_t* config, bool enable) {
    if (config) {
        config->sys_config.audio_enabled = enable;
    }
}

void userconfig_set_led_brightness(userconfig_t* config, uint8_t brightness) {
    if (config) {
        if (brightness > 100) brightness = 100;
        config->sys_config.led_brightness = brightness;
    }
}

void userconfig_set_pwm_voltage(userconfig_t* config, uint8_t voltage) {
    // Sets PWM output voltage: 0 = 5V (default), 1 = 3.3V
    if (config) {
        if (voltage > 1) voltage = 0;  // Validate and default to 5V if invalid
        config->sys_config.pwm_voltage = voltage;
    }
}

bool userconfig_save_system(userconfig_t* config) {
    if (!config || !config->nvram_if || !config->nvram_if->write_array) {
        return false;
    }

    config->sys_config.magic = USERCONFIG_SYSTEM_MAGIC;
    config->sys_config.version = USERCONFIG_SYSTEM_VERSION;

    uint32_t crc_size = sizeof(userconfig_system_t) - sizeof(uint32_t);
    config->sys_config.crc32 = calculate_crc32((uint8_t*)&config->sys_config, crc_size);

    config->nvram_if->write_array(USERCONFIG_SYSTEM_BLOCK_ADDR,
                                   (uint8_t*)&config->sys_config,
                                   sizeof(userconfig_system_t));

    return true;
}

bool userconfig_load_system(userconfig_t* config) {
    if (!config || !config->nvram_if || !config->nvram_if->read_array) {
        return false;
    }

    userconfig_system_t temp_config;
    config->nvram_if->read_array(USERCONFIG_SYSTEM_BLOCK_ADDR,
                                  (uint8_t*)&temp_config,
                                  sizeof(userconfig_system_t));

    if (temp_config.magic != USERCONFIG_SYSTEM_MAGIC) {
        return false;
    }

    if (temp_config.version != USERCONFIG_SYSTEM_VERSION) {
        return false;
    }

    uint32_t stored_crc = temp_config.crc32;
    uint32_t crc_size = sizeof(userconfig_system_t) - sizeof(uint32_t);
    uint32_t calculated_crc = calculate_crc32((uint8_t*)&temp_config, crc_size);

    if (stored_crc != calculated_crc) {
        return false;
    }

    memcpy(&config->sys_config, &temp_config, sizeof(userconfig_system_t));
    return true;
}

void userconfig_reset_system(userconfig_t* config) {
    if (config) {
        init_default_system_config(&config->sys_config);
        userconfig_save_system(config);
    }
}

// ============================================================================
// Diagnostic & Utility Implementation
// ============================================================================

const userconfig_wifi_t* userconfig_get_wifi_block(userconfig_t* config) {
    return config ? &config->wifi_config : NULL;
}

const userconfig_network_t* userconfig_get_network_block(userconfig_t* config) {
    return config ? &config->net_config : NULL;
}

const userconfig_system_t* userconfig_get_system_block(userconfig_t* config) {
    return config ? &config->sys_config : NULL;
}

bool userconfig_validate_all(userconfig_t* config) {
    if (!config) return false;

    bool wifi_ok = userconfig_load_wifi(config);
    bool net_ok = userconfig_load_network(config);
    bool sys_ok = userconfig_load_system(config);

    return (wifi_ok && net_ok && sys_ok);
}

bool userconfig_erase_all(userconfig_t* config) {
    if (!config) return false;

    init_default_wifi_config(&config->wifi_config);
    init_default_network_config(&config->net_config);
    init_default_system_config(&config->sys_config);

    bool wifi_ok = userconfig_save_wifi(config);
    bool net_ok = userconfig_save_network(config);
    bool sys_ok = userconfig_save_system(config);

    return (wifi_ok && net_ok && sys_ok);
}

// ============================================================================
// Battery Monitor Calibration Implementation
// ============================================================================

bool userconfig_has_batmon_calibration(userconfig_t* config) {
    if (!config) return false;
    return (config->batmon_config.magic == USERCONFIG_BATMON_MAGIC &&
            config->batmon_config.version == USERCONFIG_BATMON_VERSION);
}

const userconfig_batmon_t* userconfig_get_batmon_block(userconfig_t* config) {
    if (!config || !userconfig_has_batmon_calibration(config)) return NULL;
    return &config->batmon_config;
}

bool userconfig_set_batmon_calibration(userconfig_t* config, float vref,
                                        float zero_raw, float zero_buffered,
                                        float sens_raw, float sens_buffered,
                                        uint8_t invert_polarity) {
    if (!config) return false;
    if (vref < 1.0f || vref > 5.0f) return false;  // Sanity check
    if (sens_raw < 0.01f || sens_raw > 0.1f) return false;
    if (sens_buffered < 0.01f || sens_buffered > 0.1f) return false;

    config->batmon_config.vref = vref;
    config->batmon_config.zero_current_raw = zero_raw;
    config->batmon_config.zero_current_buffered = zero_buffered;
    config->batmon_config.sensitivity_raw = sens_raw;
    config->batmon_config.sensitivity_buffered = sens_buffered;
    config->batmon_config.invert_polarity = invert_polarity;
    return true;
}

bool userconfig_save_batmon(userconfig_t* config) {
    if (!config || !config->nvram_if) return false;

    config->batmon_config.magic = USERCONFIG_BATMON_MAGIC;
    config->batmon_config.version = USERCONFIG_BATMON_VERSION;

    // Calculate CRC32 (exclude crc32 field itself)
    config->batmon_config.crc32 = calculate_crc32(
        (uint8_t*)&config->batmon_config,
        sizeof(userconfig_batmon_t) - sizeof(uint32_t)
    );

    // Write to NVRAM
    config->nvram_if->write_array(
        USERCONFIG_BATMON_BLOCK_ADDR,
        (uint8_t*)&config->batmon_config,
        sizeof(userconfig_batmon_t)
    );

    return true;
}

bool userconfig_load_batmon(userconfig_t* config) {
    if (!config || !config->nvram_if) return false;

    // Read from NVRAM
    config->nvram_if->read_array(
        USERCONFIG_BATMON_BLOCK_ADDR,
        (uint8_t*)&config->batmon_config,
        sizeof(userconfig_batmon_t)
    );

    // Validate magic and version
    if (config->batmon_config.magic != USERCONFIG_BATMON_MAGIC ||
        config->batmon_config.version != USERCONFIG_BATMON_VERSION) {
        init_default_batmon_config(&config->batmon_config);
        return false;  // No valid calibration
    }

    // Validate CRC32
    uint32_t calculated_crc = calculate_crc32(
        (uint8_t*)&config->batmon_config,
        sizeof(userconfig_batmon_t) - sizeof(uint32_t)
    );

    if (calculated_crc != config->batmon_config.crc32) {
        init_default_batmon_config(&config->batmon_config);
        return false;  // CRC mismatch
    }

    return true;
}

void userconfig_reset_batmon(userconfig_t* config) {
    if (!config) return;
    init_default_batmon_config(&config->batmon_config);
}

bool userconfig_set_batmon_channels(userconfig_t* config, uint8_t raw_enabled, uint8_t buffered_enabled) {
    if (!config) return false;

    // Update channel enable flags
    config->batmon_config.raw_channel_enabled = raw_enabled ? 1 : 0;
    config->batmon_config.buffered_channel_enabled = buffered_enabled ? 1 : 0;

    // Save to NVRAM
    return userconfig_save_batmon(config);
}
