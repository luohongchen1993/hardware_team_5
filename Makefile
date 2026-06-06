##########################################################
# Makefile for STM32G474RE — Health Monitor Firmware
# Toolchain: arm-none-eabi-gcc
# Adjust CUBE_HAL_DIR to point at your STM32CubeG4 HAL root
##########################################################

TARGET   = health_monitor
BUILD    = build
CUBE_HAL_DIR ?= $(HOME)/STM32Cube/Repository/STM32Cube_FW_G4_V1.5.0

######################################
# Sources
######################################
C_SOURCES  = Core/Src/main.c
C_SOURCES += Core/Src/mpu6050.c
C_SOURCES += Core/Src/distance_sensor.c
C_SOURCES += Core/Src/temp_sensor.c
C_SOURCES += Core/Src/servo.c
C_SOURCES += Core/Src/led.c
C_SOURCES += Core/Src/serial_log.c
C_SOURCES += Core/Src/health_monitor.c

# HAL driver sources (subset — add more as needed)
HAL_SRC = $(CUBE_HAL_DIR)/Drivers/STM32G4xx_HAL_Driver/Src
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_rcc.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_rcc_ex.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_cortex.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_gpio.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_i2c.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_i2c_ex.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_uart.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_uart_ex.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_tim.c
C_SOURCES += $(HAL_SRC)/stm32g4xx_hal_tim_ex.c

# Startup file (adjust path for your Cube install)
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
# Compiler flags
######################################
CPU   = -mcpu=cortex-m4
FPU   = -mfpu=fpv4-sp-d16
FLOAT = -mfloat-abi=hard
MCU   = $(CPU) -mthumb $(FPU) $(FLOAT)

OPT   = -O2
CFLAGS  = $(MCU) $(OPT) -Wall -Wextra -fdata-sections -ffunction-sections \
          -std=c11 $(C_DEFS) $(C_INCLUDES) -MMD -MP
ASFLAGS = $(MCU) -Wall

LDSCRIPT = STM32G474RETx_FLASH.ld
LIBS     = -lc -lm -lnosys
LDFLAGS  = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBS) \
           -Wl,-Map=$(BUILD)/$(TARGET).map,--cref -Wl,--gc-sections

######################################
# Build rules
######################################
OBJECTS  = $(addprefix $(BUILD)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD)/,$(notdir $(ASM_SOURCES:.s=.o)))

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin
	$(SZ) $<

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD)/%.o: %.s | $(BUILD)
	$(AS) -c $(ASFLAGS) -o $@ $<

$(BUILD)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(CP) -O binary -S $< $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

flash: $(BUILD)/$(TARGET).bin
	openocd -f interface/stlink.cfg -f target/stm32g4x.cfg \
	  -c "program $(BUILD)/$(TARGET).bin verify reset exit 0x08000000"

-include $(wildcard $(BUILD)/*.d)

.PHONY: all clean flash
