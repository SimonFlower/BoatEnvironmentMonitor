## Introduction

This project contains software to run an environmental monitor for a boat
and send the data over a mobile modem to an MQTT broker. The computer used
for the project is a Nucleo-32 STM32L432 development board which uses the
STM32L432KCU6 ARM microprocessor.

During development the Nucleo board attaches to a host computer by USB. When
USB is connected two devices will be created on the host computer:

1. A mass storage device. Programs are uploaded to the board by copying a
   file with the .bin suffix (any filename can be used) to this device.
   The makefile in this project generates a file called build/app.bin.
2. A serial port. The serial port is used to monitor debugging output
   from the program. It can also be used with a debugger.

This project was built using Linux Debian 13. All examples in this
documentation use Linux commands.

## Installing and configuring build software

Software installation for ARM cross compiler using make for building code:

```
  sudo apt install -y \
    build-essential \
    git \
    make \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    gdb-multiarch \
    openocd \
    stlink-tools
```

Software installation for a terminal emulator that will allow access to
output from the Nucleo board over its USB serial port:

```
  sudo apt install -y picocom
```

Examples in this document assume you are using picocom to access the
Nucleo board.

If you need an editor:

```
  sudo apt install -y geany geany-plugins
```

To allow st-flash and openocd to communicate with the Nucleo board over USB without requiring sudo, create a udev rule file for ST-LINK devices. Enter this text in /etc/udev/rules.d/99-stlink.rules

```
  SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374b", MODE="0666", GROUP="plugdev"
```

Reload udev rules and add your user account to the plugdev group:

```
  sudo udevadm control --reload-rules
  sudo udevadm trigger
  sudo usermod -aG plugdev $USER
```

Log out and back in for group membership to take effect.

To allow access to the serial port created by the Nucleo USB connection, add
your user to the 'dialout' group:

```
  sudo usermod -aG dialout $USER```

## Creating a FreeRTOS project

This project was created like this.

```
    mkdir BoatEnvironmentalMonitor
    cd BoatEnvironmentalMonitor
    git init
```

All commands in this README assume a current directory of "BoatEnvironmentalMonitor".

Project structure:

```
    BoatEnvironmentalMonitor/
    ├── makefile
    ├── STM32L432KCXx_FLASH.ld
    ├── inc/
    │   ├── FreeRTOSConfig.h
    ├── src/
    │   ├── main.c
    │   ├── startup_stm32l432kc.s
    ├── drivers/
    │   ├── CMSIS_5                (ARM definitinions)
    │   ├── cmsis_device_l4        (ST Microelectronics definitIons)
    └── FreeRTOS/                  (FreeRTOS Kernel source directory - git submodule)
        ├── include/                (Header files: task.h, queue.h, etc.)
        ├── tasks.c
        ├── queue.c
        ├── list.c
        ├── timers.c
        ├── portable/
        │   ├── GCC/
        │   │   └── ARM_CM4F/       (Cortex-M4 Port for GCC)
        │   │       ├── port.c
        │   │       └── portmacro.h
        │   └── MemMang/
        │       └── heap_4.c        (Heap memory allocator)
```


## Installing FreeRTOS

To install FreeRTOS into the project:

```
    git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git FreeRTOS
```

## Installing ARM and STM32 definitions

These projects provide C header files required for working with the STM32 hardware.

```
    git submodule add https://github.com/ARM-software/CMSIS_5.git drivers/CMSIS_5
    git submodule add https://github.com/STMicroelectronics/cmsis_device_l4.git drivers/cmsis_device_l4
```

## Viewing debug output

The file inc/debug.h defines whether debugging code is built into the program. Debugging
includes writing messages to the Nucleo development boards UART (serial port). These can
be viewed as follows:

Find the serial port created by the USB connection to the Nucleo board:

```
  ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

Nucleo boards almost always enumerate as /dev/ttyACM0 (or /dev/ttyACM1 
if another device is connected). Assuming that the serial port created 
was /dev/ttyACM0, connect to the Nucleo board like this:

```
  picocom -b 115200 /dev/ttyACM0 
```
