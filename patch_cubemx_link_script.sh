#!/usr/bin/env bash
#
# A script that modifies the CubeMX generated link script in 
# CubeMX/STM32L432xx_FLASH.ld to define a block of flash memory
# to reserve for configuration data on the development board.

set -eu

LINKER_SCRIPT="CubeMX/STM32L432xx_FLASH.ld"

if [ ! -f "$LINKER_SCRIPT" ]; then
    echo "ERROR: File not found: $LINKER_SCRIPT" >&2
    exit 1
fi

# See if the old or new configuration exists
old_flash_count=$(grep -F "FLASH" CubeMX/STM32L432xx_FLASH.ld | grep -F "ORIGIN" | grep -c "256K" || true)
discard_count=$(grep -Fc "/DISCARD/" CubeMX/STM32L432xx_FLASH.ld || true)
new_flash_count=$(grep -F "FLASH" CubeMX/STM32L432xx_FLASH.ld | grep -F "ORIGIN" | grep -c "254K" || true)
config_count=$(grep -F "CONFIG" CubeMX/STM32L432xx_FLASH.ld | grep -F "ORIGIN" | grep -c "2K" || true)
if [ $old_flash_count -eq 1 -a $new_flash_count -eq 0 -a $config_count -eq 0 -a $discard_count -eq 1 ] ; then
	: # Linker script needs patching
elif [ $old_flash_count -eq 0 -a $new_flash_count -eq 1 -a $config_count -eq 1 -a $discard_count -eq 1 ] ; then
	echo "Linker script patch already applied"
	exit 0
else
	echo "ERROR: searching for memory definitions in $LINKER_SCRIPT"
	exit 1
fi

# Find the line number for the FLASH memory definition and for the DISCARD section
flash_line_no=$(grep -n "FLASH" CubeMX/STM32L432xx_FLASH.ld | grep -F "ORIGIN" | grep "256K" | cut -d : -f 1)
discard_line_no=$(grep -Fn "/DISCARD/" CubeMX/STM32L432xx_FLASH.ld | cut -d : -f 1)

# Change the FLASH memory length
sed -i 's/LENGTH = 256K/LENGTH = 254K/' $LINKER_SCRIPT

# Add the CONFIG memory section
config_line='CONFIG (rx)    : ORIGIN = 0x803F800, LENGTH = 2K'
sed_cmd="${flash_line_no}a\\${config_line}"

sed -i "$sed_cmd" $LINKER_SCRIPT

# Add the config definitions that can be used in the source code
# The /DISCARD/ line which is the placeholder for these definition
# has a comment before it and blank lines before that - insert the 
# the config definitions 2 lines before that
discard_line_no=$(expr $discard_line_no '-' '2')
config_line='  __config_start__ = ORIGIN(CONFIG);'
sed_cmd="${discard_line_no}a\\${config_line}"
sed -i "$sed_cmd" $LINKER_SCRIPT
discard_line_no=$(expr $discard_line_no '+' '1')
config_line='  __config_end__   = ORIGIN(CONFIG) + LENGTH(CONFIG);'
sed_cmd="${discard_line_no}a\\${config_line}"
sed -i "$sed_cmd" $LINKER_SCRIPT

echo "Linker script patch complete."
