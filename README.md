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

If you want to debug the code, use ddd:

```
  sudo apt install -y ddd
```

Examples in this document assume you are using ddd for debugging.

Software installation for a terminal emulator that will allow access to
debug output from the Nucleo board over its USB serial port:

```
  sudo apt install -y picocom
```

Examples in this document assume you are using the picocom terminal emulator
to access the Nucleo board.

If you need a source code editor:

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
  sudo usermod -aG dialout $USER
```

To allow access to the ST-LINK programmer:

```
  sudo usermod -aG plugdev $USER
```

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
    git submodule add https://github.com/STMicroelectronics/stm32l4xx_hal_driver drivers/stm32l4xx_hal_driver
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

## Using the ST-LINK debugger

The Nucleo-32 development board includes the ST-LINK debugger. To access
the debugger, run openocd to connect to the ST-LINK, then run ddd to
provide a friendly user iterface to the debugger.

Run openocd as a background service. You need to do this each time you
start the computer that runs your development environment. The configuration
files are found in /usr/share/openocd/scripts/. This command will block
the terminal where it's issued (unless you terminate it with '&' to place
it in the background):

```
   openocd -f interface/stlink-v2-1.cfg -f target/stm32l4x.cfg
```

NOTE: Once openocd is running, don't use the "make flash" command,
as the underlying st-flash program requires the same USB interface
as openocd. Instead use the debuggers "load" command to upload a
newly compiled program onto the board (see below for an example of
using "load").

Next run ddd, telling it where to find the program being used on
the Nucleo-32 development board:

```
   ddd --debugger gdb-multiarch build/app.elf
```

Inside the ddd command console (at the "gdb" prompt) enter these commands:

```
    target remote localhost:3333    # connect to openocd
    monitor reset halt              # restart
    load                            # upload the program to the Nucleo board
```

You can now place graphical breakpoints directly on lines of code, 
step through instructions, and view target RAM/registers.




## Draft MQTT payload details

Example:

```
{
    "date": "yyyy-mm-ddThh:mm:ss",
    "batteries": {
        "1": "xx.xV",
        "2": "xx.xV",
        "3": "xx.xV"
    },
    "temperatures": {
        "1": "xx.xC",
        "2": "xx.xC",
        "3": "xx.xC",
        "4": "xx.xC",
        "5": "xx.xC"
    },
    "humidities": {
        "1": "xx.x%",
        "2": "xx.x%",
        "3": "xx.x%",
        "4": "xx.x%",
        "5": "xx.x%"
    },
    "mains_present": true
}
```

Size about 0.5Kb. Allow another 0.5Kb for MQTT set up = 1Kb every 15 minutes, or about 100Kb per day or about 3.5Mb per month.
