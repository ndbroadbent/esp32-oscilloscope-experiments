#!/bin/bash
# Reset all environment variables that might interfere with ESP-IDF
unset PYTHONPATH
unset PYTHONHOME
unset VIRTUAL_ENV

# Debug information
echo "Current Python: $(which python)"
echo "Current Python3: $(which python3)"

# Ensure we're in the project directory
cd "$(dirname "$0")"

# Install click if needed
pip install click

# Source ESP-IDF environment with full debugging
echo "Sourcing ESP-IDF environment..."
. $HOME/esp/esp-idf/export.sh

# Verify Python environment
python -c "import click; print('ESP-IDF Python environment is working with click module')"

# Build and flash
echo "Building project..."
idf.py build

# Only flash if build was successful
if [ $? -eq 0 ]; then
    echo "Flashing to device..."
    idf.py -p /dev/tty.usbserial-0001 flash
else
    echo "Build failed, not flashing"
fi