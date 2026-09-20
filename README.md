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

If you need an editor:

```
  sudo apt install geany geany-plugins
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


## Create a project

```
    mkdir my-project
    cd my-project
    git init
```

All commands in this README assume a current directory of "my-project".

Project structure:

```
    my-project/
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

