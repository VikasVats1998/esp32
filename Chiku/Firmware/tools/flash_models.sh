#!/usr/bin/env bash
# Writes the ESP-SR speech models (wake word "Hi ESP" + command recogniser) to the 'model' partition.
# Needed once per board, and again after anything that erases the whole flash.
#   Chiku/Firmware/tools/flash_models.sh [/dev/ttyACM0]
set -euo pipefail
PORT="${1:-/dev/ttyACM0}"
MODELS="$(ls ~/.platformio/packages/framework-arduinoespressif32-libs/esp32s3/esp_sr/srmodels.bin)"
ESPTOOL="${ESPTOOL:-$(dirname "$0")/../../Reference/DeskBuddy/.venv/bin/python -m esptool}"
# 0xD10000 = 'model' in esp_sr_16.csv (see platformio.ini)
$ESPTOOL --chip esp32s3 --port "$PORT" --baud 460800 write_flash 0xD10000 "$MODELS"
