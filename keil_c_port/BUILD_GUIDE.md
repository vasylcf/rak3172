# Сборка и проверка кода LoRa P2P на Ubuntu (без Keil)

Пошаговое описание того, как мы настроили компиляцию прошивки для RAK3172 (STM32WLE5CC) на Ubuntu 24.02, используя ARM GCC вместо Keil µVision.

---

## Предыстория

Keil µVision — **только Windows**. На Ubuntu он не работает. Однако цель — **проверить код на 0 ошибок компиляции** — полностью достижима с помощью `arm-none-eabi-gcc`. Это тот же ARM-компилятор, который ловит все те же ошибки (неправильные типы, отсутствующие функции, синтаксические ошибки).

---

## Шаг 1: Установка ARM GCC кросс-компилятора

Проверили, что `arm-none-eabi-gcc` не установлен:

```bash
which arm-none-eabi-gcc
# arm-none-eabi-gcc not found
```

Установили компилятор и стандартную библиотеку для embedded:

```bash
sudo apt-get update -qq
sudo apt-get install -y gcc-arm-none-eabi libnewlib-arm-none-eabi
```

Проверили установку:

```bash
arm-none-eabi-gcc --version
# arm-none-eabi-gcc (15:13.2.rel1-2) 13.2.1 20231009
```

---

## Шаг 2: Скачивание STM32WL HAL драйверов и CMSIS

Код проекта использует `#include "stm32wlxx_hal.h"` — это HAL-библиотека от STMicroelectronics. Без неё компилятор не знает, что такое `HAL_SUBGHZ_Init`, `UART_HandleTypeDef` и т.д.

Нужны три набора заголовков/исходников:

| Компонент | Что это | Откуда |
|---|---|---|
| **STM32WLxx HAL Driver** | Драйверы периферии (UART, SUBGHZ, RCC, GPIO...) | [stm32wlxx_hal_driver](https://github.com/STMicroelectronics/stm32wlxx_hal_driver) |
| **CMSIS Device** | Описание регистров и startup-файл для STM32WLE5 | [cmsis_device_wl](https://github.com/STMicroelectronics/cmsis_device_wl) |
| **CMSIS Core** | Заголовки ядра ARM Cortex-M4 | [CMSIS_5](https://github.com/ARM-software/CMSIS_5) |

### 2.1 Клонирование репозиториев

```bash
# HAL Driver (subghz, uart, rcc, gpio и т.д.)
git clone --depth 1 https://github.com/STMicroelectronics/stm32wlxx_hal_driver.git /tmp/stm32wlxx_hal_driver

# CMSIS Device (регистры STM32WLE5, startup, linker script)
git clone --depth 1 https://github.com/STMicroelectronics/cmsis_device_wl.git /tmp/cmsis_device_wl

# CMSIS Core (core_cm4.h, cmsis_gcc.h и т.д.)
git clone --depth 1 https://github.com/ARM-software/CMSIS_5.git /tmp/CMSIS_5 --no-checkout
cd /tmp/CMSIS_5
git sparse-checkout init --cone
git sparse-checkout set CMSIS/Core/Include
git checkout
```

> **Примечание:** Основной репозиторий [STM32CubeWL](https://github.com/STMicroelectronics/STM32CubeWL) использует git submodules для драйверов, поэтому мы клонировали подмодули напрямую — это быстрее и проще.

### 2.2 Копирование в проект

Создали структуру `Drivers/` в проекте:

```bash
cd /home/vash/apps/rak3172/keil_c_port

mkdir -p Drivers/STM32WLxx_HAL_Driver/Inc \
         Drivers/STM32WLxx_HAL_Driver/Src \
         Drivers/CMSIS/Device/ST/STM32WLxx/Include \
         Drivers/CMSIS/Core/Include
```

Скопировали файлы:

```bash
# HAL заголовки и исходники
cp -r /tmp/stm32wlxx_hal_driver/Inc/* Drivers/STM32WLxx_HAL_Driver/Inc/
cp -r /tmp/stm32wlxx_hal_driver/Src/* Drivers/STM32WLxx_HAL_Driver/Src/

# CMSIS Device (регистры STM32WLE5)
cp /tmp/cmsis_device_wl/Include/* Drivers/CMSIS/Device/ST/STM32WLxx/Include/

# CMSIS Core (ядро ARM Cortex-M4)
cp /tmp/CMSIS_5/CMSIS/Core/Include/* Drivers/CMSIS/Core/Include/

# System init и GCC startup assembly
cp /tmp/cmsis_device_wl/Source/Templates/system_stm32wlxx.c \
   Drivers/CMSIS/Device/ST/STM32WLxx/
cp /tmp/cmsis_device_wl/Source/Templates/gcc/startup_stm32wle5xx.s \
   Drivers/CMSIS/Device/ST/STM32WLxx/

# Linker script для STM32WLE5 (256K Flash, 64K RAM)
cp /tmp/cmsis_device_wl/Source/Templates/gcc/linker/STM32WLE5XX_FLASH.ld .
```

### 2.3 Создание конфигурации HAL

HAL требует файл `stm32wlxx_hal_conf.h`, который определяет, какие модули включены. Использовали шаблон от ST:

```bash
cp Drivers/STM32WLxx_HAL_Driver/Inc/stm32wlxx_hal_conf_template.h \
   Inc/stm32wlxx_hal_conf.h
```

Шаблон включает все модули по умолчанию (UART, SUBGHZ, RCC, GPIO, DMA, FLASH, PWR и т.д.) — это то, что нам нужно.

---

## Шаг 3: Создание Makefile

Keil использует `.uvprojx` файл проекта для управления сборкой. На Linux его аналог — `Makefile`.

Создали `Makefile` со следующей конфигурацией:

- **MCU:** Cortex-M4, Thumb mode, без FPU (soft float) — как в STM32WLE5CC
- **Defines:** `-DSTM32WLE5xx -DUSE_HAL_DRIVER`
- **Include paths:** `Inc/`, HAL headers, CMSIS Device, CMSIS Core
- **Оптимизация:** `-Os` (размер), `-g` (отладочная информация)
- **Линковка:** `STM32WLE5XX_FLASH.ld`, nano specs (минимальная libc), gc-sections (удаление неиспользуемого кода)

HAL-исходники, включённые в сборку:

| Файл | Для чего |
|---|---|
| `stm32wlxx_hal.c` | Базовый HAL (HAL_Init, HAL_Delay, SysTick) |
| `stm32wlxx_hal_cortex.c` | NVIC, SysTick |
| `stm32wlxx_hal_rcc.c` / `_ex.c` | Тактирование (MSI 48 MHz) |
| `stm32wlxx_hal_gpio.c` | GPIO (порты A, B, C) |
| `stm32wlxx_hal_uart.c` / `_ex.c` | USART2 (отладочный вывод) |
| `stm32wlxx_hal_subghz.c` | SubGHz SPI к радиомодулю SX126x |
| `stm32wlxx_hal_pwr.c` / `_ex.c` | Управление питанием |
| `stm32wlxx_hal_dma.c` | DMA (зависимость HAL) |
| `stm32wlxx_hal_flash.c` / `_ex.c` | Flash (зависимость HAL) |
| `system_stm32wlxx.c` | SystemInit (CMSIS) |
| `startup_stm32wle5xx.s` | Startup assembly (вектор прерываний, Reset_Handler) |

Файлы приложения:

| Файл | Роль |
|---|---|
| `hw_init.c` | Инициализация тактирования, GPIO, USART2 |
| `lora_p2p.c` | LoRa P2P драйвер (прямой HAL_SUBGHZ) |
| `uart_debug.c` | Printf через UART |
| `main_master.c` **или** `main_slave.c` | Точка входа (выбирается при сборке) |

---

## Шаг 4: Первая компиляция и обнаружение ошибок

Запустили сборку:

```bash
make master
```

### Результат: 3 ошибки компиляции в `lora_p2p.c`

Все ошибки связаны с тем, что код был написан под **старую версию HAL** (или другой middleware), а актуальная версия HAL использует другие имена:

#### Ошибка 1: Неправильный тип команды

```
error: unknown type name 'RadioSetCmd_TypeDef'
```

- **Было:** `RadioSetCmd_TypeDef` — такого типа нет в HAL
- **Стало:** `SUBGHZ_RadioSetCmd_t` — правильное имя enum в `stm32wlxx_hal_subghz.h`

#### Ошибка 2: Несуществующая константа DIO2

```
error: 'RadioSetCmd_TypeDef' undeclared
```

Это из макроса:
```c
#define RADIO_SET_DIO2ASRFSWITCH  ((RadioSetCmd_TypeDef)0x9DU)
```

- **Было:** самодельный макрос с кастом несуществующего типа
- **Стало:** `RADIO_SET_RFSWITCHMODE` — эта константа (тоже 0x9D) уже есть в HAL enum, макрос не нужен

#### Ошибка 3: Неправильное имя команды IRQ

```
error: 'RADIO_SET_DIOIRQPARAMS' undeclared; did you mean 'RADIO_SET_PACKETPARAMS'?
```

- **Было:** `RADIO_SET_DIOIRQPARAMS`
- **Стало:** `RADIO_CFG_DIOIRQ` (0x08) — правильное имя в HAL

---

## Шаг 5: Исправление ошибок

Внесли три изменения в `Src/lora_p2p.c`:

```diff
- static void sx_cmd1(RadioSetCmd_TypeDef cmd, uint8_t val)
+ static void sx_cmd1(SUBGHZ_RadioSetCmd_t cmd, uint8_t val)
```

```diff
- #define RADIO_SET_DIO2ASRFSWITCH  ((RadioSetCmd_TypeDef)0x9DU)
- ...
- HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_DIO2ASRFSWITCH, &p, 1U);
+ /* DIO2-as-RF-switch: RADIO_SET_RFSWITCHMODE (0x9D) in HAL enum */
+ ...
+ HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_RFSWITCHMODE, &p, 1U);
```

```diff
- HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_SET_DIOIRQPARAMS, p, 8U);
+ HAL_SUBGHZ_ExecSetCmd(&s_hsubghz, RADIO_CFG_DIOIRQ, p, 8U);
```

---

## Шаг 6: Успешная сборка

После исправлений:

```bash
make clean && make all
```

```
==============================================
  MASTER: Build SUCCESS — 0 errors!
==============================================
   text    data     bss     dec     hex filename
  13848     112    2360   16320    3fc0 build/master/master_firmware.elf

==============================================
  SLAVE: Build SUCCESS — 0 errors!
==============================================
   text    data     bss     dec     hex filename
  13776     112    2352   16240    3f70 build/slave/slave_firmware.elf
```

Оба проекта — **Master и Slave** — собраны без единой ошибки.

---

## Шаг 7: Ошибка с map-файлом (побочная)

На этапе линковки возникла ошибка пути:

```
ld: cannot open map file build/build/master/master_firmware.elf.map
```

Причина: в Makefile переменная `$@` в контексте `LDFLAGS` расширялась в имя target (`master`), а не в путь к `.elf`. Исправлено: map-файл генерируется прямо в правиле линковки с правильным путём.

---

## Структура проекта после настройки

```
keil_c_port/
├── Makefile                         # ← СОЗДАН: сборочный скрипт
├── STM32WLE5XX_FLASH.ld             # ← СКОПИРОВАН: linker script
├── Inc/
│   ├── app_config.h
│   ├── hw_init.h
│   ├── lora_p2p.h
│   ├── uart_debug.h
│   └── stm32wlxx_hal_conf.h        # ← СОЗДАН: конфигурация HAL
├── Src/
│   ├── hw_init.c
│   ├── lora_p2p.c                   # ← ИСПРАВЛЕН: 3 бага
│   ├── main_master.c
│   ├── main_slave.c
│   └── uart_debug.c
├── Drivers/                         # ← СКАЧАН: HAL + CMSIS
│   ├── STM32WLxx_HAL_Driver/
│   │   ├── Inc/   (HAL заголовки)
│   │   └── Src/   (HAL исходники)
│   └── CMSIS/
│       ├── Core/Include/            (ARM Cortex-M4 headers)
│       └── Device/ST/STM32WLxx/
│           ├── Include/             (регистры STM32WLE5)
│           ├── system_stm32wlxx.c   (SystemInit)
│           └── startup_stm32wle5xx.s (GCC startup)
├── build/                           # ← ГЕНЕРИРУЕТСЯ: артефакты сборки
│   ├── master/
│   │   ├── master_firmware.bin      (14 KB — готовая прошивка)
│   │   ├── master_firmware.hex
│   │   ├── master_firmware.elf
│   │   └── master_firmware.map
│   └── slave/
│       ├── slave_firmware.bin       (14 KB — готовая прошивка)
│       ├── slave_firmware.hex
│       ├── slave_firmware.elf
│       └── slave_firmware.map
├── KEIL_SETUP.md
├── README.md
└── README-ru.md
```

---

## Итог

### Что установлено
1. **arm-none-eabi-gcc 13.2.1** — ARM кросс-компилятор (аналог компилятора в Keil)
2. **STM32WL HAL Driver** — драйверы периферии от ST (subghz, uart, rcc, gpio...)
3. **CMSIS Core + Device** — заголовки Cortex-M4 и описание регистров STM32WLE5

### Что создано
- `Makefile` — сборочный скрипт (замена Keil project)
- `stm32wlxx_hal_conf.h` — конфигурация HAL
- `Drivers/` — папка с HAL и CMSIS (скачана с GitHub ST)

### Найденные и исправленные баги в коде

При компиляции обнаружились **3 ошибки** в `lora_p2p.c`:

| Ошибка | Было (неправильно) | Стало (правильно) |
|---|---|---|
| Тип команды | `RadioSetCmd_TypeDef` | `SUBGHZ_RadioSetCmd_t` |
| DIO2 RF switch | `RADIO_SET_DIO2ASRFSWITCH` (самодельный макрос) | `RADIO_SET_RFSWITCHMODE` (уже есть в HAL enum) |
| DIO IRQ params | `RADIO_SET_DIOIRQPARAMS` | `RADIO_CFG_DIOIRQ` |

Эти ошибки вызвали бы **crash при компиляции в Keil точно так же**. Теперь они исправлены.

### Как пользоваться
```bash
cd keil_c_port
make master   # Собрать Master (build/master/master_firmware.bin)
make slave    # Собрать Slave  (build/slave/slave_firmware.bin)
make all      # Собрать оба
make clean    # Очистить
```

### Сгенерированные прошивки
| Файл | Размер | Формат |
|---|---|---|
| `master_firmware.bin` | 14 KB | Готов к прошивке через STM32CubeProgrammer |
| `master_firmware.hex` | 39 KB | Intel HEX (альтернативный формат) |
| `slave_firmware.bin` | 14 KB | Готов к прошивке |
| `slave_firmware.hex` | 39 KB | Intel HEX |

Предупреждения `_read/_write is not implemented` — это **нормально** для bare-metal (без операционной системы). На реальном железе `vsnprintf` будет просто записывать в буфер, а вывод идёт через `HAL_UART_Transmit` напрямую.
