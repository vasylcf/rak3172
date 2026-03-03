/**
 * @file    uart_debug.h
 * @brief   Thin UART printf/println wrapper for STM32 HAL
 *
 * By default uses USART2 (= ST-Link VCP on most Nucleo/RAK boards).
 * Change UART_DEBUG_HANDLE to match your CubeMX-generated handle.
 */

#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  Initialise the debug UART (115200-8N1).
 *         If you already initialised the UART via CubeMX HAL_UART_Init(),
 *         you may skip this call.
 */
void debug_uart_init(void);

/**
 * @brief  Blocking printf over UART (max 256 chars per call).
 */
void debug_printf(const char *fmt, ...);

/**
 * @brief  Send a single character.
 */
void debug_putchar(char c);

/**
 * @brief  Printf followed by "\r\n".
 */
void debug_println(const char *fmt, ...);

/**
 * @brief  Pretty hex dump, 16 bytes per row (matches Arduino hexDump).
 */
void debug_hex_dump(const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* UART_DEBUG_H */
