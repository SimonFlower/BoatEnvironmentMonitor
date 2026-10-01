# ==============================================================================
# Top-Level Makefile (BoatEnvironmentalMonitor)
#
# The main build is performed using the Makefile in the CubeMX folder, which
# is managed by the CubeMX software. This makefile supplements the CubeMX
# makefile, adding sources from the App folder. This is done by adding
# to the list of sources and list of include files in the CubeMX Makefile
# using the patch_cubemx_makefile.sh script, which adds the extra sources.
# This means that CubeMX can regenerate the Makefile without affecting the build.
# ==============================================================================

CUBEMX_DIR = CubeMX

# Custom application paths (absolute paths)
APP_C_SOURCES = $(shell pwd)/App/Src/app.c \
			    $(shell pwd)/App/Src/modem_at.c \
			    $(shell pwd)/App/Src/modem_ll.c \
			    $(shell pwd)/App/Src/ring.c \
			    $(shell pwd)/App/Src/debug.c \
			    $(shell pwd)/App/Src/app_iwdg.c \
			    $(shell pwd)/App/Src/led.c \
			    $(shell pwd)/App/Src/utils.c
APP_C_INCLUDES = -I$(shell pwd)/App/Inc

.PHONY: all clean flash patch

all: patch
	$(MAKE) -C $(CUBEMX_DIR) \
		EXTRA_C_SOURCES="$(APP_C_SOURCES)" \
		EXTRA_C_INCLUDES="$(APP_C_INCLUDES)"

patch:
	@./patch_cubemx_makefile.sh

clean:
	$(MAKE) -C $(CUBEMX_DIR) clean

flash: all
	@echo "Flash starting..."
	openocd \
		-f interface/stlink-v2-1.cfg \
		-f target/stm32l4x.cfg \
		-c "program $(CUBEMX_DIR)/build/CubeMX.elf verify reset exit"
	@echo "Flash completed OK"
