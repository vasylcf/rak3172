/**
 * @file    main_master.c
 * @brief   LoRa P2P Master – plain C port for STM32WL / Keil
 *
 * Ported from: RAK3172_P2P_Master.ino
 * Original:    https://github.com/Kongduino/RUI3_LoRa_P2P_PING_PONG
 *
 * Behaviour
 * ---------
 *  - On startup: configure radio, enter RX for 3 s, then loop.
 *  - Loop: every 5 s send a fixed payload ("508012"), then return to RX.
 *  - When a packet arrives, print RSSI/SNR and a hex dump.
 *
 * Build
 * -----
 *  Compile ONLY this file (not main_slave.c) when building the Master
 *  firmware. Both files define main(), so include exactly one.
 *
 * Prerequisites
 * -------------
 *  - STM32CubeMX project for STM32WLE5CC with:
 *      USART2  enabled (115200-8N1, for debug output)
 *      SubGHz  radio enabled (LoRa modem)
 *  - STM32CubeWL SubGHz_Phy middleware added to the project.
 */

#include "stm32wlxx_hal.h"
#include "lora_p2p.h"
#include "uart_debug.h"
#include "app_config.h"

#include <string.h>
#include <stdbool.h>

/*
 * hw_init() replaces the four CubeMX-generated calls below.
 * Delete this include and restore the extern declarations if you
 * are using a CubeMX-generated project.
 */
#include "hw_init.h"

/* --------------------------------------------------------------------------
 * Application state
 * -------------------------------------------------------------------------- */
static volatile bool rx_done = false;

/* Radio parameters (match the Arduino Master sketch) */
static const lora_p2p_config_t master_cfg = {
    .frequency        = 868000000U,
    .spreading_factor = 7,
    .bandwidth        = LORA_BW_125,   /* bw = 0 in Arduino code */
    .coding_rate      = LORA_CR_4_5,   /* cr = 0 → 4/5 */
    .preamble_len     = 10,
    .tx_power         = 14,            /* dBm – match RUI3 Master & Slave */
};

/* --------------------------------------------------------------------------
 * Callbacks
 * -------------------------------------------------------------------------- */

/**
 * RX callback – invoked from radio IRQ context.
 */
static void on_rx(lora_p2p_rx_data_t data)
{
    rx_done = true;

    if (data.buffer_size == 0) {
        debug_println("Empty buffer.");
        return;
    }

    debug_printf("Incoming message, length: %d, RSSI: %d, SNR: %d\r\n",
                 data.buffer_size, data.rssi, data.snr);
    debug_hex_dump(data.buffer, data.buffer_size);
}

/**
 * TX-done callback – after sending, go back to continuous RX.
 */
static void on_tx_done(void)
{
    bool ok = lora_p2p_receive(65534);
    debug_printf("P2P set Rx mode %s\r\n", ok ? "Success" : "Fail");
}

/**
 * RX timeout – just re-enter RX.
 */
static void on_rx_timeout(void)
{
    debug_println("RX timeout – restarting RX");
    lora_p2p_receive(65534);
}

/* --------------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------------- */
int main(void)
{
    /* ---- HAL & peripheral init ---- */
    hw_init();

    /* ---- Banner ---- */
    debug_println("RAKwireless LoRa P2P – MASTER (plain C / Keil)");
    debug_println("------------------------------------------------------");
    HAL_Delay(2000);

    debug_println("P2P Start");
    debug_printf("Frequency    : %lu Hz\r\n", (unsigned long)master_cfg.frequency);
    debug_printf("SF           : %u\r\n",     master_cfg.spreading_factor);
    debug_printf("BW           : %u (0=125,1=250,2=500 kHz)\r\n", master_cfg.bandwidth);
    debug_printf("CR           : 4/%u\r\n",   master_cfg.coding_rate + 4);
    debug_printf("Preamble     : %u\r\n",     master_cfg.preamble_len);
    debug_printf("TX power     : %d dBm\r\n", master_cfg.tx_power);

    /* ---- Radio init ---- */
    lora_p2p_register_rx_callback(on_rx);
    lora_p2p_register_tx_callback(on_tx_done);
    lora_p2p_register_rx_timeout_callback(on_rx_timeout);

    if (!lora_p2p_init(&master_cfg)) {
        debug_println("ERROR: lora_p2p_init failed!");
        while (1) { __NOP(); }
    }

    /* Kick-start: listen for 3 seconds */
    debug_printf("P2P set Rx mode %s\r\n",
                 lora_p2p_receive(3000) ? "Success" : "Fail");

    debug_println(" ++++++++++++++++   MASTER   ++++++++++++++++++++++++");

    /* ---- Super-loop ---- */
    const uint8_t payload[] = MASTER_PAYLOAD;
    const uint8_t payload_len = sizeof(payload) - 1; /* exclude NUL */

    while (1) {
        /* Process pending radio IRQs (dispatches callbacks above) */
        lora_p2p_irq_process();

        /*
         * The original Arduino Master used a blocking while(1) that sent
         * every 5 s regardless of rx_done. We replicate that behaviour
         * with a non-blocking tick approach so the IRQ process loop keeps
         * running.
         */
        static uint32_t last_tx_tick = 0;
        uint32_t now = HAL_GetTick();

        if (now - last_tx_tick >= 5000U) {
            last_tx_tick = now;

            debug_println("  ------  L O O P -----");
            debug_println("             tx cycle");

            /* Switch to standby (stop RX), then transmit */
            lora_p2p_standby();
            debug_println("Set P2P to Tx mode Success");

            bool ok = lora_p2p_send(payload, payload_len);
            debug_printf("P2P send %s\r\n", ok ? "Success" : "Fail");
            /* on_tx_done() will re-enter RX automatically */
        }
    }
}

/* --------------------------------------------------------------------------
 * SubGHz radio IRQ handler (defined weak in startup_stm32wle5xx.s)
 * -------------------------------------------------------------------------- */
void SUBGHZ_Radio_IRQHandler(void)
{
    lora_p2p_irq_process();
}
