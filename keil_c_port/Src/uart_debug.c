/**
 * @file    uart_debug.c
 * @brief   Blocking debug UART output for STM32 HAL
 *
 * Expects CubeMX-generated UART handle "huart2".
 * If your board uses a different UART, change UART_DEBUG_HANDLE below.
 */

#include "uart_debug.h"
#include "stm32wlxx_hal.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---- Handle provided by CubeMX-generated code (usart.c) ---- */
extern UART_HandleTypeDef huart2;
#define UART_DEBUG_HANDLE   (&huart2)

/* Max single-call output length */
#define DBG_BUF_SIZE   256

/* -------------------------------------------------------------------------- */

void debug_uart_init(void)
{
    /* Normally CubeMX generates MX_USART2_UART_Init().
     * Call that from main() BEFORE calling this, or add init here. */
}

void debug_putchar(char c)
{
    HAL_UART_Transmit(UART_DEBUG_HANDLE, (uint8_t *)&c, 1, HAL_MAX_DELAY);
}

void debug_printf(const char *fmt, ...)
{
    char buf[DBG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        HAL_UART_Transmit(UART_DEBUG_HANDLE, (uint8_t *)buf,
                          (uint16_t)len, HAL_MAX_DELAY);
    }
}

void debug_println(const char *fmt, ...)
{
    char buf[DBG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf) - 2, fmt, args);
    va_end(args);
    if (len > 0) {
        buf[len++] = '\r';
        buf[len++] = '\n';
        HAL_UART_Transmit(UART_DEBUG_HANDLE, (uint8_t *)buf,
                          (uint16_t)len, HAL_MAX_DELAY);
    }
}

void debug_hex_dump(const uint8_t *buf, uint16_t len)
{
    static const char hex[17] = "0123456789abcdef";

    debug_println("   +------------------------------------------------+ +----------------+");
    debug_println("   |.0 .1 .2 .3 .4 .5 .6 .7 .8 .9 .a .b .c .d .e .f | |      ASCII     |");

    for (uint16_t i = 0; i < len; i += 16) {
        if (i % 128 == 0) {
            debug_println("   +------------------------------------------------+ +----------------+");
        }

        char s[] = "|                                                | |                |";
        uint8_t ix = 1, iy = 52;

        for (uint8_t j = 0; j < 16; j++) {
            if (i + j < len) {
                uint8_t c = buf[i + j];
                s[ix++] = hex[(c >> 4) & 0x0F];
                s[ix++] = hex[c & 0x0F];
                ix++;
                s[iy++] = (c > 31 && c < 128) ? (char)c : '.';
            }
        }

        uint8_t index = (uint8_t)(i / 16);
        if (i < 256) debug_putchar(' ');
        debug_printf("%X.", index);
        debug_println("%s", s);
    }

    debug_println("   +------------------------------------------------+ +----------------+");
}
