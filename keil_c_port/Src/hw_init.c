/**
 * @file  hw_init.c
 * @brief Minimal HAL hardware initialisation for RAK3172 / STM32WLE5CC.
 *
 * Use this file when you do NOT have a CubeMX-generated main.c.
 * If you DO have CubeMX code, keep its generated init files and delete
 * this file; just remove the MX_SubGHz_Init() call from your main().
 *
 * Clock tree
 * ----------
 *  MSI 48 MHz  ->  SYSCLK = HCLK = 48 MHz
 *  APB1 = APB2 = 48 MHz
 *  (No external HSE crystal on RAK3172; the radio TCXO is handled by
 *   lora_p2p.c via the RADIO_SET_TCXOMODE command.)
 *
 * Peripherals initialised here
 * ----------------------------
 *  USART2  PA2/PA3  115200-8N1  (debug UART, accessible via USB-UART adapter)
 */

#include "hw_init.h"
#include <string.h>

UART_HandleTypeDef huart2;

/* --------------------------------------------------------------------------
 * Error_Handler - required by CubeMX-generated stm32wlxx_hal_msp.c
 * -------------------------------------------------------------------------- */
void Error_Handler(void)
{
    __disable_irq();
    while (1) { /* stay here */ }
}

/* --------------------------------------------------------------------------
 * SystemClock_Config - 48 MHz from MSI
 * -------------------------------------------------------------------------- */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Enable MSI, set to 48 MHz range (range 11) */
    osc.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    osc.MSIState       = RCC_MSI_ON;
    osc.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    osc.MSIClockRange  = RCC_MSIRANGE_11;   /* 48 MHz */
    osc.PLL.PLLState   = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                       | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_MSI;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

/* --------------------------------------------------------------------------
 * MX_GPIO_Init - enable port clocks (extend as needed)
 * -------------------------------------------------------------------------- */
void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
}

/* --------------------------------------------------------------------------
 * MX_USART2_UART_Init - PA2=TX, PA3=RX, 115200 8N1
 * -------------------------------------------------------------------------- */
void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &g);

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

/* --------------------------------------------------------------------------
 * hw_init - call once at the start of main()
 * -------------------------------------------------------------------------- */
void hw_init(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
}
