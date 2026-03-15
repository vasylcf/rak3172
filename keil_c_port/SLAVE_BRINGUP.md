# RAK3172 Slave Firmware Bringup — Step-by-Step Log

## Summary

Successfully ported the RAK3172 P2P Slave firmware from Arduino/RUI3 to plain C (Keil MDK).
The firmware initializes the SX126x radio via direct `HAL_SUBGHZ` SPI calls and enters
continuous LoRa RX mode on 868 MHz.

**Final working version:** `v8-XTAL`

---

## Hardware

- **Module:** RAK3172 (base model — **XTAL**, not RAK3172-T)
- **MCU:** STM32WLE5CC (Cortex-M4 + integrated SX126x)
- **Crystal:** Standard 32 MHz XTAL on XTA/XTB pins (no TCXO)
- **Debug UART:** PA2 (TX) / PA3 (RX), 115200 8N1, via USB-UART adapter
- **Flashing:** STM32 ROM bootloader via UART (BOOT pin to VDD, then STM32CubeProgrammer)
- **IDE:** Keil MDK v5.43a, ARM Compiler V6.24
- **Master device:** Second RAK3172 running Arduino/RUI3 firmware (confirmed working)

## Radio Parameters

Initially configured for SF12 (matching original Arduino sketch), then changed to match
the actual Master device firmware:

| Parameter | Slave (initial) | Slave (current) | Master (actual) |
|-----------|----------------|-----------------|-----------------|
| Frequency | 868 MHz | 868 MHz | 868 MHz |
| SF | 12 | **7** | **7** |
| BW | 125 kHz | 125 kHz | 125 kHz |
| CR | 4/5 | 4/5 | 4/5 |
| Preamble | 8 | **10** | **10** |
| TX Power | 22 dBm | **14 dBm** | **14 dBm** |

---

## Issues Encountered and Fixed (chronological)

### Issue 1: Wrong source file in Keil project (v1)
- **Symptom:** Only "RAK" partial output, then hang
- **Root cause:** `rak-prj.uvprojx` included `main_master.c` instead of `main_slave.c`
- **Fix:** Edited `.uvprojx` XML to reference `main_slave.c`

### Issue 2: SysTick hang (v1)
- **Symptom:** Output truncated after ~11 bytes (`BOOT:1\nRAK`)
- **Root cause:** No `SysTick_Handler` defined. The default weak handler in the startup
  assembly is `B .` (infinite loop). HAL_Init() enables SysTick, and ~1 ms later the ISR
  fires and the CPU hangs forever.
- **Fix:** Added `SysTick_Handler()` calling `HAL_IncTick()` to `hw_init.c`
- **Evidence:** Linker map showed `HAL_IncTick` was removed as unused (no reference to it)

### Issue 3: TCXO error 0x0020 — XoscStartErr (v2–v7)
- **Symptom:** All BOOT markers pass, but radio calibration fails with `err=0x0020` at
  every TCXO voltage (0x01–0x07). Status stays `st=0xAA` (command processing error).
- **Attempts that did NOT work:**
  - TCXO voltage 0x01 → 0x06 → auto-sweep all 0x01–0x07
  - `SET_BIT(RCC->CR, RCC_CR_HSEBYPPWR)` — confirmed bit was set, still 0x0020
  - `SET_BIT(RCC->CR, RCC_CR_HSEBYPPWR | RCC_CR_HSEON)` — still 0x0020
  - XTA/XTB trim registers set to 0x00
  - ClearDeviceErrors before calibration
  - Extended TCXO timeout to 500 ms
  - Raw XTAL fallback mode (v7 diagnostic)
- **Root cause:** **The RAK3172 base model has a standard XTAL crystal, NOT a TCXO.**
  The `SetTcxoMode` command (0x97) tells the radio to expect a clock signal from an
  external TCXO on DIO3/VDDTCXO. Since there is no TCXO, the radio waits forever for
  a clock that never comes, and reports XoscStartErr.
- **How we discovered it:** Photo of the module label shows "RAK3172" without the "-T"
  suffix. RAK3172 = XTAL, RAK3172-T = TCXO. Confirmed by RAK3172 datasheet.
- **Fix (v8-XTAL):**
  1. **Removed `SetTcxoMode` entirely** — the function `sx_set_tcxo()` was deleted
  2. **Removed `HSEBYPPWR` bit** from `SystemClock_Config()` — no TCXO supply path needed
  3. **Removed TCXO constants** — `LORA_TCXO_VOLTAGE`, `TCXO_TO_*` defines deleted
  4. **Left XTAL trim registers at defaults** — do NOT zero them (they're needed for XTAL)
  5. **Simplified init sequence:** `STDBY_RC → Calibrate(0x7F) → DCDC → ...`

### En-dash encoding issue (v1)
- **Symptom:** UART output garbled mid-string
- **Root cause:** C source contained UTF-8 en-dash `–` in string literal; UART terminal
  showed garbage bytes
- **Fix:** Replaced with ASCII hyphen `-`

### Issue 4: RX buffer always empty — uint8_t overflow (v8-XTAL)
- **Symptom:** `plen=10` from radio, but `copylen=0`, `buffer_size=0` in callback.
  Slave received packets (IRQ=0x0002) but always reported "Empty buffer."
- **Root cause:** `RX_BUF_SIZE` was defined as `256U`. The comparison
  `(plen > (uint8_t)RX_BUF_SIZE)` cast 256 to `uint8_t`, which wraps to **0**.
  So `(10 > 0)` was always true, and `copylen = (uint8_t)256 = 0`.
- **Fix:** Changed `RX_BUF_SIZE` from `256U` to `255U`

### Issue 5: SF/Preamble mismatch between Master and Slave
- **Symptom:** Slave in RX mode (`st=0xD2`) but never receiving packets from Master
- **Root cause:** Master (RUI3 firmware) was configured with SF=7, Preamble=10, TxPower=14.
  Slave was configured with SF=12, Preamble=8, TxPower=22.
  LoRa with different SF values cannot communicate — they are orthogonal modulations.
- **Fix:** Changed `slave_cfg` to match Master: SF=7, Preamble=10, TxPower=14

---

## Files Modified

| File | Changes |
|------|---------|
| `Src/hw_init.c` | Added `SysTick_Handler()`. Removed `HSEBYPPWR` from `SystemClock_Config()` |
| `Src/lora_p2p.c` | Removed all TCXO code. Clean XTAL init. Fixed RX_BUF_SIZE 256→255. Added RX diag prints |
| `Src/main_slave.c` | Added BOOT markers. Fixed en-dash. Version tag `v8-XTAL`. SF=7/Preamble=10/TxPower=14 |
| `Inc/app_config.h` | `APP_ROLE_SLAVE=1`, `SLAVE_SERIAL_NUMBER="508011"` |
| `rak-prj.uvprojx` | Changed `main_master.c` → `main_slave.c` |

---

## v8-XTAL Init Sequence (final working version)

```
STDBY_RC → Calibrate(0x7F) → SetRegulatorMode(DCDC) → SetPacketType(LoRa)
→ SetRfFrequency(868MHz) → CalibrateImage → SetPaConfig(HP)
→ SetTxParams(22dBm) → SetModulationParams(SF12/BW125/CR4_5/LDRO)
→ SetPacketParams → SetBufferBaseAddress → SetDio2AsRfSwitch
→ ConfigDioIrq → SetRx(continuous)
```

No `SetTcxoMode`. No `HSEBYPPWR`. No trim register manipulation.

---

## Successful Boot Log (v8-XTAL)

```
BOOT:1
RAKwireless LoRa P2P - SLAVE (plain C / Keil) v8-XTAL
------------------------------------------------------
BOOT:2
BOOT:3
P2P Start
Frequency    : 868000000 Hz
SF           : 12
BW           : 0 (0=125,1=250,2=500 kHz)
CR           : 4/5
Preamble     : 8
TX power     : 22 dBm
BOOT:4
  --------------   SLAVE  --------------
BOOT:5
[LORA] HAL_SUBGHZ_Init OK
[LORA] NVIC enabled for SUBGHZ_Radio_IRQn
[LORA] STANDBY_RC          st=0xA2  err=0x0000
[LORA] CALIBRATE_ALL       st=0xA2  err=0x0000
[LORA] SET_DCDC            st=0xA2  err=0x0000
[LORA] CAL_IMAGE+FREQ      st=0xA2  err=0x0000
[LORA] INIT_DONE           st=0xA2  err=0x0000
BOOT:6
[LORA] START_RX            st=0xD2  err=0x0000
P2P set Rx mode Success
BOOT:7
!![LORA] IRQ=0x0200
RX timeout – restarting RX
[LORA] START_RX            st=0xD2  err=0x0000
[LORA] IRQ=0x0000
[LORA] POLL irq=0x0000 mode=0xD2 pend=0
[LORA] POLL irq=0x0000 mode=0xD2 pend=0
[LORA] POLL irq=0x0000 mode=0xD2 pend=0
```

### Status decode
- `st=0xA2` → chipMode=010 (STBY_RC), cmdStatus=001 (success)
- `st=0xD2` → chipMode=101 (RX), cmdStatus=001 (success)
- `err=0x0000` → no device errors
- `IRQ=0x0200` → RX timeout (expected after 3s initial timeout, then continuous RX)
- `mode=0xD2` → radio in RX mode, listening

---

## Key Lesson Learned

**Always verify the hardware variant before writing radio init code.**

RAK3172 exists in two variants:
- **RAK3172** (base) — 32 MHz XTAL crystal → do NOT call `SetTcxoMode`
- **RAK3172-T** — TCXO on DIO3/VDDTCXO → must call `SetTcxoMode` + set `HSEBYPPWR`

The error `0x0020` (XoscStartErr) after `SetTcxoMode` is a definitive indicator that
the module does NOT have a TCXO.

---

## Next Steps

- [x] Test packet reception from Master device
- [x] Verify bidirectional communication (Slave responds to Master)
- [ ] Remove BOOT markers and debug prints for production
- [ ] Build and test Master firmware with same XTAL fix
- [ ] Test "It's me" response (Master must send "508011")

---

## Current Status (2026-03-15)

### Bidirectional P2P Communication — CONFIRMED WORKING

**What works:**
- Slave initialization (XTAL mode, all `err=0x0000`)
- Packet reception from Master (IRQ=0x0002, plen=10, RSSI=-61, SNR=12)
- Payload decoding (receives `"000006"`, `"000007"`, ... — Master sends counter)
- Serial number comparison (correctly reports `=NO==` since `"00000X"` ≠ `"508011"`)
- Response transmission `"It's not me"` (IRQ=0x0001 = TX_DONE)
- Automatic return to RX after TX (continuous operation)

**Note:** For the "It's me" response to trigger, the Master must send `"508011"`
(the Slave's serial number). The current Master firmware sends an incrementing counter
`"00000X"` instead of the expected serial number.

### Working P2P Communication Log

```
!![LORA] IRQ=0x0002
[LORA] RX plen=10 offset=0
[LORA] RX buf[0..3]=30 30 30 30 copylen=10
      -----  recv_cb  sz=10 rssi=-62 snr=12  -----
Incoming message, length: 10, RSSI: -62, SNR: 12
serial number = 508011
==  begin ==
303030303036==  begin 1 ==
000006
==  begin 2 ==
=NO==
Set P2P to Tx mode Success
P2P send – Success
This not me
![LORA] IRQ=0x0001
[LORA] START_RX            st=0xD2  err=0x0000
P2P set Rx mode Success
```
