# Master Firmware Bringup Log

## Overview

C-портированная Master прошивка для RAK3172 (STM32WLE5CC, XTAL модель).
Отправляет payload `"508011"` (серийный номер Slave) каждые 5 секунд по LoRa P2P, затем слушает ответ.

## Ветка

`master-keil` (от `tx-master`)

## Радиопараметры

| Параметр | Значение |
|----------|----------|
| Frequency | 868 000 000 Hz |
| SF | 7 |
| BW | 125 kHz |
| CR | 4/5 |
| Preamble | 10 |
| TX Power | 14 dBm |

Параметры синхронизированы со Slave и RUI3-Master.

## Изменения относительно Slave-ветки

### 1. Переключение роли (app_config.h)

```c
#define APP_ROLE_MASTER   1
#define APP_ROLE_SLAVE    0
#define MASTER_PAYLOAD    "508011"   // серийный номер Slave
```

### 2. Радиопараметры (main_master.c)

SF 12→7, Preamble 8→10, TxPower 22→14 — чтобы совпадать с RUI3-Master и Slave.

### 3. Keil проект (rak-prj.uvprojx)

`main_slave.c` → `main_master.c` в списке исходников.

### 4. Убран debug-спам (lora_p2p.c)

- **POLL спам**: печатался каждые ~10 вызовов `lora_p2p_irq_process()`. Заменён на таймер 5 секунд + печать только при обнаружении пропущенного IRQ.
- **IRQ=0x0000 спам**: `s_irq_pending` срабатывал ложно, печатая нулевой IRQ. Добавлен early return при `irq == 0`.
- **`!` при каждом ISR**: убран `debug_putchar('!')`.

## Коммиты

| Коммит | Описание |
|--------|----------|
| `fd5af04` | Switch to Master role, payload 508011, SF7, preamble 10, txPower 14 |
| `8dbc58f` | Reduce POLL debug spam — only print when IRQ missed, 5s timer |
| `a9d2901` | Skip IRQ=0x0000 spam — return early on spurious wake |

## Успешный лог

```
RAKwireless LoRa P2P – MASTER (plain C / Keil)
------------------------------------------------------
P2P Start
Frequency    : 868000000 Hz
SF           : 7
BW           : 0 (0=125,1=250,2=500 kHz)
CR           : 4/5
Preamble     : 10
TX power     : 14 dBm
[LORA] HAL_SUBGHZ_Init OK
[LORA] NVIC enabled for SUBGHZ_Radio_IRQn
[LORA] STANDBY_RC          st=0xA2  err=0x0000
[LORA] CALIBRATE_ALL       st=0xA2  err=0x0000
[LORA] SET_DCDC            st=0xA2  err=0x0000
[LORA] CAL_IMAGE+FREQ      st=0xA2  err=0x0000
[LORA] INIT_DONE           st=0xA2  err=0x0000
[LORA] START_RX            st=0xD2  err=0x0000
P2P set Rx mode Success
 ++++++++++++++++   MASTER   ++++++++++++++++++++++++
[LORA] IRQ=0x0200
RX timeout – restarting RX
[LORA] START_RX            st=0xD2  err=0x0000
  ------  L O O P -----
             tx cycle
Set P2P to Tx mode Success
P2P send Success
[LORA] IRQ=0x0001
[LORA] START_RX            st=0xD2  err=0x0000
P2P set Rx mode Success
  ------  L O O P -----
             tx cycle
Set P2P to Tx mode Success
P2P send Success
[LORA] IRQ=0x0001
[LORA] START_RX            st=0xD2  err=0x0000
P2P set Rx mode Success
```

### Расшифровка ключевых IRQ

| IRQ | Значение |
|-----|----------|
| `0x0001` | TX_DONE — пакет успешно отправлен |
| `0x0002` | RX_DONE — пакет принят |
| `0x0200` | RX_TIMEOUT — таймаут приёма |

## Следующие шаги

- [ ] Запустить Master + Slave одновременно и проверить полный цикл "It's me"
- [ ] Проверить приём ответа от Slave на стороне Master
- [ ] Убрать debug-принты для production
