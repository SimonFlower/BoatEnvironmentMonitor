# Toolchain definitions
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# Microcontroller target flags (Cortex-M4 with FPU)
MCU_FLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# Include directories
INCLUDES = -Iinc \
           -Idrivers/CMSIS_5/CMSIS/Core/Include \
           -Idrivers/cmsis_device_l4/Include \
           -IFreeRTOS/include \
           -IFreeRTOS/portable/GCC/ARM_CM4F

# Compiler flags
CFLAGS  = $(MCU_FLAGS) -g -Os -Wall $(INCLUDES) -DSTM32L432xx

# Linker flags
LDFLAGS = $(MCU_FLAGS) -T STM32L432KCXx_FLASH.ld -Wl,--gc-sections --specs=nano.specs

# Source files
SRCS = src/main.c \
       src/startup_stm32l432kc.s \
       drivers/cmsis_device_l4/Source/Templates/system_stm32l4xx.c \
       FreeRTOS/tasks.c \
       FreeRTOS/queue.c \
       FreeRTOS/list.c \
       FreeRTOS/timers.c \
       FreeRTOS/portable/GCC/ARM_CM4F/port.c \
       FreeRTOS/portable/MemMang/heap_4.c

# Object files output mapping
OBJS = $(addprefix build/, $(notdir $(addsuffix .o, $(basename $(SRCS)))))

TARGET = build/app

# Search paths for source files
vpath %.c src drivers/cmsis_device_l4/Source/Templates FreeRTOS FreeRTOS/portable/GCC/ARM_CM4F FreeRTOS/portable/MemMang
vpath %.s src

all: $(TARGET).bin

build/%.o: %.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.s | build
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

build:
	mkdir -p build

flash: $(TARGET).bin
	st-flash --connect-under-reset write $(TARGET).bin 0x08000000

clean:
	rm -rf build

.PHONY: all flash clean
