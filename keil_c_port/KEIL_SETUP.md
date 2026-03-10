# Подключение к Keil — пошаговая инструкция

Целевой чип: **STM32WLE5CC** (RAK3172)
Подход: прямой HAL_SUBGHZ, **без SubGHz_Phy middleware**.

> **Проверено:** Keil MDK v5.43a, ARM Compiler V6.24, STM32WLxx DFP.
> Сборка: **0 errors**, 1 cosmetic warning.

---

## 1. Установка и подготовка Keil

### 1.1. Установка Keil MDK

Скачать установщик с сайта ARM (например, `MDK543a.exe`) и установить.

### 1.2. Установка Device Family Pack

После установки Keil автоматически откроется **Pack Installer**.
Если нет — в µVision: **Pack → Pack Installer**.

1. Слева в дереве Devices: **STMicroelectronics → STM32WL Series → STM32WLE5 → STM32WLE5CCUx**
2. Справа найдите пакет **Keil::STM32WLxx_DFP** → нажмите **Install**
3. Убедитесь, что **ARM::CMSIS** имеет статус "Up to date"
4. Закройте Pack Installer

### 1.3. Создание проекта в µVision

1. Откройте **Keil µVision5** (это отдельное приложение от Pack Installer)
2. **Project → New µVision Project...**
3. Выберите папку `keil_c_port/` и имя проекта
4. В диалоге выбора чипа: в поле **Search** введите `STM32WLE5` → выберите **STM32WLE5CCUx**
5. В диалоге "Manage Run-Time Environment" — нажмите **Cancel**
   (мы добавим файлы вручную)

> **Если CubeMX уже есть** — переходите к шагу 6 (внизу).

---

## 2. Добавить файлы порта в проект

**Project → Manage → Project Items...** (Ctrl+Shift+3)

Создайте **3 группы** (кнопка New/Insert):
- `Application`
- `HAL_Driver`
- `CMSIS`

### Группа Application — файлы из `keil_c_port/Src/`:

| Файл | Назначение |
|---|---|
| `hw_init.c` | Тактирование (MSI 48 МГц), GPIO, USART2, `huart2`, `Error_Handler` |
| `lora_p2p.c` | LoRa P2P драйвер поверх HAL_SUBGHZ |
| `uart_debug.c` | Отладочный вывод через UART |
| `main_master.c` | Прошивка Master **— только одно из двух** |
| `main_slave.c` | Прошивка Slave &nbsp; **— только одно из двух** |

> `main_master.c` и `main_slave.c` оба определяют `main()`.
> В проект добавлять **строго один** из них.

> **Важно:** `hw_init.c` экспортирует символы `SystemClock_Config`, `MX_GPIO_Init`,
> `MX_USART2_UART_Init`, `Error_Handler` и `huart2`, которые нужны линковщику.

---

## 3. Include Paths и Define

**Alt+F7** → вкладка **C/C++ (AC6)**

### Define

```
STM32WLE5xx, USE_HAL_DRIVER
```

### Include Paths

Нажмите кнопку `...` рядом с Include Paths и добавьте 4 пути:

```
.\Inc
.\Drivers\STM32WLxx_HAL_Driver\Inc
.\Drivers\CMSIS\Device\ST\STM32WLxx\Include
.\Drivers\CMSIS\Core\Include
```

> Если проект создан не в папке `keil_c_port/` — замените `.\` на
> соответствующий относительный путь.

Проверить, что компилятор находит:

- `lora_p2p.h`, `uart_debug.h`, `app_config.h`, `hw_init.h`
- `stm32wlxx_hal.h` (из HAL)
- `stm32wlxx_hal_conf.h` — **обязательный файл конфигурации HAL**

> **Важно:** HAL ищет файл `stm32wlxx_hal_conf.h` в Include Paths.
> В папке `Inc/` он уже есть. Если нет — скопируйте шаблон:
> ```
> Drivers/STM32WLxx_HAL_Driver/Inc/stm32wlxx_hal_conf_template.h
>   → Inc/stm32wlxx_hal_conf.h
> ```

---

## 4. HAL-файлы и CMSIS (группы HAL_Driver и CMSIS)

### Группа HAL_Driver

Добавить из `Drivers/STM32WLxx_HAL_Driver/Src/` (**все 13 файлов**):

```
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

> **Внимание:** Если забыть любой из этих файлов — линковщик выдаст
> `L6218E: Undefined symbol HAL_...` ошибки.

### Группа CMSIS

Добавить из `Drivers/CMSIS/Device/ST/STM32WLxx/`:

```
system_stm32wlxx.c
startup_stm32wle5xx_keil.s     # Startup для Keil (ARM Compiler 6)
```

> **ВАЖНО:** В репозитории есть **два** startup-файла:
> - `startup_stm32wle5xx.s` — для **GCC** (Linux/Makefile)
> - `startup_stm32wle5xx_keil.s` — для **Keil** (ARM Compiler 5/6)
>
> В Keil используйте **только** `startup_stm32wle5xx_keil.s`!
> GCC-версия не совместима с ARM Compiler и вызовет ошибки линковки.

---

## 5. Настройки линковщика и Target

**Alt+F7** → вкладка **Linker**:

- Поставьте галочку **"Use Memory Layout from Target Dialog"**
- Поле **Scatter File** оставьте пустым

**Alt+F7** → вкладка **Target**:

- **IROM1:** Start = `0x8000000`, Size = `0x40000` (256 КБ Flash)
- **IRAM1:** Start = `0x20000000`, Size = `0x8000` (или `0x10000`)

> Keil автоматически сгенерирует scatter-файл на основе этих адресов.

---

## 6. Сборка

**Project → Rebuild All Target Files** (или **F7**)

Ожидаемый результат:
```
Program Size: Code=21588 RO-data=1924 RW-data=12 ZI-data=2100
"...\rak-prj.axf" - 0 Error(s), 1 Warning(s).
```

> Warning `--target=armv7em-arm-none-eabi is not supported` — это
> косметическое предупреждение ARM Compiler V6.24, не влияет на результат.

Для генерации `.hex` файла: **Alt+F7 → Output → Create HEX File** ✓

---

## 7. Удалить SubGHz_Phy middleware (если есть)

Если middleware был добавлен ранее — удалить из проекта:

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

## 8. Если используется CubeMX-проект (main.c уже есть)

В этом случае `hw_init.c` **можно не добавлять**, но только при соблюдении
условий ниже. Если хоть одно не выполнено — **добавьте `hw_init.c` обратно**,
это самый простой путь (он не конфликтует с CubeMX-файлами).

### Что заменяет hw_init.c

`hw_init.c` определяет 5 символов, которые нужны другим файлам:

| Символ | Кто использует | Что произойдёт без него |
|---|---|---|
| `huart2` | `uart_debug.c` (`extern`) | **`L6218E: Undefined symbol huart2`** |
| `SystemClock_Config()` | `main_master.c` / `main_slave.c` | `L6218E: Undefined symbol SystemClock_Config` |
| `MX_GPIO_Init()` | `main_master.c` / `main_slave.c` | `L6218E: Undefined symbol MX_GPIO_Init` |
| `MX_USART2_UART_Init()` | `main_master.c` / `main_slave.c` | `L6218E: Undefined symbol MX_USART2_UART_Init` |
| `Error_Handler()` | `stm32wlxx_hal_msp.c` (CubeMX) | `L6218E: Undefined symbol Error_Handler` |

### Если вы удаляете hw_init.c — убедитесь, что:

1. **`usart.c` есть в проекте** — CubeMX генерирует его **только если USART2
   включён** (Connectivity → USART2 → Asynchronous). Именно `usart.c` определяет
   переменную `UART_HandleTypeDef huart2;`.

   > ⚠️ **Самая частая ошибка:** USART2 не был включён в CubeMX, файл `usart.c`
   > не сгенерирован, `huart2` нигде не определён →
   > **`L6218E: Undefined symbol huart2`**

   Проверьте Build Output — должна быть строка `compiling usart.c...`.
   Если её нет — файл не в проекте.

2. **`main.c` (от CubeMX) содержит** `SystemClock_Config()`, `MX_GPIO_Init()`,
   `Error_Handler()` — они генерируются автоматически.

3. **`main.c` не определяет свой `main()`** — если вы используете `main_master.c`
   или `main_slave.c`, в CubeMX-файле `main.c` удалите или закомментируйте
   функцию `int main(void)`, иначе будет `L6200E: Symbol main multiply defined`.

4. В `main_master.c` / `main_slave.c` замените:
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

5. `MX_SubGHz_Init()` — **не вызывать** (lora_p2p_init() делает всё сам)

6. Startup-файл: используйте тот, что сгенерировал CubeMX (он уже в формате
   ARM Compiler)

### Если сомневаетесь — просто добавьте hw_init.c

Это безопасный вариант: все 5 символов будут определены, конфликтов с CubeMX
не возникнет. Функции в `hw_init.c` имеют те же сигнатуры, что и CubeMX-версии.

---

## 9. Master / Slave — выбор роли

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

## 10. TCXO — если радио не инициализируется

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

## 11. Известные исправления (найдены при верификации сборки)

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

## 12. Альтернатива Keil — проверка сборки через GCC на Linux/macOS

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

> **Примечание:** Makefile использует `startup_stm32wle5xx.s` (GCC-формат),
> а Keil использует `startup_stm32wle5xx_keil.s` (ARM Compiler формат).
> Оба файла находятся в `Drivers/CMSIS/Device/ST/STM32WLxx/`.

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
