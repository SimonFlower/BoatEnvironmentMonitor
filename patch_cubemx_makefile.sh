#!/usr/bin/env bash
#
# A script that modifies the CubeMX generated Makefile (in CubeMX/Makefile) to
# add in sources used by this application so that running "make" in the
# CubeMX directory builds the whole application (the application code is in
# the top level "App" directory).

set -e

MAKEFILE="CubeMX/Makefile"

if [ ! -f "$MAKEFILE" ]; then
    echo "Error: $MAKEFILE not found!"
    exit 1
fi

# Check if already patched
if grep -q "EXTRA_C_SOURCES" "$MAKEFILE"; then
    exit 0
fi

echo "Patching $MAKEFILE..."

# Insert EXTRA_C_SOURCES and EXTRA_C_INCLUDES before OBJECTS is constructed
sed -i '/^# list of objects/i \
# Custom Application Overrides (Auto-patched)\n\
C_SOURCES += $(EXTRA_C_SOURCES)\n\
C_INCLUDES += $(EXTRA_C_INCLUDES)\n' "$MAKEFILE"

echo "$MAKEFILE successfully patched."
