# LoRa P2P Master / Slave — Порт на чистый C для STM32WL (Keil µVision)

Портировано из Arduino/RUI3 скетчей данного репозитория, оригинал:  
<https://github.com/Kongduino/RUI3_LoRa_P2P_PING_PONG>

---

## Сравнение: оригинал vs Arduino-код vs данный C-порт

| Аспект | Оригинал (Kongduino) | Arduino Master | Arduino Slave | Данный C-порт |
|---|---|---|---|---|
| **API** | `api.lorawan.*` (новый RUI3) | `api.lora.*` (старый RUI3) | `api.lora.*` | STM32 SubGHz_Phy `Radio.*` |
| **Frequency** | 868.125 MHz | 868.000 MHz | 868.000 MHz | 868.000 MHz |
| **SF** | 12 | 12 | 7 ⚠️ | 12 (оба) |
| **BW** | 0 (125 kHz) | 0 (125 kHz) | 4 ⚠️ | 0 (125 kHz) |
| **CR** | 0 (4/5) | 0 (4/5) | 1 ⚠️ | 1 (4/5) |
| **TX Power** | 22 dBm | var=10, но `set(22)` | 22 dBm | 22 dBm |
| **TX payload** | `"payload #XXXX"` (счётчик) | `"508012"` (фиксированный) | `"It's me"` / `"It's not me"` | Как в Arduino |
| **Логика RX** | Ping-pong по rx_done | Игнорирует rx_done, шлёт каждые 5 с | Проверка серийного номера | Как в Arduino |
| **Разделение ролей** | Один файл, обе роли | Отдельный файл Master | Отдельный файл Slave | Отдельные `main_master.c` / `main_slave.c` |

> ⚠️ **Баг в оригинальном Arduino-коде:** Slave использует SF=7, BW=4, CR=1, тогда как Master — SF=12, BW=0, CR=0. Эти настройки **несовместимы** — на обоих концах должны быть одинаковые параметры LoRa-модуляции. В C-порте оба устройства используют SF=12/BW=125kHz/CR=4\/5.

---

## Структура файлов

```
keil_c_port/
├── Inc/
│   ├── app_config.h        # Выбор роли (MASTER vs SLAVE), серийные номера
│   ├── lora_p2p.h          # API абстракции радио
│   └── uart_debug.h        # Debug printf через UART
├── Src/
│   ├── lora_p2p.c          # Драйвер радио (обёртка над SubGHz_Phy middleware)
│   ├── uart_debug.c        # Реализация UART
│   ├── main_master.c       # Приложение Master (включать ТОЛЬКО его ИЛИ slave)
│   └── main_slave.c        # Приложение Slave
└── Doc/
    └── (этот README)
```

---

## Настройка проекта в Keil

### Шаг 1 — Генерация базового проекта в STM32CubeMX

1. Откройте STM32CubeMX, создайте новый проект для **STM32WLE5CCU6** (MCU внутри RAK3172).
2. Включите периферию:
   - **USART2**: Asynchronous, 115200 baud, 8N1 (отладочный вывод через ST-Link VCP)  
   - **SubGHz Radio**: включите радио-периферию
   - **RTC** (опционально, для меток времени)
3. В **Middleware → SubGHz_Phy**: включите. Это добавит драйвер `Radio`.
4. Установите **Project Manager → Toolchain** в **MDK-ARM** (Keil).
5. **Generate Code**.

### Шаг 2 — Добавление файлов порта

1. В Keil µVision откройте сгенерированный `.uvprojx`.
2. Добавьте в **Source Group**:
   - `Src/lora_p2p.c`
   - `Src/uart_debug.c`
   - **Либо** `Src/main_master.c` **либо** `Src/main_slave.c` (не оба — оба определяют `main()`).
3. Добавьте `Inc/` в **Options → C/C++ → Include Paths**.
4. Удалите или переименуйте сгенерированный CubeMX файл `main.c` (он тоже определяет `main()`).

### Шаг 3 — Сборка и прошивка

- Соберите проект: **Project → Build Target** (F7).
- Прошейте через **Debug → Start/Stop Debug Session**, используя ST-Link, подключённый к SWD-пинам RAK3172.

---

## Распиновка (RAK3172 / STM32WLE5CC)

| Функция | STM32 Pin | RAK3172 Pin | Примечание |
|---|---|---|---|
| UART2 TX | PA2 | Pin 1 (UART2_TX) | Отладочный вывод |
| UART2 RX | PA3 | Pin 2 (UART2_RX) | Отладочный ввод |
| SWDIO | PA13 | SWD header | Программирование/отладка |
| SWCLK | PA14 | SWD header | Программирование/отладка |
| SubGHz Radio | Internal | — | Встроенный SX126x, внешний SPI не нужен |

---

## How to Test

1. **Serial terminal test** (одна плата) — прошейте Master, откройте PuTTY/minicom на 115200 baud, проверьте стартовый баннер и TX-сообщения
2. **Two-board P2P test** — прошейте Master на один RAK3172, Slave на другой, подключите оба UART к ПК, проверьте двустороннюю связь
3. **RF sniffing** — используйте RTL-SDR, настроенный на 868 MHz, с SDR# или GQRX, чтобы увидеть LoRa chirp-передачи
4. **SWD debugging** — используйте встроенный отладчик Keil с ST-Link для пошаговой отладки на MCU
5. **PC-based unit tests** — создайте mock для Radio и HAL функций, скомпилируйте через GCC, протестируйте логику приложения (проверка серийного номера, hex dump) без железа

---

## Стратегия тестирования (подробно)

### 1. Модульное тестирование (на ПК, без железа)

Можно протестировать **логику приложения** (проверка серийного номера, hex dump, формирование payload) на рабочем ПК:

```bash
# Компиляция только логических частей через gcc (mock HAL/Radio вызовов)
gcc -DUNIT_TEST -I Inc/ Src/uart_debug.c tests/test_logic.c -o test_logic
./test_logic
```

Создайте файл `tests/test_logic.c`, который:
- Вызывает `check_serial()` с известными буферами и проверяет `this_is_me`.
- Вызывает `debug_hex_dump()` и захватывает вывод.

### 2. Тест на одной плате (Loopback)

- Прошейте прошивку **Master**.
- Откройте серийный терминал (PuTTY, minicom или встроенный терминал Keil) на 115200 baud.
- Убедитесь, что вы видите стартовый баннер и периодические сообщения "P2P send Success".
- Радио действительно передаёт в эфир — это можно подтвердить с помощью SDR или второй платы.

### 3. Тест двух плат P2P (рекомендуется)

| Плата | Прошивка | Ожидаемое поведение |
|---|---|---|
| RAK3172 #1 | `main_master.c` | Отправляет `"508012"` каждые 5 с |
| RAK3172 #2 | `main_slave.c` | Принимает, проверяет серийный номер, отвечает `"It's me"` или `"It's not me"` |

**Порядок действий:**
1. Прошейте Master на плату #1, Slave на плату #2.
2. Подключите UART каждой платы к ПК (два USB-serial адаптера или два терминала).
3. Подайте питание на обе платы.
4. **Терминал Master** должен показать: `P2P send Success` → затем входящий ответ от Slave.
5. **Терминал Slave** должен показать: `recv_cb` → проверка серийного номера → `P2P send` ответ.

### 4. Инструменты для тестирования LoRa RF

| Инструмент | Назначение | Примечание |
|---|---|---|
| **Serial terminal** (PuTTY, minicom, Tera Term) | Просмотр отладочного вывода | 115200 baud, 8N1 |
| **STM32CubeProgrammer** | Прошивка через SWD | Бесплатный от ST |
| **Keil µVision Debugger** | Пошаговая отладка | Используйте ST-Link |
| **Saleae Logic Analyzer** | Проверка тайминга SPI/UART | Декодирование SPI к SubGHz radio |
| **SDR dongle (RTL-SDR)** + **SDR#** или **GQRX** | Перехват LoRa RF-передач | Настройте на 868 MHz, увидите chirp spread spectrum |
| **LoRa field tester** (напр., LilyGO T-Beam) | Независимая проверка | Запустите простой LoRa receiver скетч |
| **Wireshark + gr-lora** (GNU Radio) | Декодирование LoRa-пакетов из эфира | Продвинутый: нужен SDR + GNU Radio |
| **ST-Link + SWO/ITM** | Трассировка в реальном времени без нагрузки UART | Быстрее, чем printf-отладка |

### 5. Автоматизированное регрессионное тестирование (CI-friendly)

Для непрерывного тестирования без железа создайте mock-реализации HAL и Radio:

```c
// tests/mock_radio.c
#include "radio.h"
static RadioEvents_t *mock_events;
void Radio_Init(RadioEvents_t *events) { mock_events = events; }
void Radio_Send(uint8_t *buf, uint8_t len) { /* записать вызов */ mock_events->TxDone(); }
void Radio_Rx(uint32_t timeout) { /* имитировать rx после задержки */ }
// ... и т.д.
```

Затем линкуйте с mock-драйвером вместо реального и запускайте логику приложения в тестовом окружении на ПК.

---

## Краткая справка: Arduino API → C API

| Arduino (RUI3) | C (SubGHz_Phy / данный порт) |
|---|---|
| `Serial.begin(115200)` | `MX_USART2_UART_Init()` (CubeMX) |
| `Serial.printf(...)` | `debug_printf(...)` |
| `Serial.println(...)` | `debug_println(...)` |
| `delay(ms)` | `HAL_Delay(ms)` |
| `millis()` | `HAL_GetTick()` |
| `api.lora.pfreq.set(f)` | `Radio.SetChannel(f)` |
| `api.lora.psf.set(sf)` | Часть `Radio.SetTxConfig()` / `SetRxConfig()` |
| `api.lora.precv(timeout)` | `lora_p2p_receive(timeout)` → `Radio.Rx()` |
| `api.lora.precv(0)` | `lora_p2p_standby()` → `Radio.Standby()` |
| `api.lora.psend(len, buf)` | `lora_p2p_send(buf, len)` → `Radio.Send()` |
| `api.lora.registerPRecvCallback` | `lora_p2p_register_rx_callback()` |
| `api.lora.registerPSendCallback` | `lora_p2p_register_tx_callback()` |
