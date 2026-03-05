# Подключение к Keil — пошаговая инструкция

Целевой чип: **STM32WLE5CC** (RAK3172)
Подход: прямой HAL_SUBGHZ, **без SubGHz_Phy middleware**.

---

## 1. Создание CubeMX-проекта (с нуля)

1. Открыть STM32CubeMX → New Project → выбрать **STM32WLE5CCUx**
2. **Connectivity → USART2** → Mode: `Asynchronous`
   - Baud Rate: `115200`, Word Length: `8`, Parity: `None`, Stop Bits: `1`
3. **Middleware → SubGHz_Phy** — **НЕ добавлять** (не нужен)
4. **Project Manager**:
   - Toolchain/IDE: `MDK-ARM`
   - Min Version: `V5`
5. **Generate Code**

> **Если уже есть готовый CubeMX-проект** — переходите сразу к шагу 2.
> Убедитесь, что в нём нет SubGHz_Phy middleware в дереве файлов.

---

## 2. Добавить файлы порта в проект

В Keil: правой кнопкой по группе в Project Explorer → **Add Existing Files**.

### Файлы из папки `keil_c_port/Src/`:

| Файл | Назначение |
|---|---|
| `hw_init.c` | Тактирование (MSI 48 МГц), GPIO, USART2, `huart2` |
| `lora_p2p.c` | LoRa P2P драйвер поверх HAL_SUBGHZ |
| `uart_debug.c` | Отладочный вывод через UART |
| `main_master.c` | Прошивка Master **— только одно из двух** |
| `main_slave.c` | Прошивка Slave &nbsp; **— только одно из двух** |

> `main_master.c` и `main_slave.c` оба определяют `main()`.
> В проект добавлять **строго один** из них.

---

## 3. Include Paths

`Options for Target → C/C++ → Include Paths` — добавить:

```
../keil_c_port/Inc
```

или абсолютный путь до папки `Inc/`, если структура другая.

Проверить, что компилятор находит:

- `lora_p2p.h`
- `uart_debug.h`
- `app_config.h`
- `hw_init.h`
- `stm32wlxx_hal.h` (из STM32CubeWL HAL)
- `stm32wlxx_hal_conf.h` — **обязательный файл конфигурации HAL**

> **Важно:** HAL ищет файл `stm32wlxx_hal_conf.h` в Include Paths.
> Если CubeMX его не сгенерировал, скопируйте шаблон:
> ```
> Drivers/STM32WLxx_HAL_Driver/Inc/stm32wlxx_hal_conf_template.h
>   → Inc/stm32wlxx_hal_conf.h
> ```
> Шаблон включает все модули по умолчанию — этого достаточно.

---

## 4. Обязательные HAL-файлы в проекте

Убедиться, что в Sources присутствуют (обычно добавляет CubeMX):

```
Drivers/STM32WLxx_HAL_Driver/Src/
    stm32wlxx_hal.c            # Базовый HAL (HAL_Init, HAL_Delay, SysTick)
    stm32wlxx_hal_cortex.c     # NVIC, SysTick
    stm32wlxx_hal_rcc.c        # Тактирование
    stm32wlxx_hal_rcc_ex.c     # Расширенное тактирование
    stm32wlxx_hal_gpio.c       # GPIO
    stm32wlxx_hal_uart.c       # UART (отладочный вывод)
    stm32wlxx_hal_uart_ex.c    # UART расширенный
    stm32wlxx_hal_subghz.c     # SubGHz SPI к радиомодулю SX126x
    stm32wlxx_hal_pwr.c        # Управление питанием
    stm32wlxx_hal_pwr_ex.c     # Управление питанием расширенный
    stm32wlxx_hal_dma.c        # DMA (зависимость HAL)
    stm32wlxx_hal_flash.c      # Flash (зависимость HAL)
    stm32wlxx_hal_flash_ex.c   # Flash расширенный
```

> **Внимание:** `stm32wlxx_hal_dma.c`, `stm32wlxx_hal_flash.c` и `stm32wlxx_hal_flash_ex.c`
> часто забывают добавить. Без них линковщик выдаст `undefined reference` на функции,
> которые вызываются внутри других HAL-модулей.

CMSIS и startup также должны присутствовать:

```
Core/Src/system_stm32wlxx.c
startup_stm32wle5xx.s          (или .s в зависимости от версии)
```

---

## 5. Удалить SubGHz_Phy middleware

Если middleware уже был добавлен — удалить из проекта (или исключить из сборки):

```
Middlewares/Third_Party/SubGHz_Phy/    <- удалить всю группу
SubGHz_Phy/Target/                      <- удалить
```

Также удалить из Include Paths пути вида:
```
../Middlewares/Third_Party/SubGHz_Phy/...
../SubGHz_Phy/Target
```

---

## 6. Если используется CubeMX-проект (main.c уже есть)

В этом случае `hw_init.c` **не нужен**. Вместо этого:

1. Удалить `hw_init.c` из Sources
2. В `main_master.c` / `main_slave.c` заменить:
   ```c
   #include "hw_init.h"
   // ...
   hw_init();
   ```
   обратно на:
   ```c
   extern void SystemClock_Config(void);
   extern void MX_GPIO_Init(void);
   extern void MX_USART2_UART_Init(void);
   // ...
   HAL_Init();
   SystemClock_Config();
   MX_GPIO_Init();
   MX_USART2_UART_Init();
   ```
3. `MX_SubGHz_Init()` — **не вызывать** (lora_p2p_init() делает всё сам)
4. Убедиться, что в вашем `main.c` от CubeMX **нет** своего `main()` —
   либо удалить его, либо переименовать CubeMX-файл

---

## 7. Master / Slave — выбор роли

В файле `Inc/app_config.h`:

```c
#define APP_ROLE_MASTER   1   // <- 1 = Master
#define APP_ROLE_SLAVE    0   // <- 1 = Slave (поменять местами)
```

Или через препроцессорный дефайн в Options → C/C++ → Define:
```
APP_ROLE_MASTER=1
```

---

## 8. TCXO — если радио не инициализируется

По умолчанию `lora_p2p.c` выставляет напряжение TCXO = **3.0 В** (код `0x06`).
Если ваш модуль использует другое напряжение — добавьте дефайн:

```
Options → C/C++ → Define:  LORA_TCXO_VOLTAGE=0x07
```

| Код | Напряжение |
|-----|-----------|
| 0x01 | 1.7 В |
| 0x02 | 1.8 В |
| 0x05 | 2.7 В |
| 0x06 | 3.0 В (RAK3172 по умолчанию) |
| 0x07 | 3.3 В |

---

## 9. Известные исправления (найдены при верификации сборки)

При проверке кода кросс-компилятором `arm-none-eabi-gcc` на Linux были обнаружены
и исправлены **3 ошибки** в `lora_p2p.c`. Эти же ошибки возникли бы и в Keil.

Если вы работаете со старой версией файлов — проверьте, что в вашем `lora_p2p.c`
используются **правильные** имена из HAL:

| Что | Неправильно (старое) | Правильно (актуальное) | Описание |
|---|---|---|---|
| Тип команды | `RadioSetCmd_TypeDef` | `SUBGHZ_RadioSetCmd_t` | Enum в `stm32wlxx_hal_subghz.h` |
| DIO2 RF switch | `RADIO_SET_DIO2ASRFSWITCH` | `RADIO_SET_RFSWITCHMODE` | Значение 0x9D, уже есть в HAL enum |
| DIO IRQ config | `RADIO_SET_DIOIRQPARAMS` | `RADIO_CFG_DIOIRQ` | Значение 0x08 |

> Эти имена зависят от версии STM32CubeWL HAL. В актуальной версии (v1.3.0+)
> используются имена из правого столбца. Если вы видите ошибку вида
> `unknown type name 'RadioSetCmd_TypeDef'` — примените исправления выше.

---

## 10. Альтернатива Keil — проверка сборки через GCC на Linux/macOS

Если Keil недоступен (например, на Linux/macOS), можно верифицировать код
тем же ARM-компилятором через командную строку.

### Установка (Ubuntu/Debian)

```bash
sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi
```

### Сборка

В папке `keil_c_port/` есть готовый `Makefile`:

```bash
cd keil_c_port
make master   # Собрать Master (build/master/master_firmware.bin)
make slave    # Собрать Slave  (build/slave/slave_firmware.bin)
make all      # Собрать оба
make clean    # Очистить
```

Makefile автоматически подтягивает HAL-драйверы из `Drivers/` и генерирует
готовые `.bin`/`.hex` файлы для прошивки через STM32CubeProgrammer.

> **Примечание:** Для работы Makefile необходимо, чтобы папка `Drivers/`
> содержала HAL и CMSIS файлы. Инструкция по скачиванию — в `BUILD_GUIDE.md`.

### Что это даёт

- **0 errors** = код гарантированно соберётся и в Keil
- Те же ошибки типов, пропущенных include, несовместимых аргументов
- Можно запускать в CI/CD для автоматической проверки

---

---

# Сбор информации для отладки

Если что-то не работает — выполните следующие шаги и предоставьте вывод.

---

## A. Ошибки компиляции — список всех ошибок

В Keil: **Build → Rebuild All** и скопировать полный вывод из окна Build Output.

Особенно важны строки вида:
```
error: ...
fatal error: ...
undefined reference to ...
```

---

## B. Какие файлы реально попали в сборку

`Project → Manage → Project Items` — скриншот дерева файлов.
Или в Build Output найти строки:
```
compiling lora_p2p.c...
compiling hw_init.c...
```

---

## C. Проверка Include Paths

Если ошибки типа `fatal error: lora_p2p.h: No such file`:

`Options for Target → C/C++ → Include Paths` — скопировать список путей.

---

## D. Проверка — нет ли SubGHz_Phy в проекте

В папке проекта выполнить поиск:

```
# Windows (PowerShell)
Get-ChildItem -Recurse -Filter "radio.h" .
Get-ChildItem -Recurse -Filter "radio_driver.c" .

# Linux / macOS
find . -name "radio.h" -o -name "radio_driver.c"
```

Если нашлись — эти файлы (или их папки) нужно удалить из Sources в Keil.

---

## E. Проверка — нет ли двойного определения main()

```
# Linux / macOS
grep -rn "^int main" .

# Windows (PowerShell)
Select-String -Recurse -Pattern "^int main" -Include "*.c"
```

Должен быть только **один** `main()` — либо в `main_master.c`, либо в `main_slave.c`.

---

## F. Проверка дефайнов компилятора

`Options for Target → C/C++ → Define` — скопировать содержимое.

Обычно там должно быть что-то вроде:
```
STM32WLE5xx, USE_HAL_DRIVER
```

---

## G. Runtime — нет вывода в UART

1. Проверить подключение: `PA2 = TX`, `PA3 = RX` (USART2 RAK3172)
2. Настройки терминала: `115200, 8N1, no flow control`
3. Добавить в начало `main()` тестовый вызов сразу после `hw_init()`:
   ```c
   debug_println("BOOT OK");
   ```
   Если этой строки нет — проблема в инициализации клока или UART.

---

## H. Runtime — радио не запускается (lora_p2p_init возвращает false)

Добавить проверку возврата HAL_SUBGHZ_Init:

```c
// в lora_p2p.c -> lora_p2p_init(), временно для отладки:
if (HAL_SUBGHZ_Init(&s_hsubghz) != HAL_OK) {
    debug_println("ERROR: HAL_SUBGHZ_Init failed");
    return false;
}
debug_println("HAL_SUBGHZ_Init OK");
```

Если `HAL_SUBGHZ_Init` завершается успешно, но радио молчит — скорее всего
неправильное напряжение TCXO (см. шаг 8 выше).

---

## I. Полная информация для issue/вопроса

При обращении за помощью предоставить:

- [ ] Версия STM32CubeWL (Help → About в CubeMX)
- [ ] Версия Keil MDK
- [ ] Полный Build Output (Rebuild All)
- [ ] Скриншот дерева файлов проекта
- [ ] Содержимое `Options → C/C++ → Include Paths`
- [ ] Содержимое `Options → C/C++ → Define`
- [ ] Вывод UART (если устройство стартует)
