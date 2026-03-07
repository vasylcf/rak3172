# LoRa P2P Master / Slave — Plain C Port for STM32WL (Keil µVision)

Ported from the Arduino/RUI3 sketches in this repo, which originate from
<https://github.com/Kongduino/RUI3_LoRa_P2P_PING_PONG>

---

## Comparison: Original vs Your Arduino Code vs This C Port

| Aspect | Original (Kongduino) | Your Arduino Master | Your Arduino Slave | This C Port |
|---|---|---|---|---|
| **API** | `api.lorawan.*` (newer RUI3) | `api.lora.*` (older RUI3) | `api.lora.*` | HAL_SUBGHZ direct |
| **Frequency** | 868.125 MHz | 868.000 MHz | 868.000 MHz | 868.000 MHz |
| **SF** | 12 | 12 | 7 ⚠️ | 12 (both) |
| **BW** | 0 (125 kHz) | 0 (125 kHz) | 4 ⚠️ | 0 (125 kHz) |
| **CR** | 0 (4/5) | 0 (4/5) | 1 ⚠️ | 1 (4/5) |
| **TX Power** | 22 dBm | var=10 but `set(22)` | 22 dBm | 22 dBm |
| **TX payload** | `"payload #XXXX"` (counter) | `"508012"` (fixed) | `"It's me"` / `"It's not me"` | Same as your Arduino |
| **RX logic** | Ping-pong on rx_done | Ignores rx_done, sends every 5 s | Serial-number match | Same as your Arduino |
| **Role split** | Single file, both roles | Separate Master file | Separate Slave file | Separate `main_master.c` / `main_slave.c` |
| **Radio backend** | RUI3 internal | RUI3 internal | RUI3 internal | HAL_SUBGHZ direct, no middleware |

> ⚠️ **Bug in original Arduino code:** The Slave uses SF=7, BW=4, CR=1 while the Master uses SF=12, BW=0, CR=0. These are **incompatible** — both ends must use the same LoRa modulation settings. The C port defaults both to SF=12/BW=125kHz/CR=4\/5.

---

## File Structure

```
keil_c_port/
├── Inc/
│   ├── app_config.h            # Role selection (MASTER vs SLAVE), serial numbers
│   ├── hw_init.h               # hw_init() prototype and extern huart2
│   ├── lora_p2p.h              # Radio abstraction API
│   ├── uart_debug.h            # Debug printf over UART
│   └── stm32wlxx_hal_conf.h    # HAL configuration (from ST template)
├── Src/
│   ├── hw_init.c               # Clock (MSI 48 MHz), GPIO, USART2, Error_Handler
│   ├── lora_p2p.c              # LoRa P2P driver over HAL_SUBGHZ (no middleware)
│   ├── uart_debug.c            # UART implementation
│   ├── main_master.c           # Master application (include ONLY this OR slave)
│   └── main_slave.c            # Slave application
├── Drivers/                    # HAL & CMSIS from ST (downloaded separately, see BUILD_GUIDE.md)
│   ├── STM32WLxx_HAL_Driver/   # HAL drivers (subghz, uart, rcc, gpio...)
│   └── CMSIS/                  # Cortex-M4 Core + STM32WLE5 device headers + startup
│       └── Device/ST/STM32WLxx/
│           ├── startup_stm32wle5xx.s       # Startup for GCC (Makefile)
│           └── startup_stm32wle5xx_keil.s  # Startup for Keil (ARM Compiler 5/6)
├── Makefile                    # Build via arm-none-eabi-gcc (Linux/macOS alternative to Keil)
├── STM32WLE5XX_FLASH.ld        # GCC linker script
├── KEIL_SETUP.md               # Step-by-step Keil setup guide + debug checklist
├── BUILD_GUIDE.md              # Step-by-step GCC build guide for Linux
└── README.md                   # This file
```

---

## How to Set Up the Keil Project

> 📄 Full step-by-step guide with debug checklist: **[KEIL_SETUP.md](KEIL_SETUP.md)**

> **Verified:** Keil MDK v5.43a, ARM Compiler V6.24 — **0 errors** build.

### Option A — Standalone (no CubeMX, recommended)

1. Install **Keil MDK** and the **Keil::STM32WLxx_DFP** pack via Pack Installer.
2. **Project → New µVision Project** → select **STM32WLE5CCUx**.
3. Create 3 file groups (**Project → Manage → Project Items**):
   - **Application:** `hw_init.c`, `lora_p2p.c`, `uart_debug.c`, `main_master.c` or `main_slave.c`
   - **HAL_Driver:** 13 files from `Drivers/STM32WLxx_HAL_Driver/Src/` (hal, cortex, rcc, rcc_ex, gpio, uart, uart_ex, subghz, pwr, pwr_ex, dma, flash, flash_ex)
   - **CMSIS:** `system_stm32wlxx.c` + **`startup_stm32wle5xx_keil.s`** (not the GCC version!)
4. **Options (Alt+F7):**
   - **C/C++ → Define:** `STM32WLE5xx, USE_HAL_DRIVER`
   - **C/C++ → Include Paths:** `.\Inc`, `.\Drivers\STM32WLxx_HAL_Driver\Inc`, `.\Drivers\CMSIS\Device\ST\STM32WLxx\Include`, `.\Drivers\CMSIS\Core\Include`
   - **Linker:** check "Use Memory Layout from Target Dialog"
5. **F7** — build.

> ⚠️ **Two startup files:** `startup_stm32wle5xx.s` is for GCC, `startup_stm32wle5xx_keil.s` is for Keil.
> In Keil, use **only** `_keil.s` — the GCC version causes linker errors.

### Option B — CubeMX project (if you already have one)

1. Open the generated `.uvprojx` in Keil.
2. Add `lora_p2p.c`, `uart_debug.c`, and one of `main_*.c`.
3. **`hw_init.c` is not needed** — CubeMX generates its own init.
4. Remove or rename the CubeMX `main.c` (it also defines `main()`).
5. Make sure SubGHz_Phy middleware is **not** included.

### Build & Flash

- Build: **F7**.
- For `.hex` output: **Options → Output → Create HEX File** ✓
- Flash via **Debug → Start/Stop Debug Session** (ST-Link on RAK3172 SWD pins).

| Function | STM32 Pin | RAK3172 Pin | Notes |
|---|---|---|---|
| UART2 TX | PA2 | Pin 1 (UART2_TX) | Debug output |
| UART2 RX | PA3 | Pin 2 (UART2_RX) | Debug input |
| SWDIO | PA13 | SWD header | Programming/debug |
| SWCLK | PA14 | SWD header | Programming/debug |
| SubGHz Radio | Internal | — | On-chip SX126x, no external SPI needed |

---

## Build Verification

The code has been verified on **two** toolchains:

| Toolchain | OS | Result |
|---|---|---|
| `arm-none-eabi-gcc 13.2.1` | Linux (Ubuntu) | **0 errors, 0 warnings** |
| ARM Compiler V6.24 (Keil MDK v5.43a) | Windows | **0 errors, 1 cosmetic warning** |

During verification, **3 bugs** in `lora_p2p.c` were found and fixed (incompatible names with current HAL version):

| Was (wrong) | Now (correct) |
|---|---|
| `RadioSetCmd_TypeDef` | `SUBGHZ_RadioSetCmd_t` |
| `RADIO_SET_DIO2ASRFSWITCH` | `RADIO_SET_RFSWITCHMODE` |
| `RADIO_SET_DIOIRQPARAMS` | `RADIO_CFG_DIOIRQ` |

### Building without Keil (Linux/macOS)

If Keil is not available, you can verify compilation with GCC:

```bash
# Install (Ubuntu/Debian)
sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi

# Build
cd keil_c_port
make master   # Build Master (build/master/master_firmware.bin)
make slave    # Build Slave  (build/slave/slave_firmware.bin)
make all      # Build both
make clean    # Clean
```

> Full instructions: **[BUILD_GUIDE.md](BUILD_GUIDE.md)**

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

For continuous testing without hardware, create a mock implementation of `lora_p2p`:

```c
// tests/mock_lora.c
#include "lora_p2p.h"

static lora_p2p_rx_cb_t mock_rx_cb = NULL;
static lora_p2p_tx_cb_t mock_tx_cb = NULL;

bool lora_p2p_init(const lora_p2p_config_t *cfg) { return true; }
void lora_p2p_register_rx_callback(lora_p2p_rx_cb_t cb)  { mock_rx_cb = cb; }
void lora_p2p_register_tx_callback(lora_p2p_tx_cb_t cb)  { mock_tx_cb = cb; }
bool lora_p2p_receive(uint32_t t)  { return true; }
bool lora_p2p_standby(void)        { return true; }
bool lora_p2p_send(const uint8_t *d, uint8_t l) {
    if (mock_tx_cb) mock_tx_cb();  /* simulate TX Done */
    return true;
}
void lora_p2p_irq_process(void) {}

/* Inject a simulated incoming packet from a test */
void simulate_rx(const uint8_t *buf, uint8_t len, int16_t rssi, int8_t snr) {
    if (mock_rx_cb) {
        lora_p2p_rx_data_t d = { .buffer=buf, .buffer_size=len, .rssi=rssi, .snr=snr };
        mock_rx_cb(d);
    }
}
```

---

## Quick Reference: Arduino API → C API Mapping

| Arduino (RUI3) | C (this port) |
|---|---|
| `Serial.begin(115200)` | `hw_init()` or `MX_USART2_UART_Init()` (CubeMX) |
| `Serial.printf(...)` | `debug_printf(...)` |
| `Serial.println(...)` | `debug_println(...)` |
| `delay(ms)` | `HAL_Delay(ms)` |
| `millis()` | `HAL_GetTick()` |
| `api.lora.pfreq.set(f)` | `cfg.frequency` field in `lora_p2p_config_t` |
| `api.lora.psf.set(sf)` | `cfg.spreading_factor` field |
| `api.lora.pbw.set(bw)` | `cfg.bandwidth` field |
| `api.lora.pcr.set(cr)` | `cfg.coding_rate` field |
| `api.lora.ptp.set(pwr)` | `cfg.tx_power` field |
| `api.lora.precv(timeout)` | `lora_p2p_receive(timeout)` |
| `api.lora.precv(0)` | `lora_p2p_standby()` |
| `api.lora.psend(len, buf)` | `lora_p2p_send(buf, len)` |
| `api.lora.registerPRecvCallback` | `lora_p2p_register_rx_callback()` |
| `api.lora.registerPSendCallback` | `lora_p2p_register_tx_callback()` |
