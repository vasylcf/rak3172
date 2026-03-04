/**
 * @file  hw_init.h
 * @brief Minimal hardware initialisation for RAK3172 (STM32WLE5CC)
 *        without CubeMX-generated main.c.
 *
 * Call hw_init() at the very start of main() BEFORE any HAL function.
 * It replaces:
 *   HAL_Init() + SystemClock_Config() + MX_GPIO_Init()
 *   + MX_USART2_UART_Init() + MX_SubGHz_Init()
 *
 * If you are using a CubeMX-generated project you do NOT need this file -
 * keep the generated initialisation and remove MX_SubGHz_Init() from main().
 */

#ifndef HW_INIT_H
#define HW_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wlxx_hal.h"

/**
 * @brief  Initialise clocks, GPIO, USART2 (115200-8N1) and provide the
 *         huart2 handle used by uart_debug.c.
 */
void hw_init(void);

/* huart2 used by uart_debug.c */
extern UART_HandleTypeDef huart2;

#ifdef __cplusplus
}
#endif

#endif /* HW_INIT_H */
