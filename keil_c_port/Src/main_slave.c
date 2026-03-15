/**
 * @file    main_slave.c
 * @brief   LoRa P2P Slave – plain C port for STM32WL / Keil
 *
 * Ported from: RAK3172_P2P_Slave.ino
 * Original:    https://github.com/Kongduino/RUI3_LoRa_P2P_PING_PONG
 *
 * Behaviour
 * ---------
 *  - Listens continuously for incoming LoRa packets.
 *  - When a packet arrives, compares the first 6 bytes against
 *    SLAVE_SERIAL_NUMBER ("508011").
 *  - If it matches → sets this_is_me = true, responds with "It's me".
 *  - If it doesn't  → sets this_is_me = false, responds with "It's not me".
 *
 * Build
 * -----
 *  Compile ONLY this file (not main_master.c) when building the Slave
 *  firmware. Both files define main(), so include exactly one.
 *
 * NOTE on radio parameters
 * ------------------------
 *  The original Arduino Slave used SF=7, BW=4, CR=1, while the Master
 *  used SF=12, BW=0, CR=0. Those two devices would NOT be able to
 *  communicate because LoRa requires matching SF/BW/CR on both ends.
 *  This C port defaults to SF=12, BW=125kHz, CR=4/5 to match the Master.
 *  If you intentionally want different settings, change slave_cfg below.
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
static volatile bool rx_done   = false;
static volatile bool this_is_me = false;

static const char serial_number[SLAVE_SERIAL_LEN + 1] = SLAVE_SERIAL_NUMBER;

/* Radio parameters – MUST match the Master for successful communication */
static const lora_p2p_config_t slave_cfg = {
    .frequency        = 868000000U,
    .spreading_factor = 7,
    .bandwidth        = LORA_BW_125,
    .coding_rate      = LORA_CR_4_5,
    .preamble_len     = 10,
    .tx_power         = 14,
};

/* --------------------------------------------------------------------------
 * Serial-number check (replaces the Arduino hexDump-with-compare)
 * -------------------------------------------------------------------------- */
static void check_serial(const uint8_t *buf, uint16_t len)
{
    char s[SLAVE_SERIAL_LEN + 1] = {0};

    debug_println("==  begin ==");

    /* Copy first SLAVE_SERIAL_LEN bytes as characters */
    for (uint16_t i = 0; i < SLAVE_SERIAL_LEN && i < len; i++) {
        s[i] = (char)buf[i];
        debug_printf("%02X", buf[i]);
    }
    debug_println("");
    debug_println("==  begin 1 ==");
    debug_println("%s", s);
    debug_println("==  begin 2 ==");

    if (strncmp(serial_number, s, SLAVE_SERIAL_LEN) == 0) {
        debug_println("== OK ==");
        this_is_me = true;
    } else {
        debug_println("=NO==");
        this_is_me = false;
    }
}

/* --------------------------------------------------------------------------
 * Callbacks
 * -------------------------------------------------------------------------- */

static void on_rx(lora_p2p_rx_data_t data)
{
    rx_done = true;
    debug_printf("      -----  recv_cb  sz=%u rssi=%d snr=%d  -----\r\n",
                 data.buffer_size, data.rssi, data.snr);

    if (data.buffer_size == 0) {
        debug_println("Empty buffer.");
        return;
    }

    debug_printf("Incoming message, length: %d, RSSI: %d, SNR: %d\r\n",
                 data.buffer_size, data.rssi, data.snr);
    debug_printf("serial number = %s\r\n", serial_number);

    check_serial(data.buffer, data.buffer_size);
}

static void on_tx_done(void)
{
    bool ok = lora_p2p_receive(65534);
    debug_printf("P2P set Rx mode %s\r\n", ok ? "Success" : "Fail");
}

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
    debug_println("BOOT:1");

    /* ---- Banner ---- */
    debug_println("RAKwireless LoRa P2P - SLAVE (plain C / Keil) v8-XTAL");
    debug_println("------------------------------------------------------");
    debug_println("BOOT:2");

    HAL_Delay(2000);
    debug_println("BOOT:3");

    debug_println("P2P Start");
    debug_printf("Frequency    : %lu Hz\r\n", (unsigned long)slave_cfg.frequency);
    debug_printf("SF           : %u\r\n",     slave_cfg.spreading_factor);
    debug_printf("BW           : %u (0=125,1=250,2=500 kHz)\r\n", slave_cfg.bandwidth);
    debug_printf("CR           : 4/%u\r\n",   slave_cfg.coding_rate + 4);
    debug_printf("Preamble     : %u\r\n",     slave_cfg.preamble_len);
    debug_printf("TX power     : %d dBm\r\n", slave_cfg.tx_power);
    debug_println("BOOT:4");

    debug_println("  --------------   SLAVE  --------------");

    /* ---- Radio init ---- */
    lora_p2p_register_rx_callback(on_rx);
    lora_p2p_register_tx_callback(on_tx_done);
    lora_p2p_register_rx_timeout_callback(on_rx_timeout);

    debug_println("BOOT:5");
    if (!lora_p2p_init(&slave_cfg)) {
        debug_println("ERROR: lora_p2p_init failed!");
        while (1) { __NOP(); }
    }
    debug_println("BOOT:6");

    /* Start receiving (3 s initial timeout, then continuous via callbacks) */
    debug_printf("P2P set Rx mode %s\r\n",
                 lora_p2p_receive(3000) ? "Success" : "Fail");
    debug_println("BOOT:7");

    /* ---- Super-loop ---- */
    while (1) {
        lora_p2p_irq_process();

        if (rx_done) {
            rx_done = false;

            /* Choose response payload based on serial number match */
            const uint8_t *payload;
            uint8_t payload_len;

            if (this_is_me) {
                static const uint8_t resp_me[]     = "It's me";
                payload     = resp_me;
                payload_len = sizeof(resp_me) - 1;
            } else {
                static const uint8_t resp_not_me[] = "It's not me";
                payload     = resp_not_me;
                payload_len = sizeof(resp_not_me) - 1;
            }

            HAL_Delay(1000);

            lora_p2p_standby();
            debug_println("Set P2P to Tx mode Success");

            bool ok = lora_p2p_send(payload, payload_len);
            debug_printf("P2P send – %s\r\n", ok ? "Success" : "Fail");

            if (this_is_me) {
                debug_println("This is me");
            } else {
                debug_println("This not me");
            }
            /* on_tx_done() will re-enter RX */
        }

        HAL_Delay(500);
    }
}

/* --------------------------------------------------------------------------
 * SubGHz radio IRQ handler
 * -------------------------------------------------------------------------- */
void SUBGHZ_Radio_IRQHandler(void)
{
    lora_p2p_irq_process();
}
