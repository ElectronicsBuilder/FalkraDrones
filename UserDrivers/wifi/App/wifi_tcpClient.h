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
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef WIFI_TCP_CLIENT_H
#define WIFI_TCP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/
#define TCP_TEST_BUFFER_SIZE    2048

/* Exported functions --------------------------------------------------------*/
/**
  * @brief  Test TCP connection to Python server
  * @param  server_ip: IP address of Python server
  * @param  server_port: Port of Python server
  * @param  test_data: Data to send for testing
  * @param  data_len: Length of test data
  * @retval 0 on success, -1 on error
  */
int32_t wifi_tcp_test(const char* server_ip, uint16_t server_port, const uint8_t* test_data, uint32_t data_len);

/**
  * @brief  Simple TCP test with default parameters
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 on success, -1 on error
  */
int32_t wifi_tcp_simple_test(int32_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_TCP_CLIENT_H */