#!/bin/bash

# OTA firmware upload script for ring devices
# Usage: ./ota_upload_all.sh [ring_numbers...]
#   No arguments: uploads to all rings (1-12)
#   With arguments: uploads only to specified rings
#   Examples:
#     ./ota_upload_all.sh           # all rings 1-12
#     ./ota_upload_all.sh 1 3 5     # only rings 1, 3, 5
#     ./ota_upload_all.sh 2 7       # only rings 2, 7

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PIO_ENV="default"
FIRMWARE="$SCRIPT_DIR/.pio/build/$PIO_ENV/firmware.bin"
ESPOTA="$HOME/.platformio/packages/framework-arduinoespressif32/tools/espota.py"
PIO="$HOME/.platformio/penv/Scripts/pio"
PYTHON="$HOME/.platformio/penv/Scripts/python"
OTA_PORT=3232

# Devices: use arguments if provided, otherwise all rings 1-12
if [ $# -gt 0 ]; then
    DEVICES=("$@")
else
    DEVICES=(1 2 3 4 5 6 7 8 9 10 11 12)
fi

# Build firmware
echo "========================================"
echo "Building firmware (env: $PIO_ENV)..."
echo "========================================"
cd "$SCRIPT_DIR"
"$PIO" run -e "$PIO_ENV"

if [ ! -f "$FIRMWARE" ]; then
    echo "ERROR: Firmware not found at $FIRMWARE"
    exit 1
fi

echo ""
echo "Firmware built successfully: $FIRMWARE"
echo ""

# Upload to each device
succeeded=()
failed=()

for num in "${DEVICES[@]}"; do
    host="ring${num}.local"
    echo "========================================"
    echo "Uploading to $host ..."
    echo "========================================"

    if "$PYTHON" "$ESPOTA" -i "$host" -p "$OTA_PORT" -f "$FIRMWARE"; then
        echo "OK: $host"
        succeeded+=("$host")
    else
        echo "FAILED: $host"
        failed+=("$host")
    fi
    echo ""
done

# Summary
echo "========================================"
echo "UPLOAD SUMMARY"
echo "========================================"
echo "Succeeded (${#succeeded[@]}/${#DEVICES[@]}):"
for h in "${succeeded[@]}"; do
    echo "  + $h"
done

if [ ${#failed[@]} -gt 0 ]; then
    echo ""
    echo "Failed (${#failed[@]}/${#DEVICES[@]}):"
    for h in "${failed[@]}"; do
        echo "  - $h"
    done
    exit 1
fi

echo ""
echo "All devices updated successfully!"
