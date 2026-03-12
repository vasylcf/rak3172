/**
 * @file    lora_p2p.c
 * @brief   LoRa P2P radio driver - direct HAL_SUBGHZ implementation
 *
 * Talks directly to the SX126x radio that is built into the STM32WLE5
 * via the STM32WL HAL_SUBGHZ API.
 *
 * NO SubGHz_Phy middleware required (no radio.h, no radio_driver.c,
 * no MX_SubGHz_Init).
 *
 * Minimum CubeMX project
 * ----------------------
 *  - USART2 enabled  ->  generates usart.c / huart2  (uart_debug)
 *  - GPIO as needed
 *  - SubGHz_Phy middleware: NOT needed, do NOT add it
 *  - SUBGHZ peripheral: optionally enable so CubeMX generates
 *    HAL_SUBGHZ_MspInit(); if absent, this driver enables the
 *    clock itself.
 *
 * TCXO voltage (DIO3 output)
 * --------------------------
 *  Override with -DLORA_TCXO_VOLTAGE=0x07 etc.
 *  0x00=1.6 V  0x01=1.7 V  0x02=1.8 V  0x03=2.2 V
 *  0x04=2.4 V  0x05=2.7 V  0x06=3.0 V  0x07=3.3 V
 *  RAK3172 TCXO is fed from DIO3; typical value: 0x06 (3.0 V).
 */

#include "lora_p2p.h"
#include "stm32wlxx_hal.h"
#include "uart_debug.h"
#include <string.h>

/* 1 = extra debug prints in IRQ path (disable for production) */
#define LORA_P2P_DEBUG  1

/* ==========================================================================
 * SX126x constants
 * ========================================================================== */
#define SX_STDBY_RC             0x00U
#define SX_STDBY_XOSC           0x01U
#define SX_PKT_TYPE_LORA        0x01U
#define SX_REG_DCDC             0x01U

/* PA config - HP path (STM32WLE5 / SX1262) */
#define SX_PA_DUTY_CYCLE        0x04U
#define SX_PA_HP_MAX            0x07U
#define SX_PA_DEVSEL_SX1262     0x00U
#define SX_PA_LUT               0x01U

#define SX_RAMP_200US           0x04U

/* LoRa bandwidth register values */
#define SX_BW_125               0x04U
#define SX_BW_250               0x05U
#define SX_BW_500               0x06U

/* LoRa packet params */
#define SX_LORA_HDR_VARIABLE    0x00U
#define SX_LORA_CRC_ON          0x01U
#define SX_LORA_IQ_STD          0x00U

/* IRQ bit-masks */
#define SX_IRQ_TX_DONE          0x0001U
#define SX_IRQ_RX_DONE          0x0002U
#define SX_IRQ_CRC_ERR          0x0040U
#define SX_IRQ_TIMEOUT          0x0200U
#define SX_IRQ_ALL              (SX_IRQ_TX_DONE | SX_IRQ_RX_DONE | \
                                  SX_IRQ_CRC_ERR | SX_IRQ_TIMEOUT)

/* TCXO on DIO3 */
#ifndef LORA_TCXO_VOLTAGE
#  define LORA_TCXO_VOLTAGE     0x06U   /* 3.0 V */
#endif
/* Stabilisation timeout: 5 ms / 15.625 us = 320 = 0x000140 */
#define TCXO_TO_HI              0x00U
#define TCXO_TO_MID             0x01U
#define TCXO_TO_LO              0x40U

/* Image calibration for 868 MHz band */
#define CAL_IMG_F1              0xD7U
#define CAL_IMG_F2              0xDBU

/* RF frequency register value: f_Hz * 2^25 / 32e6 */
#define RF_FREQ_REG(hz)  ((uint32_t)(((uint64_t)(hz) * (1ULL << 25)) / 32000000ULL))

/* TX timeout: 3 s in 15.625 us units = 192000 = 0x02EE00 */
#define TX_TO_HI                0x02U
#define TX_TO_MID               0xEEU
#define TX_TO_LO                0x00U

/* RX timeout */
#define RX_CONTINUOUS           0xFFFFFFUL
#define MS_TO_RXTIMEOUT(ms)     ((uint32_t)(ms) * 64UL)

/* DIO2-as-RF-switch: RADIO_SET_RFSWITCHMODE (0x9D) in HAL enum */

/* ==========================================================================
 * Private state
 * ========================================================================== */
static SUBGHZ_HandleTypeDef     s_hsubghz;
static lora_p2p_config_t        s_cfg;
static lora_p2p_rx_cb_t         s_rx_cb         = NULL;
static lora_p2p_tx_cb_t         s_tx_cb         = NULL;
static lora_p2p_rx_timeout_cb_t s_rx_timeout_cb = NULL;
static lora_p2p_rx_error_cb_t   s_rx_error_cb   = NULL;

static volatile uint8_t s_irq_pending = 0U;

#define RX_BUF_SIZE  256U
static uint8_t s_rx_buf[RX_BUF_SIZE];

/* ==========================================================================
 * SX126x low-level wrappers  (all blocking, use HAL SUBGHZ SPI)
 * ========================================================================== */

static void sx_cmd1(SUBGHZ_RadioSetCmd_t cmd, uint8_t val)
{
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, cmd, &val, 1U);
}

static void sx_set_standby(uint8_t mode)   { sx_cmd1(RADIO_SET_STANDBY,       mode); }
static void sx_set_pkt_type_lora(void)     { sx_cmd1(RADIO_SET_PACKETTYPE,     SX_PKT_TYPE_LORA); }
static void sx_set_regulator_dcdc(void)    { sx_cmd1(RADIO_SET_REGULATORMODE,  SX_REG_DCDC); }

static void sx_set_tcxo(void)
{
    uint8_t p[4] = { LORA_TCXO_VOLTAGE, TCXO_TO_HI, TCXO_TO_MID, TCXO_TO_LO };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_TCXOMODE, p, 4U);
}

static void sx_calibrate_all(void)
{
    uint8_t p = 0x7FU;
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_CALIBRATE, &p, 1U);
    HAL_Delay(5U);
}

static void sx_calibrate_image(void)
{
    uint8_t p[2] = { CAL_IMG_F1, CAL_IMG_F2 };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_CALIBRATEIMAGE, p, 2U);
}

static void sx_set_frequency(uint32_t hz)
{
    uint32_t r = RF_FREQ_REG(hz);
    uint8_t p[4] = { (uint8_t)(r >> 24), (uint8_t)(r >> 16),
                     (uint8_t)(r >>  8), (uint8_t)(r) };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_RFFREQUENCY, p, 4U);
}

static void sx_set_pa_hp(void)
{
    uint8_t p[4] = { SX_PA_DUTY_CYCLE, SX_PA_HP_MAX,
                     SX_PA_DEVSEL_SX1262, SX_PA_LUT };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_PACONFIG, p, 4U);
}

static void sx_set_tx_params(int8_t dbm)
{
    uint8_t p[2] = { (uint8_t)dbm, SX_RAMP_200US };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_TXPARAMS, p, 2U);
}

static uint8_t bw_reg(lora_bw_t bw)
{
    if (bw == LORA_BW_250) return SX_BW_250;
    if (bw == LORA_BW_500) return SX_BW_500;
    return SX_BW_125;
}

static void sx_set_modulation(void)
{
    uint8_t sf   = s_cfg.spreading_factor;
    uint8_t bw   = bw_reg(s_cfg.bandwidth);
    uint8_t cr   = (uint8_t)s_cfg.coding_rate; /* enum == SX126x value */
    /* LDRO needed when symbol time > 16.38 ms */
    uint8_t ldro = ((sf == 12U) || (sf == 11U && bw == SX_BW_125)) ? 1U : 0U;
    uint8_t p[4] = { sf, bw, cr, ldro };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_MODULATIONPARAMS, p, 4U);
}

static void sx_set_pkt_params(uint8_t payload_len)
{
    uint16_t pre = s_cfg.preamble_len;
    uint8_t p[6] = { (uint8_t)(pre >> 8), (uint8_t)pre,
                     SX_LORA_HDR_VARIABLE, payload_len,
                     SX_LORA_CRC_ON,       SX_LORA_IQ_STD };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_PACKETPARAMS, p, 6U);
}

static void sx_set_buf_base(void)
{
    uint8_t p[2] = { 0x00U, 0x00U };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_BUFFERBASEADDRESS, p, 2U);
}

static void sx_set_dio2_rf_switch(void)
{
    uint8_t p = 0x01U;
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_RFSWITCHMODE, &p, 1U);
}

static void sx_set_dio_irq(void)
{
    uint8_t p[8] = {
        (uint8_t)(SX_IRQ_ALL >> 8), (uint8_t)SX_IRQ_ALL,   /* global  */
        (uint8_t)(SX_IRQ_ALL >> 8), (uint8_t)SX_IRQ_ALL,   /* -> DIO1 */
        0x00U, 0x00U,                                         /* DIO2   */
        0x00U, 0x00U                                          /* DIO3   */
    };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_CFG_DIOIRQ, p, 8U);
}

static uint16_t sx_get_irq(void)
{
    uint8_t b[2] = {0};
    HAL_SUBGHZ_ExecGetCmd(&s_hsubghz, RADIO_GET_IRQSTATUS, b, 2U);
    return ((uint16_t)b[0] << 8) | b[1];
}

static void sx_clear_irq(uint16_t mask)
{
    uint8_t p[2] = { (uint8_t)(mask >> 8), (uint8_t)mask };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_CLR_IRQSTATUS, p, 2U);
}

static void sx_start_rx(uint32_t timeout_ms)
{
    uint32_t t = (timeout_ms == 0U || timeout_ms >= 65534U)
                    ? RX_CONTINUOUS
                    : MS_TO_RXTIMEOUT(timeout_ms);
    if (t > 0xFFFFFEUL) t = 0xFFFFFEUL;
    uint8_t p[3] = { (uint8_t)(t >> 16), (uint8_t)(t >> 8), (uint8_t)t };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_RX, p, 3U);
}

static void sx_start_tx(void)
{
    uint8_t p[3] = { TX_TO_HI, TX_TO_MID, TX_TO_LO };
    HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_TX, p, 3U);
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

bool lora_p2p_init(const lora_p2p_config_t *cfg)
{
    if (cfg) {
        s_cfg = *cfg;
    } else {
        s_cfg.frequency        = LORA_P2P_FREQUENCY;
        s_cfg.spreading_factor = 12U;
        s_cfg.bandwidth        = LORA_BW_125;
        s_cfg.coding_rate      = LORA_CR_4_5;
        s_cfg.preamble_len     = LORA_P2P_PREAMBLE_LEN;
        s_cfg.tx_power         = LORA_P2P_TX_POWER;
    }

    /* Enable SUBGHZ SPI clock (idempotent) */
    __HAL_RCC_SUBGHZSPI_CLK_ENABLE();

    s_hsubghz.Init.BaudratePrescaler = SUBGHZSPI_BAUDRATEPRESCALER_4;
    if (HAL_SUBGHZ_Init(&s_hsubghz) != HAL_OK) {
#if LORA_P2P_DEBUG
        debug_println("[LORA] HAL_SUBGHZ_Init FAILED");
#endif
        return false;
    }
#if LORA_P2P_DEBUG
    debug_println("[LORA] HAL_SUBGHZ_Init OK");
#endif

    HAL_NVIC_SetPriority(SUBGHZ_Radio_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
#if LORA_P2P_DEBUG
    debug_println("[LORA] NVIC enabled for SUBGHZ_Radio_IRQn");
#endif

    /*
     * SX126x init sequence:
     *   STDBY_RC -> TCXO -> wait -> calibrate -> DCDC -> LoRa packet type
     *   -> frequency -> image cal -> PA -> TX params -> modulation
     *   -> packet params -> buffer base -> DIO2 RF switch -> DIO IRQ
     */
    sx_set_standby(SX_STDBY_RC);
    sx_set_tcxo();
    HAL_Delay(10U);

    sx_calibrate_all();
    sx_set_regulator_dcdc();
    sx_set_pkt_type_lora();

    sx_set_frequency(s_cfg.frequency);
    sx_calibrate_image();
    HAL_Delay(3U);

    sx_set_pa_hp();
    sx_set_tx_params(s_cfg.tx_power);
    sx_set_modulation();
    sx_set_pkt_params(0xFFU);
    sx_set_buf_base();
    sx_set_dio2_rf_switch();
    sx_set_dio_irq();

    return true;
}

void lora_p2p_register_rx_callback(lora_p2p_rx_cb_t cb)         { s_rx_cb         = cb; }
void lora_p2p_register_tx_callback(lora_p2p_tx_cb_t cb)         { s_tx_cb         = cb; }
void lora_p2p_register_rx_timeout_callback(lora_p2p_rx_timeout_cb_t cb) { s_rx_timeout_cb = cb; }
void lora_p2p_register_rx_error_callback(lora_p2p_rx_error_cb_t cb)     { s_rx_error_cb   = cb; }

bool lora_p2p_receive(uint32_t timeout_ms)
{
    sx_set_pkt_params(0xFFU);
    sx_start_rx(timeout_ms);
    return true;
}

bool lora_p2p_standby(void)
{
    sx_set_standby(SX_STDBY_XOSC);
    return true;
}

bool lora_p2p_send(const uint8_t *data, uint8_t len)
{
    sx_set_standby(SX_STDBY_XOSC);
    sx_set_pkt_params(len);
    HAL_SUBGHZ_WriteBuffer(&s_hsubghz, 0x00U, (uint8_t *)data, len);
    sx_start_tx();
    return true;
}

/* --------------------------------------------------------------------------
 * lora_p2p_irq_process
 *
 * Call from BOTH:
 *   SUBGHZ_Radio_IRQHandler  - sets pending flag, returns (no SPI in ISR)
 *   main super-loop          - does the actual SPI reads and callbacks
 * -------------------------------------------------------------------------- */
void lora_p2p_irq_process(void)
{
    if (__get_IPSR() != 0U) {   /* called from ISR */
        s_irq_pending = 1U;
        /* EXTI 44 is a direct (level-sensitive) line.  DIO1 stays high
         * until the SX126x IRQ is cleared via SPI, which we do NOT do
         * from ISR context.  Mask the NVIC line so the ISR does not
         * re-fire endlessly; the main-loop path below re-enables it
         * after clearing the radio IRQ.                                */
        HAL_NVIC_DisableIRQ(SUBGHZ_Radio_IRQn);
#if LORA_P2P_DEBUG
        debug_putchar('!');
#endif
        return;
    }

    if (!s_irq_pending) {
#if LORA_P2P_DEBUG
        /* Poll radio status every ~10 calls (~5 s at 500 ms loop) */
        static uint32_t dbg_poll = 0;
        if (++dbg_poll >= 10U) {
            dbg_poll = 0U;
            uint16_t poll_irq = sx_get_irq();
            uint8_t  opmode   = 0;
            HAL_SUBGHZ_ExecGetCmd(&s_hsubghz, RADIO_GET_STATUS, &opmode, 1U);
            debug_printf("[LORA] POLL irq=0x%04X mode=0x%02X pend=%u\r\n",
                         poll_irq, opmode, s_irq_pending);
            /* If radio has pending IRQ bits but NVIC never fired,
             * process them right here so we can at least see what happens */
            if (poll_irq != 0U) {
                debug_println("[LORA] POLL: forcing IRQ processing!");
                s_irq_pending = 1U;
            }
        }
#endif
        return;
    }
    s_irq_pending = 0U;

    uint16_t irq = sx_get_irq();
    sx_clear_irq(irq);                 /* DIO1 → low */
    HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn); /* safe to re-arm now */

#if LORA_P2P_DEBUG
    debug_printf("[LORA] IRQ=0x%04X\r\n", irq);
#endif

    if (irq & SX_IRQ_TX_DONE) {
        if (s_tx_cb) s_tx_cb();
    }

    if (irq & SX_IRQ_RX_DONE) {
        uint8_t rxbuf[2] = {0};
        HAL_SUBGHZ_ExecGetCmd(&s_hsubghz, RADIO_GET_RXBUFFERSTATUS, rxbuf, 2U);
        uint8_t plen   = rxbuf[0];
        uint8_t offset = rxbuf[1];

        uint8_t copylen = (plen > (uint8_t)RX_BUF_SIZE) ? (uint8_t)RX_BUF_SIZE : plen;
        HAL_SUBGHZ_ReadBuffer(&s_hsubghz, offset, s_rx_buf, copylen);

        /* LoRa packet status: [0]=RssiPkt, [1]=SnrPkt(signed), [2]=SignalRssi */
        uint8_t pkt[3] = {0};
        HAL_SUBGHZ_ExecGetCmd(&s_hsubghz, RADIO_GET_PACKETSTATUS, pkt, 3U);
        int16_t rssi = -(int16_t)pkt[0] / 2;
        int8_t  snr  = (int8_t)pkt[1]  / 4;

        if (s_rx_cb) {
            lora_p2p_rx_data_t d = {
                .buffer      = s_rx_buf,
                .buffer_size = copylen,
                .rssi        = rssi,
                .snr         = snr,
            };
            s_rx_cb(d);
        }
    }

    if (irq & SX_IRQ_CRC_ERR) {
        if (s_rx_error_cb) s_rx_error_cb();
    }

    if (irq & SX_IRQ_TIMEOUT) {
        if (s_rx_timeout_cb) s_rx_timeout_cb();
    }
}
