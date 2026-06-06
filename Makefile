##########################################################
# Makefile for STM32G474RE — Navigation Game firmware
# Toolchain: arm-none-eabi-gcc (Arm GNU Toolchain)
#
# This builds the application sources against the STM32CubeG4
# HAL/CMSIS package. Point CUBE_HAL_DIR at your install, e.g.:
#   make CUBE_HAL_DIR=/c/Users/you/STM32Cube/Repository/STM32CubeG4
# (See CLAUDE.md "Build & flash" for the one-time HAL setup.)
##########################################################

TARGET   = navgame
BUILD    = build
CUBE_HAL_DIR ?= $(HOME)/STM32Cube/Repository/STM32CubeG4

######################################
# Application sources
######################################
C_SOURCES  = Core/Src/main.c
C_SOURCES += Core/Src/mpu6050.c
C_SOURCES += Core/Src/servo.c
C_SOURCES += Core/Src/audio.c
C_SOURCES += Core/Src/navigation.c
C_SOURCES += Core/Src/game.c
C_SOURCES += Core/Src/led.c
C_SOURCES += Core/Src/serial_log.c

# CMSIS system file (provides SystemInit / SystemCoreClock)
C_SOURCES += $(CUBE_HAL_DIR)/Drivers/CMSIS/Device/ST/STM32G4xx/Source/Templates/system_stm32g4xx.c

######################################
# HAL driver sources (only what we use)
######################################
HAL_SRC = $(CUBE_HAL_DIR)/Drivers/STM32G4xx_HAL_Driver/Src
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_rcc.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_rcc_ex.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_cortex.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_pwr.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_pwr_ex.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_gpio.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_i2c.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_i2c_ex.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_uart.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_uart_ex.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_tim.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_tim_ex.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_rng.c

######################################
# Startup (assembly)
######################################
ASM_SOURCES = \
  $(CUBE_HAL_DIR)/Drivers/CMSIS/Device/ST/STM32G4xx/Source/Templates/gcc/startup_stm32g474xx.s

######################################
# Includes
######################################
C_INCLUDES  = -ICore/Inc
C_INCLUDES += -I$(CUBE_HAL_DIR)/Drivers/STM32G4xx_HAL_Driver/Inc
C_INCLUDES += -I$(CUBE_HAL_DIR)/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy
C_INCLUDES += -I$(CUBE_HAL_DIR)/Drivers/CMSIS/Device/ST/STM32G4xx/Include
C_INCLUDES += -I$(CUBE_HAL_DIR)/Drivers/CMSIS/Core/Include

######################################
# Defines
######################################
C_DEFS = -DSTM32G474xx -DUSE_HAL_DRIVER

######################################
# Toolchain
######################################
PREFIX = arm-none-eabi-
CC     = $(PREFIX)gcc
AS     = $(PREFIX)gcc -x assembler-with-cpp
CP     = $(PREFIX)objcopy
SZ     = $(PREFIX)size

######################################
# Flags
######################################
CPU   = -mcpu=cortex-m4
FPU   = -mfpu=fpv4-sp-d16
FLOAT = -mfloat-abi=hard
MCU   = $(CPU) -mthumb $(FPU) $(FLOAT)

OPT   = -O2
CFLAGS  = $(MCU) $(OPT) -Wall -Wextra -fdata-sections -ffunction-sections \
          -std=gnu11 $(C_DEFS) $(C_INCLUDES) -MMD -MP
ASFLAGS = $(MCU) -Wall

LDSCRIPT = STM32G474RETx_FLASH.ld
LIBS     = -lc -lm -lnosys
# -u _printf_float: enable %f in newlib-nano printf/snprintf (telemetry needs it)
LDFLAGS  = $(MCU) -specs=nano.specs -specs=nosys.specs -u _printf_float -T$(LDSCRIPT) $(LIBS) \
           -Wl,-Map=$(BUILD)/$(TARGET).map,--cref -Wl,--gc-sections

######################################
# Build rules
######################################
OBJECTS  = $(addprefix $(BUILD)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD)/,$(notdir $(ASM_SOURCES:.s=.o)))

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex
	$(SZ) $(BUILD)/$(TARGET).elf

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD)/%.o: %.s | $(BUILD)
	$(AS) -c $(ASFLAGS) -o $@ $<

$(BUILD)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(CP) -O binary -S $< $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(CP) -O ihex $< $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

# Flash via OpenOCD (ST-Link). Alternatively use STM32CubeProgrammer:
#   STM32_Programmer_CLI -c port=SWD -w build/navgame.elf -rst
flash: $(BUILD)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32g4x.cfg \
	  -c "program $(BUILD)/$(TARGET).elf verify reset exit"

-include $(wildcard $(BUILD)/*.d)

.PHONY: all clean flash
