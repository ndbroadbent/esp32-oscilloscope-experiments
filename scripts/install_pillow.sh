#!/bin/bash

# Create a virtual environment for installing Pillow
echo "Creating Python virtual environment..."
python3 -m venv venv
source venv/bin/activate

# Install Pillow in the virtual environment
echo "Installing Pillow..."
pip install Pillow

echo "Installation complete. You can now use the script with:"
echo "source venv/bin/activate"
echo "python3 process_matrix_gif.py"