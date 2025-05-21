#!/bin/bash
# Script to process an image for the ESP32 oscilloscope grayscale demo
# Installs required dependencies and runs the Python script

set -e  # Exit on error

# Path to the repo
REPO_PATH="$(cd "$(dirname "$0")/.." && pwd)"
SCRIPT_PATH="$REPO_PATH/scripts"
PROCESSED_PATH="$REPO_PATH/processed"
INPUT_IMAGE="${1:-$REPO_PATH/pictures/masha2.png}"
OUTPUT_DIR="${2:-$PROCESSED_PATH}"

# Check if input image exists
if [ ! -f "$INPUT_IMAGE" ]; then
    echo "Error: Input image $INPUT_IMAGE not found."
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Use Python in a virtual environment
echo "Setting up Python environment with required packages..."
VENV_PATH="$SCRIPT_PATH/.venv"

# Create virtual environment if it doesn't exist
if [ ! -d "$VENV_PATH" ]; then
    echo "Creating virtual environment..."
    python3 -m venv "$VENV_PATH"
fi

# Activate virtual environment
source "$VENV_PATH/bin/activate"

# Install required packages
echo "Installing required packages..."
pip install --quiet Pillow numpy

# Run the Python script
echo "Processing image $INPUT_IMAGE..."
python "$SCRIPT_PATH/process_image.py" "$INPUT_IMAGE" "$OUTPUT_DIR"

# Copy the header file to the examples directory
HEADER_FILE="$OUTPUT_DIR/$(basename "${INPUT_IMAGE%.*}")_image.h"
EXAMPLES_PATH="$REPO_PATH/examples"

if [ -f "$HEADER_FILE" ]; then
    cp "$HEADER_FILE" "$EXAMPLES_PATH/"
    echo "Header file copied to $EXAMPLES_PATH/$(basename "$HEADER_FILE")"
else
    echo "Error: Header file was not generated."
    exit 1
fi

echo "Image processing complete."
exit 0