##############################################################################
# Makefile для проверки компиляции LoRa P2P Master/Slave (STM32WLE5CC)
# Замена Keil Build на Linux (arm-none-eabi-gcc)
#
# Использование:
#   make master    — собрать прошивку Master (0 errors = код корректен)
#   make slave     — собрать прошивку Slave
#   make clean     — очистить артефакты сборки
#   make all       — собрать оба (Master и Slave)
##############################################################################

# Toolchain
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
LD      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# Directories
PROJ_DIR   = .
SRC_DIR    = $(PROJ_DIR)/Src
INC_DIR    = $(PROJ_DIR)/Inc
DRV_DIR    = $(PROJ_DIR)/Drivers
HAL_DIR    = $(DRV_DIR)/STM32WLxx_HAL_Driver
CMSIS_DEV  = $(DRV_DIR)/CMSIS/Device/ST/STM32WLxx
CMSIS_CORE = $(DRV_DIR)/CMSIS/Core

BUILD_DIR  = build

# MCU flags (Cortex-M4 without FPU, as in STM32WLE5)
MCU_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

# C defines
C_DEFS = \
	-DSTM32WLE5xx \
	-DUSE_HAL_DRIVER

# Include paths
C_INCLUDES = \
	-I$(INC_DIR) \
	-I$(HAL_DIR)/Inc \
	-I$(CMSIS_DEV)/Include \
	-I$(CMSIS_CORE)/Include

# Compiler flags
CFLAGS = $(MCU_FLAGS) $(C_DEFS) $(C_INCLUDES) \
	-Wall -Wextra -Wno-unused-parameter \
	-fdata-sections -ffunction-sections \
	-std=c11 -Os -g

# Assembler flags
ASFLAGS = $(MCU_FLAGS) -Wall -fdata-sections -ffunction-sections

# Linker flags
LDSCRIPT = $(PROJ_DIR)/STM32WLE5XX_FLASH.ld
LDFLAGS  = $(MCU_FLAGS) -T$(LDSCRIPT) \
	--specs=nosys.specs --specs=nano.specs \
	-Wl,--gc-sections \
	-lc -lm -lnosys

##############################################################################
# Source files
##############################################################################

# Startup (GCC assembly)
ASM_SRC = $(CMSIS_DEV)/startup_stm32wle5xx.s

# CMSIS system init
SYS_SRC = $(CMSIS_DEV)/system_stm32wlxx.c

# HAL driver sources (only what we use)
HAL_SRC = \
	$(HAL_DIR)/Src/stm32wlxx_hal.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_cortex.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_rcc.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_rcc_ex.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_gpio.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_uart.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_uart_ex.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_subghz.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_pwr.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_pwr_ex.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_dma.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_flash.c \
	$(HAL_DIR)/Src/stm32wlxx_hal_flash_ex.c

# Application sources (common to both Master and Slave)
APP_COMMON_SRC = \
	$(SRC_DIR)/hw_init.c \
	$(SRC_DIR)/lora_p2p.c \
	$(SRC_DIR)/uart_debug.c

# All common sources
COMMON_SRC = $(SYS_SRC) $(HAL_SRC) $(APP_COMMON_SRC)

##############################################################################
# Build targets
##############################################################################

.PHONY: all master slave clean

all: master slave

# ---------- MASTER ----------
MASTER_SRC = $(COMMON_SRC) $(SRC_DIR)/main_master.c
MASTER_OBJ = $(addprefix $(BUILD_DIR)/master/, \
	$(notdir $(MASTER_SRC:.c=.o)) \
	$(notdir $(ASM_SRC:.s=.o)))

master: $(BUILD_DIR)/master/master_firmware.elf
	@echo ""
	@echo "=============================================="
	@echo "  MASTER: Build SUCCESS — 0 errors!"
	@echo "=============================================="
	$(SIZE) $<

$(BUILD_DIR)/master/master_firmware.elf: $(MASTER_OBJ)
	$(LD) $(MASTER_OBJ) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map),--cref -o $@
	$(OBJCOPY) -O ihex $@ $(BUILD_DIR)/master/master_firmware.hex
	$(OBJCOPY) -O binary $@ $(BUILD_DIR)/master/master_firmware.bin

# ---------- SLAVE ----------
SLAVE_SRC = $(COMMON_SRC) $(SRC_DIR)/main_slave.c
SLAVE_OBJ = $(addprefix $(BUILD_DIR)/slave/, \
	$(notdir $(SLAVE_SRC:.c=.o)) \
	$(notdir $(ASM_SRC:.s=.o)))

slave: $(BUILD_DIR)/slave/slave_firmware.elf
	@echo ""
	@echo "=============================================="
	@echo "  SLAVE: Build SUCCESS — 0 errors!"
	@echo "=============================================="
	$(SIZE) $<

$(BUILD_DIR)/slave/slave_firmware.elf: $(SLAVE_OBJ)
	$(LD) $(SLAVE_OBJ) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map),--cref -o $@
	$(OBJCOPY) -O ihex $@ $(BUILD_DIR)/slave/slave_firmware.hex
	$(OBJCOPY) -O binary $@ $(BUILD_DIR)/slave/slave_firmware.bin

##############################################################################
# Compilation rules (VPATH-based)
##############################################################################

VPATH = $(sort $(dir $(COMMON_SRC) $(SRC_DIR)/main_master.c $(SRC_DIR)/main_slave.c $(ASM_SRC)))

$(BUILD_DIR)/master/%.o: %.c | $(BUILD_DIR)/master
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/master/%.o: %.s | $(BUILD_DIR)/master
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/slave/%.o: %.c | $(BUILD_DIR)/slave
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/slave/%.o: %.s | $(BUILD_DIR)/slave
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/master:
	mkdir -p $@

$(BUILD_DIR)/slave:
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

# Print detected sources (debug)
info:
	@echo "MASTER sources:"
	@echo $(MASTER_SRC) | tr ' ' '\n'
	@echo ""
	@echo "SLAVE sources:"
	@echo $(SLAVE_SRC) | tr ' ' '\n'
