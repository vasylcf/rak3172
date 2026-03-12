# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Embedded firmware for **RAK3172** (STM32WLE5CC + integrated SX126x LoRa radio) implementing point-to-point LoRa communication. Ported from Arduino/RUI3 sketches to plain C using direct HAL_SUBGHZ calls — **no LoRa middleware** (SubGHz_Phy is intentionally not used). Firmware size is ~14 KB.

## Build Commands

### GCC (Linux/macOS — arm-none-eabi-gcc)
```bash
cd keil_c_port
make master        # Build master firmware → build/master/master_firmware.bin
make slave         # Build slave firmware → build/slave/slave_firmware.bin
make all           # Build both
make clean         # Remove build artifacts
```
Required toolchain: `arm-none-eabi-gcc 13.2.1` (or compatible).

### Keil MDK (Windows — primary)
Open `keil_c_port/rak-prj.uvprojx` in Keil µVision v5.43a. Build via Project → Build Target (F7). Uses ARM Compiler V6.24.

### Compiler defines (both toolchains)
`STM32WLE5xx`, `USE_HAL_DRIVER`. Role selected via `APP_ROLE_MASTER` / `APP_ROLE_SLAVE` (in `app_config.h` or `-D` flag).

## Architecture

All application code lives in `keil_c_port/`. The root `.ino` files are the original Arduino sources (reference only).

### Application layer (`Src/` + `Inc/`)
- **`app_config.h`** — Build-time role selection (Master/Slave) and serial number constants
- **`main_master.c`** — Master entry point: sends payload every 5s, listens for responses
- **`main_slave.c`** — Slave entry point: listens continuously, checks serial number match, responds
- **`lora_p2p.c` / `.h`** — Core LoRa P2P driver using direct `HAL_SUBGHZ_ExecSetCmd`/`GetCmd` SPI calls to configure and operate the SX126x radio. Manages TX, RX, standby, IRQ processing, and callback dispatch
- **`hw_init.c` / `.h`** — Hardware init: MSI 48 MHz clock, USART2 (PA2/PA3, 115200 baud), GPIO
- **`uart_debug.c` / `.h`** — Blocking debug printf/println/hex_dump over UART2

### Vendor drivers (`Drivers/`)
- **STM32WLxx_HAL_Driver** — ST HAL (selectively compiled: UART, SubGHz, RCC, GPIO, PWR, DMA, Flash, Cortex)
- **CMSIS** — Cortex-M4 core headers, STM32WLE5 device definitions, startup assembly

### Key radio parameters (868 MHz EU)
SF=12, BW=125 kHz, CR=4/5, TX power=22 dBm, preamble=8 symbols, TCXO on DIO3 (3.0V), DIO2 as RF switch.

## Critical Design Decisions

**IRQ handling pattern:** EXTI line 44 (radio DIO1) is **level-sensitive**. The ISR sets a flag and disables NVIC to prevent infinite re-entry. The main loop calls `lora_p2p_irq_process()` which reads/clears the radio IRQ via SPI, then re-enables NVIC. A polling fallback catches missed IRQs.

**No middleware:** The firmware talks to the SX126x directly through `HAL_SUBGHZ` SPI commands rather than using ST's SubGHz_Phy middleware. This keeps things small and debuggable but means all radio register sequences are hand-coded in `lora_p2p.c`.

## HAL API naming (common pitfall)
STM32WL HAL uses specific enum names that differ from SX126x datasheet names:
- `SUBGHZ_RadioSetCmd_t` (not `RadioSetCmd_TypeDef`)
- `RADIO_SET_RFSWITCHMODE` (not `RADIO_SET_DIO2ASRFSWITCH`)
- `RADIO_CFG_DIOIRQ` (not `RADIO_SET_DIOIRQPARAMS`)

## Testing

No automated test suite. Testing is manual:
1. Flash Master/Slave on two RAK3172 boards
2. Connect UART2 (PA2/PA3) to PC at 115200 baud
3. Verify bidirectional communication via serial terminal (PuTTY, minicom)

## Flashing
- **Keil:** Debug → Start/Stop Debug Session (ST-Link SWD)
- **STM32CubeProgrammer:** Load `.bin` at 0x08000000 via SWD
- **OpenOCD / stlink:** Command-line alternative
