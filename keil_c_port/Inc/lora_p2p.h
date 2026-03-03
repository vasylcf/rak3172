/**
 * @file    lora_p2p.h
 * @brief   LoRa P2P radio abstraction layer for STM32WL (RAK3172)
 *
 * This wraps the STM32 SubGHz_Phy middleware Radio interface to provide
 * a simple P2P API similar to the RUI3 Arduino API.
 *
 * Requires: STM32CubeWL SubGHz_Phy middleware (radio_driver.c, subghz.c)
 */

#ifndef LORA_P2P_H
#define LORA_P2P_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * Configuration defaults (match the Arduino sketches)
 * -------------------------------------------------------------------------- */
#define LORA_P2P_FREQUENCY        868000000U   /* Hz */
#define LORA_P2P_TX_POWER         22           /* dBm (ERP) */
#define LORA_P2P_PREAMBLE_LEN     8

/* Radio bandwidth enumeration (matches SubGHz_Phy / SX126x definitions) */
typedef enum {
    LORA_BW_125  = 0,  /* 125 kHz */
    LORA_BW_250  = 1,  /* 250 kHz */
    LORA_BW_500  = 2,  /* 500 kHz */
} lora_bw_t;

/* Radio coding rate enumeration */
typedef enum {
    LORA_CR_4_5  = 1,
    LORA_CR_4_6  = 2,
    LORA_CR_4_7  = 3,
    LORA_CR_4_8  = 4,
} lora_cr_t;

/* --------------------------------------------------------------------------
 * P2P configuration structure
 * -------------------------------------------------------------------------- */
typedef struct {
    uint32_t  frequency;       /* RF frequency in Hz */
    uint8_t   spreading_factor;/* 7..12 */
    lora_bw_t bandwidth;
    lora_cr_t coding_rate;
    uint16_t  preamble_len;
    int8_t    tx_power;        /* dBm */
} lora_p2p_config_t;

/* --------------------------------------------------------------------------
 * Received packet descriptor (mirrors rui_lora_p2p_recv_t)
 * -------------------------------------------------------------------------- */
typedef struct {
    uint8_t  *buffer;
    uint8_t   buffer_size;
    int16_t   rssi;
    int8_t    snr;
} lora_p2p_rx_data_t;

/* --------------------------------------------------------------------------
 * Callback typedefs
 * -------------------------------------------------------------------------- */
typedef void (*lora_p2p_rx_cb_t)(lora_p2p_rx_data_t data);
typedef void (*lora_p2p_tx_cb_t)(void);
typedef void (*lora_p2p_rx_timeout_cb_t)(void);
typedef void (*lora_p2p_rx_error_cb_t)(void);

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief  Initialise the SubGHz radio in LoRa P2P mode.
 * @param  cfg  Pointer to configuration. If NULL, uses compiled defaults.
 * @retval true on success
 */
bool lora_p2p_init(const lora_p2p_config_t *cfg);

/**
 * @brief  Register receive callback.
 */
void lora_p2p_register_rx_callback(lora_p2p_rx_cb_t cb);

/**
 * @brief  Register transmit-done callback.
 */
void lora_p2p_register_tx_callback(lora_p2p_tx_cb_t cb);

/**
 * @brief  Register RX timeout callback.
 */
void lora_p2p_register_rx_timeout_callback(lora_p2p_rx_timeout_cb_t cb);

/**
 * @brief  Register RX error callback.
 */
void lora_p2p_register_rx_error_callback(lora_p2p_rx_error_cb_t cb);

/**
 * @brief  Start receiving.
 * @param  timeout_ms  0 = single RX, 65534 = continuous, else timeout in ms.
 * @retval true on success
 */
bool lora_p2p_receive(uint32_t timeout_ms);

/**
 * @brief  Stop receiving (put radio to standby).
 * @retval true on success
 */
bool lora_p2p_standby(void);

/**
 * @brief  Transmit a packet.
 * @param  data  Payload bytes
 * @param  len   Payload length (max 255)
 * @retval true on success
 */
bool lora_p2p_send(const uint8_t *data, uint8_t len);

/**
 * @brief  Must be called from Radio IRQ handler (SUBGHZ_Radio_IRQHandler).
 *         Processes radio events and dispatches callbacks.
 */
void lora_p2p_irq_process(void);

#ifdef __cplusplus
}
#endif

#endif /* LORA_P2P_H */
