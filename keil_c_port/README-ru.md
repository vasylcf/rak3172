# LoRa P2P Master / Slave — Порт на чистый C для STM32WL (Keil µVision)

Портировано из Arduino/RUI3 скетчей данного репозитория, оригинал:
<https://github.com/Kongduino/RUI3_LoRa_P2P_PING_PONG>

---

## Сравнение: оригинал vs Arduino-код vs данный C-порт

| Аспект | Оригинал (Kongduino) | Arduino Master | Arduino Slave | Данный C-порт |
|---|---|---|---|---|
| **API** | `api.lorawan.*` (новый RUI3) | `api.lora.*` (старый RUI3) | `api.lora.*` | HAL_SUBGHZ напрямую |
| **Frequency** | 868.125 MHz | 868.000 MHz | 868.000 MHz | 868.000 MHz |
| **SF** | 12 | 12 | 7 ⚠️ | 12 (оба) |
| **BW** | 0 (125 kHz) | 0 (125 kHz) | 4 ⚠️ | 0 (125 kHz) |
| **CR** | 0 (4/5) | 0 (4/5) | 1 ⚠️ | 1 (4/5) |
| **TX Power** | 22 dBm | var=10, но `set(22)` | 22 dBm | 22 dBm |
| **TX payload** | `"payload #XXXX"` (счётчик) | `"508012"` (фиксированный) | `"It's me"` / `"It's not me"` | Как в Arduino |
| **Логика RX** | Ping-pong по rx_done | Игнорирует rx_done, шлёт каждые 5 с | Проверка серийного номера | Как в Arduino |
| **Разделение ролей** | Один файл, обе роли | Отдельный файл Master | Отдельный файл Slave | Отдельные `main_master.c` / `main_slave.c` |
| **Radio backend** | RUI3 внутренний | RUI3 внутренний | RUI3 внутренний | HAL_SUBGHZ напрямую, без middleware |

> ⚠️ **Баг в оригинальном Arduino-коде:** Slave использует SF=7, BW=4, CR=1, тогда как Master — SF=12, BW=0, CR=0. Эти настройки **несовместимы** — на обоих концах должны быть одинаковые параметры LoRa-модуляции. В C-порте оба устройства используют SF=12/BW=125kHz/CR=4\/5.

---

## Структура файлов

```
keil_c_port/
├── Inc/
│   ├── app_config.h            # Выбор роли (MASTER vs SLAVE), серийные номера
│   ├── hw_init.h               # Прототип hw_init() и extern huart2
│   ├── lora_p2p.h              # API абстракции радио
│   ├── uart_debug.h            # Debug printf через UART
│   └── stm32wlxx_hal_conf.h    # Конфигурация HAL (из шаблона ST)
├── Src/
│   ├── hw_init.c               # Тактирование (MSI 48 МГц), GPIO, USART2
│   ├── lora_p2p.c              # LoRa P2P драйвер поверх HAL_SUBGHZ (без middleware)
│   ├── uart_debug.c            # Реализация UART
│   ├── main_master.c           # Приложение Master (включать ТОЛЬКО его ИЛИ slave)
│   └── main_slave.c            # Приложение Slave
├── Drivers/                    # HAL и CMSIS от ST (скачиваются отдельно, см. BUILD_GUIDE.md)
│   ├── STM32WLxx_HAL_Driver/   # HAL драйверы (subghz, uart, rcc, gpio...)
│   └── CMSIS/                  # Cortex-M4 Core + STM32WLE5 device headers + startup
├── Makefile                    # Сборка через arm-none-eabi-gcc (Linux/macOS альтернатива Keil)
├── STM32WLE5XX_FLASH.ld        # Linker script для GCC
├── KEIL_SETUP.md               # Пошаговая инструкция по настройке Keil + отладка
├── BUILD_GUIDE.md              # Пошаговая инструкция сборки через GCC на Linux
└── README-ru.md                # Этот файл
```

---

## Настройка проекта в Keil

> 📄 Подробная пошаговая инструкция с разделом отладки: **[KEIL_SETUP.md](KEIL_SETUP.md)**

### Шаг 1 — Генерация базового проекта в STM32CubeMX

1. Откройте STM32CubeMX, создайте новый проект для **STM32WLE5CCU6** (MCU внутри RAK3172).
2. Включите периферию:
   - **USART2**: Asynchronous, 115200 baud, 8N1 (отладочный вывод через ST-Link VCP)
   - **RTC** (опционально, для меток времени)
3. **SubGHz_Phy middleware — НЕ добавлять.** Драйвер `lora_p2p.c` работает напрямую с HAL_SUBGHZ и не требует middleware.
4. Установите **Project Manager → Toolchain** в **MDK-ARM** (Keil).
5. **Generate Code**.

### Шаг 2 — Добавление файлов порта

1. В Keil µVision откройте сгенерированный `.uvprojx`.
2. Добавьте в **Source Group**:
   - `Src/hw_init.c` *(не нужен, если используете CubeMX-проект с готовым `main.c`)*
   - `Src/lora_p2p.c`
   - `Src/uart_debug.c`
   - **Либо** `Src/main_master.c` **либо** `Src/main_slave.c` (не оба — оба определяют `main()`).
3. Добавьте `Inc/` в **Options → C/C++ → Include Paths**.
4. Удалите или переименуйте сгенерированный CubeMX файл `main.c` (он тоже определяет `main()`).
5. Убедитесь, что в **Sources** есть стандартные HAL-файлы:
   `stm32wlxx_hal.c`, `stm32wlxx_hal_subghz.c`, `stm32wlxx_hal_uart.c`, `stm32wlxx_hal_uart_ex.c`,
   `stm32wlxx_hal_rcc.c`, `stm32wlxx_hal_rcc_ex.c`, `stm32wlxx_hal_gpio.c`,
   `stm32wlxx_hal_cortex.c`, `stm32wlxx_hal_pwr.c`, `stm32wlxx_hal_pwr_ex.c`,
   `stm32wlxx_hal_dma.c`, `stm32wlxx_hal_flash.c`, `stm32wlxx_hal_flash_ex.c`

> ⚠️ **Частая ошибка:** файлы `dma`, `flash`, `flash_ex` часто забывают добавить.
> Без них линковщик выдаст `undefined reference` на внутренние зависимости HAL.

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

## Верификация сборки

Код проверен кросс-компилятором `arm-none-eabi-gcc 13.2.1` на Linux — **0 ошибок, 0 предупреждений** (в коде приложения).

При верификации были найдены и исправлены **3 бага** в `lora_p2p.c` (несовместимость с актуальной версией HAL):

| Было (неправильно) | Стало (правильно) |
|---|---|
| `RadioSetCmd_TypeDef` | `SUBGHZ_RadioSetCmd_t` |
| `RADIO_SET_DIO2ASRFSWITCH` | `RADIO_SET_RFSWITCHMODE` |
| `RADIO_SET_DIOIRQPARAMS` | `RADIO_CFG_DIOIRQ` |

### Сборка без Keil (Linux/macOS)

Если у вас нет Keil, проверить компиляцию можно через GCC:

```bash
# Установка (Ubuntu/Debian)
sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi

# Сборка
cd keil_c_port
make master   # Собрать Master (build/master/master_firmware.bin)
make slave    # Собрать Slave  (build/slave/slave_firmware.bin)
make all      # Собрать оба
make clean    # Очистить
```

> Подробная инструкция: **[BUILD_GUIDE.md](BUILD_GUIDE.md)**

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

Для непрерывного тестирования без железа создайте mock-реализации HAL и `lora_p2p`:

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
    /* Симулировать TX Done */
    if (mock_tx_cb) mock_tx_cb();
    return true;
}
void lora_p2p_irq_process(void) {}

/* Симулировать прием пакета из теста */
void simulate_rx(const uint8_t *buf, uint8_t len, int16_t rssi, int8_t snr) {
    if (mock_rx_cb) {
        lora_p2p_rx_data_t d = { .buffer=buf, .buffer_size=len, .rssi=rssi, .snr=snr };
        mock_rx_cb(d);
    }
}
```

Затем линкуйте с mock-драйвером вместо реального и запускайте логику приложения в тестовом окружении на ПК.

---

## Краткая справка: Arduino API → C API

| Arduino (RUI3) | C (данный порт) |
|---|---|
| `Serial.begin(115200)` | `hw_init()` или `MX_USART2_UART_Init()` (CubeMX) |
| `Serial.printf(...)` | `debug_printf(...)` |
| `Serial.println(...)` | `debug_println(...)` |
| `delay(ms)` | `HAL_Delay(ms)` |
| `millis()` | `HAL_GetTick()` |
| `api.lora.pfreq.set(f)` | поле `cfg.frequency` в `lora_p2p_config_t` |
| `api.lora.psf.set(sf)` | поле `cfg.spreading_factor` |
| `api.lora.pbw.set(bw)` | поле `cfg.bandwidth` |
| `api.lora.pcr.set(cr)` | поле `cfg.coding_rate` |
| `api.lora.ptp.set(pwr)` | поле `cfg.tx_power` |
| `api.lora.precv(timeout)` | `lora_p2p_receive(timeout)` |
| `api.lora.precv(0)` | `lora_p2p_standby()` |
| `api.lora.psend(len, buf)` | `lora_p2p_send(buf, len)` |
| `api.lora.registerPRecvCallback` | `lora_p2p_register_rx_callback()` |
| `api.lora.registerPSendCallback` | `lora_p2p_register_tx_callback()` |
