/**
 * @file    lora_p2p.c
 * @brief   LoRa P2P radio driver – wraps STM32 SubGHz_Phy middleware
 *
 * This file implements the API declared in lora_p2p.h on top of the
 * ST-provided "Radio" driver (radio.h / radio_driver.c) that ships with
 * the STM32CubeWL firmware package.
 *
 * Integration steps
 * -----------------
 * 1. Generate a CubeMX project for STM32WLE5xx with SubGHz_Phy middleware.
 * 2. In CubeMX, enable SubGHz radio and generate code.
 * 3. Add this file + lora_p2p.h to your Keil project sources/includes.
 * 4. The generated code already provides: radio.h, radio_driver.c, subghz.c
 *
 * NOTE: If you are using the bare STM32WL HAL *without* the SubGHz_Phy
 * middleware, you will need to write low-level SPI/IRQ code for the
 * integrated SX126x yourself. Using the middleware is strongly recommended.
 */

#include "lora_p2p.h"

/* ---- STM32 HAL & SubGHz_Phy middleware headers ---- */
/* These are provided by the STM32CubeWL package.
 * If your Keil project doesn't find them, add the middleware include path
 * in Options -> C/C++ -> Include Paths.
 */
#include "radio.h"            /* SubGHz_Phy: Radio driver interface */
#include "stm32wlxx_hal.h"

#include <string.h>

/* --------------------------------------------------------------------------
 * Private state
 * -------------------------------------------------------------------------- */
static lora_p2p_config_t  s_config;
static lora_p2p_rx_cb_t          s_rx_cb          = NULL;
static lora_p2p_tx_cb_t          s_tx_cb          = NULL;
static lora_p2p_rx_timeout_cb_t  s_rx_timeout_cb  = NULL;
static lora_p2p_rx_error_cb_t    s_rx_error_cb    = NULL;

/* Temporary RX buffer used by the IRQ callback */
#define RX_BUF_SIZE   256
static uint8_t s_rx_buf[RX_BUF_SIZE];

/* --------------------------------------------------------------------------
 * Internal SubGHz_Phy radio event callbacks
 * -------------------------------------------------------------------------- */

/** Called by Radio driver when TX completes. */
static void on_tx_done(void)
{
    if (s_tx_cb) {
        s_tx_cb();
    }
}

/** Called by Radio driver when a packet is received. */
static void on_rx_done(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    if (s_rx_cb) {
        lora_p2p_rx_data_t data;
        /* Copy into our buffer so the caller can use it safely */
        uint8_t copy_len = (size > RX_BUF_SIZE) ? RX_BUF_SIZE : (uint8_t)size;
        memcpy(s_rx_buf, payload, copy_len);
        data.buffer      = s_rx_buf;
        data.buffer_size = copy_len;
        data.rssi        = rssi;
        data.snr         = snr;
        s_rx_cb(data);
    }
}

/** Called by Radio driver on RX timeout. */
static void on_rx_timeout(void)
{
    if (s_rx_timeout_cb) {
        s_rx_timeout_cb();
    }
}

/** Called by Radio driver on RX CRC / sync error. */
static void on_rx_error(void)
{
    if (s_rx_error_cb) {
        s_rx_error_cb();
    }
}

/* Radio event table expected by the SubGHz_Phy middleware */
static RadioEvents_t s_radio_events = {
    .TxDone    = on_tx_done,
    .RxDone    = on_rx_done,
    .TxTimeout = NULL,
    .RxTimeout = on_rx_timeout,
    .RxError   = on_rx_error,
};

/* --------------------------------------------------------------------------
 * Map BW enum to SubGHz_Phy bandwidth constant
 * -------------------------------------------------------------------------- */
static uint32_t map_bw(lora_bw_t bw)
{
    switch (bw) {
        case LORA_BW_250: return 1; /* LORA_BW_250 in radio_driver */
        case LORA_BW_500: return 2; /* LORA_BW_500 */
        case LORA_BW_125:
        default:          return 0; /* LORA_BW_125 */
    }
}

/* --------------------------------------------------------------------------
 * Public API implementation
 * -------------------------------------------------------------------------- */

bool lora_p2p_init(const lora_p2p_config_t *cfg)
{
    if (cfg) {
        s_config = *cfg;
    } else {
        /* Defaults matching Master sketch */
        s_config.frequency        = LORA_P2P_FREQUENCY;
        s_config.spreading_factor = 12;
        s_config.bandwidth        = LORA_BW_125;
        s_config.coding_rate      = LORA_CR_4_5;
        s_config.preamble_len     = LORA_P2P_PREAMBLE_LEN;
        s_config.tx_power         = LORA_P2P_TX_POWER;
    }

    /* Initialise the Radio driver with our event callbacks */
    Radio.Init(&s_radio_events);

    /* Set the RF channel */
    Radio.SetChannel(s_config.frequency);

    /* Configure TX */
    Radio.SetTxConfig(
        MODEM_LORA,
        s_config.tx_power,
        0,                              /* fdev (FSK only) */
        map_bw(s_config.bandwidth),
        s_config.spreading_factor,
        s_config.coding_rate,
        s_config.preamble_len,
        false,                          /* fixLen */
        true,                           /* crcOn */
        false,                          /* freqHopOn */
        0,                              /* hopPeriod */
        false,                          /* iqInverted */
        3000                            /* TX timeout ms */
    );

    /* Configure RX */
    Radio.SetRxConfig(
        MODEM_LORA,
        map_bw(s_config.bandwidth),
        s_config.spreading_factor,
        s_config.coding_rate,
        0,                              /* bandwidthAfc (FSK only) */
        s_config.preamble_len,
        0,                              /* symbolTimeout */
        false,                          /* fixLen */
        0,                              /* payloadLen */
        true,                           /* crcOn */
        false,                          /* freqHopOn */
        0,                              /* hopPeriod */
        false,                          /* iqInverted */
        true                            /* rxContinuous */
    );

    Radio.SetMaxPayloadLength(MODEM_LORA, 255);

    return true;
}

void lora_p2p_register_rx_callback(lora_p2p_rx_cb_t cb)
{
    s_rx_cb = cb;
}

void lora_p2p_register_tx_callback(lora_p2p_tx_cb_t cb)
{
    s_tx_cb = cb;
}

void lora_p2p_register_rx_timeout_callback(lora_p2p_rx_timeout_cb_t cb)
{
    s_rx_timeout_cb = cb;
}

void lora_p2p_register_rx_error_callback(lora_p2p_rx_error_cb_t cb)
{
    s_rx_error_cb = cb;
}

bool lora_p2p_receive(uint32_t timeout_ms)
{
    if (timeout_ms == 65534U || timeout_ms == 0U) {
        /* Continuous RX (no timeout) – mirrors RUI3 precv(65534) */
        Radio.Rx(0);
    } else {
        Radio.Rx(timeout_ms);
    }
    return true;
}

bool lora_p2p_standby(void)
{
    Radio.Standby();
    return true;
}

bool lora_p2p_send(const uint8_t *data, uint8_t len)
{
    Radio.Standby();                     /* ensure not in RX */
    Radio.Send((uint8_t *)data, len);    /* cast away const (driver API) */
    return true;
}

void lora_p2p_irq_process(void)
{
    /* The SubGHz_Phy middleware provides Radio.IrqProcess()
     * which reads the IRQ flags and dispatches our callbacks. */
    Radio.IrqProcess();
}
