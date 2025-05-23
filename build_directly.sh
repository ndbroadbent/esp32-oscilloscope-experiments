#!/bin/bash
# Direct build script that uses full paths and avoids environment issues

# Source ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Go to project directory
cd "$(dirname "$0")"

# Build and flash
$HOME/esp/esp-idf/tools/idf.py build
$HOME/esp/esp-idf/tools/idf.py -p /dev/tty.usbserial-0001 flash