# LoRa P2P Master / Slave — Plain C Port for STM32WL (Keil µVision)

Ported from the Arduino/RUI3 sketches in this repo, which originate from  
<https://github.com/Kongduino/RUI3_LoRa_P2P_PING_PONG>

---

## Comparison: Original vs Your Arduino Code vs This C Port

| Aspect | Original (Kongduino) | Your Arduino Master | Your Arduino Slave | This C Port |
|---|---|---|---|---|
| **API** | `api.lorawan.*` (newer RUI3) | `api.lora.*` (older RUI3) | `api.lora.*` | STM32 SubGHz_Phy `Radio.*` |
| **Frequency** | 868.125 MHz | 868.000 MHz | 868.000 MHz | 868.000 MHz |
| **SF** | 12 | 12 | 7 ⚠️ | 12 (both) |
| **BW** | 0 (125 kHz) | 0 (125 kHz) | 4 ⚠️ | 0 (125 kHz) |
| **CR** | 0 (4/5) | 0 (4/5) | 1 ⚠️ | 1 (4/5) |
| **TX Power** | 22 dBm | var=10 but `set(22)` | 22 dBm | 22 dBm |
| **TX payload** | `"payload #XXXX"` (counter) | `"508012"` (fixed) | `"It's me"` / `"It's not me"` | Same as your Arduino |
| **RX logic** | Ping-pong on rx_done | Ignores rx_done, sends every 5 s | Serial-number match | Same as your Arduino |
| **Role split** | Single file, both roles | Separate Master file | Separate Slave file | Separate `main_master.c` / `main_slave.c` |

> ⚠️ **Bug in original Arduino code:** The Slave uses SF=7, BW=4, CR=1 while the Master uses SF=12, BW=0, CR=0. These are **incompatible** — both ends must use the same LoRa modulation settings. The C port defaults both to SF=12/BW=125kHz/CR=4\/5.

---

## File Structure

```
keil_c_port/
├── Inc/
│   ├── app_config.h        # Role selection (MASTER vs SLAVE), serial numbers
│   ├── lora_p2p.h          # Radio abstraction API
│   └── uart_debug.h        # Debug printf over UART
├── Src/
│   ├── lora_p2p.c          # Radio driver (wraps SubGHz_Phy middleware)
│   ├── uart_debug.c        # UART implementation
│   ├── main_master.c       # Master application (include ONLY this OR slave)
│   └── main_slave.c        # Slave application
└── Doc/
    └── (this README)
```

---

## How to Set Up the Keil Project

### Step 1 — Generate base project with STM32CubeMX

1. Open STM32CubeMX, create a new project for **STM32WLE5CCU6** (the MCU inside RAK3172).
2. Enable peripherals:
   - **USART2**: Asynchronous, 115200 baud, 8N1 (debug output via ST-Link VCP)  
   - **SubGHz Radio**: Enable the radio peripheral
   - **RTC** (optional, for timestamping)
3. In **Middleware → SubGHz_Phy**: enable it. This adds the `Radio` driver.
4. Set **Project Manager → Toolchain** to **MDK-ARM** (Keil).
5. **Generate Code**.

### Step 2 — Add the ported source files

1. In Keil µVision, open the generated `.uvprojx`.
2. Add to **Source Group**:
   - `Src/lora_p2p.c`
   - `Src/uart_debug.c`
   - **Either** `Src/main_master.c` **or** `Src/main_slave.c` (not both — they both define `main()`).
3. Add `Inc/` to **Options → C/C++ → Include Paths**.
4. Remove or rename the CubeMX-generated `main.c` (it also defines `main()`).

### Step 3 — Build & flash

- Build with **Project → Build Target** (F7).
- Flash via **Debug → Start/Stop Debug Session** using the ST-Link connected to the RAK3172 SWD pins.

---

## Pin Mapping (RAK3172 / STM32WLE5CC)

| Function | STM32 Pin | RAK3172 Pin | Notes |
|---|---|---|---|
| UART2 TX | PA2 | Pin 1 (UART2_TX) | Debug output |
| UART2 RX | PA3 | Pin 2 (UART2_RX) | Debug input |
| SWDIO | PA13 | SWD header | Programming/debug |
| SWCLK | PA14 | SWD header | Programming/debug |
| SubGHz Radio | Internal | — | On-chip SX126x, no external SPI needed |

---

## How to Test

1. **Serial terminal test** (single board) — flash Master, open PuTTY/minicom at 115200 baud, verify startup banner and TX messages
2. **Two-board P2P test** — flash Master on one RAK3172, Slave on another, connect both UARTs to PC, verify bidirectional communication
3. **RF sniffing** — use an RTL-SDR tuned to 868 MHz with SDR# or GQRX to see LoRa chirp transmissions
4. **SWD debugging** — use Keil's built-in debugger with ST-Link for step-through on the MCU
5. **PC-based unit tests** — mock the Radio and HAL functions, compile with GCC, test application logic (serial-number matching, hex dump) without hardware

---

## Testing Strategy (detailed)

### 1. Unit-Level (on PC, without hardware)

You can test the **application logic** (serial-number matching, hex dump, payload formatting) on your development PC:

```bash
# Compile the logic-only parts with gcc (mock the HAL/Radio calls)
gcc -DUNIT_TEST -I Inc/ Src/uart_debug.c tests/test_logic.c -o test_logic
./test_logic
```

Create a `tests/test_logic.c` that:
- Calls `check_serial()` with known buffers and verifies `this_is_me`.
- Calls `debug_hex_dump()` and captures output.

### 2. Loopback Test (single board)

- Flash the **Master** firmware.
- Open a serial terminal (PuTTY, minicom, or Keil's built-in terminal) at 115200 baud.
- Verify you see the startup banner and periodic "P2P send Success" messages.
- The radio will actually transmit over the air — you can confirm with an SDR or second board.

### 3. Two-Board P2P Test (recommended)

| Board | Firmware | Expected behaviour |
|---|---|---|
| RAK3172 #1 | `main_master.c` | Sends `"508012"` every 5 s |
| RAK3172 #2 | `main_slave.c` | Receives, checks serial, responds `"It's me"` or `"It's not me"` |

**Steps:**
1. Flash Master on board #1, Slave on board #2.
2. Connect each board's UART to a PC (two USB-serial adapters or two terminals).
3. Power both boards.
4. **Master terminal** should show: `P2P send Success` → then incoming response from Slave.
5. **Slave terminal** should show: `recv_cb` → serial check → `P2P send` response.

### 4. Tools for LoRa RF Testing

| Tool | Purpose | Notes |
|---|---|---|
| **Serial terminal** (PuTTY, minicom, Tera Term) | View debug output | 115200 baud, 8N1 |
| **STM32CubeProgrammer** | Flash firmware via SWD | Free from ST |
| **Keil µVision Debugger** | Step-through debugging | Use ST-Link |
| **Saleae Logic Analyzer** | Verify SPI/UART timing | Decode SPI to the SubGHz radio |
| **SDR dongle (RTL-SDR)** + **SDR#** or **GQRX** | Sniff LoRa RF transmissions | Tune to 868 MHz, you'll see the chirp spread spectrum |
| **LoRa field tester** (e.g., LilyGO T-Beam) | Independent verification | Run a simple LoRa receiver sketch |
| **Wireshark + gr-lora** (GNU Radio) | Decode LoRa packets off-the-air | Advanced: needs SDR + GNU Radio |
| **ST-Link + SWO/ITM** | Real-time trace without UART overhead | Faster than printf debugging |

### 5. Automated Regression (CI-friendly)

For continuous testing without hardware, create mock implementations of the HAL and Radio:

```c
// tests/mock_radio.c
#include "radio.h"
static RadioEvents_t *mock_events;
void Radio_Init(RadioEvents_t *events) { mock_events = events; }
void Radio_Send(uint8_t *buf, uint8_t len) { /* record call */ mock_events->TxDone(); }
void Radio_Rx(uint32_t timeout) { /* simulate rx after delay */ }
// ... etc
```

Then link against the mock instead of the real driver and run your application logic in a test harness on the PC.

---

## Quick Reference: Arduino API → C API Mapping

| Arduino (RUI3) | C (SubGHz_Phy / this port) |
|---|---|
| `Serial.begin(115200)` | `MX_USART2_UART_Init()` (CubeMX) |
| `Serial.printf(...)` | `debug_printf(...)` |
| `Serial.println(...)` | `debug_println(...)` |
| `delay(ms)` | `HAL_Delay(ms)` |
| `millis()` | `HAL_GetTick()` |
| `api.lora.pfreq.set(f)` | `Radio.SetChannel(f)` |
| `api.lora.psf.set(sf)` | Part of `Radio.SetTxConfig()` / `SetRxConfig()` |
| `api.lora.precv(timeout)` | `lora_p2p_receive(timeout)` → `Radio.Rx()` |
| `api.lora.precv(0)` | `lora_p2p_standby()` → `Radio.Standby()` |
| `api.lora.psend(len, buf)` | `lora_p2p_send(buf, len)` → `Radio.Send()` |
| `api.lora.registerPRecvCallback` | `lora_p2p_register_rx_callback()` |
| `api.lora.registerPSendCallback` | `lora_p2p_register_tx_callback()` |
