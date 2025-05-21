#!/usr/bin/env python3
"""
Process an image to create a 32x32 5-shade grayscale version.
This script takes an input image, resizes it to 32x32 pixels,
converts it to grayscale, and quantizes it to 5 intensity levels.
It then saves the processed image and creates a C header file
with the pixel data for use with the ESP32 oscilloscope demo.
"""

import os
import sys
import numpy as np
from PIL import Image

# Constants
TARGET_SIZE = 128  # image dimensions
INTENSITY_LEVELS = 5  # Number of intensity levels (0-2) for better performance

def process_image(input_path, output_dir="./processed"):
    """Process the input image and save results."""
    # Make sure output directory exists
    os.makedirs(output_dir, exist_ok=True)
    
    # Get base filename
    base_name = os.path.splitext(os.path.basename(input_path))[0]
    
    # Load and process image
    try:
        # Open and resize image
        img = Image.open(input_path)
        img = img.convert("L")  # Convert to grayscale
        img = img.resize((TARGET_SIZE, TARGET_SIZE), Image.LANCZOS)
        
        # Normalize and quantize to intensity levels
        img_array = np.array(img)
        # Normalize to 0-1
        img_array = img_array / 255.0
        # Quantize to specified number of levels
        quantized = np.floor(img_array * (INTENSITY_LEVELS - 1) + 0.5).astype(np.uint8)
        
        # Save processed image (for visual verification)
        # Rescale to full 0-255 range for better visibility
        display_img = Image.fromarray((quantized * (255 // (INTENSITY_LEVELS - 1))).astype(np.uint8))
        output_image_path = os.path.join(output_dir, f"{base_name}_processed.png")
        display_img.save(output_image_path)
        print(f"Processed image saved to {output_image_path}")
        
        # Generate C header file
        header_path = os.path.join(output_dir, f"{base_name}_image.h")
        with open(header_path, "w") as f:
            f.write(f"// Generated grayscale image data: {base_name}\n")
            f.write(f"// {TARGET_SIZE}x{TARGET_SIZE} pixels, {INTENSITY_LEVELS} intensity levels\n\n")
            f.write("#ifndef GRAYSCALE_IMAGE_DATA_H\n")
            f.write("#define GRAYSCALE_IMAGE_DATA_H\n\n")
            f.write("#define IMAGE_WIDTH " + str(TARGET_SIZE) + "\n")
            f.write("#define IMAGE_HEIGHT " + str(TARGET_SIZE) + "\n")
            f.write("#define IMAGE_INTENSITY_LEVELS " + str(INTENSITY_LEVELS) + "\n\n")
            f.write("// Intensity values (0 = black, " + 
                   str(INTENSITY_LEVELS-1) + " = white)\n")
            f.write("const uint8_t grayscale_image[IMAGE_HEIGHT][IMAGE_WIDTH] = {\n")
            
            # Write the pixel data
            for y in range(TARGET_SIZE):
                f.write("    {")
                for x in range(TARGET_SIZE):
                    f.write(str(quantized[y, x]))
                    if x < TARGET_SIZE - 1:
                        f.write(", ")
                f.write("}")
                if y < TARGET_SIZE - 1:
                    f.write(",")
                f.write("\n")
            
            f.write("};\n\n")
            f.write("#endif // GRAYSCALE_IMAGE_DATA_H\n")
        
        print(f"Header file generated at {header_path}")
        return True
    except Exception as e:
        print(f"Error processing image: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python process_image.py <input_image> [output_directory]")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "./processed"
    
    if not os.path.exists(input_path):
        print(f"Error: Input file {input_path} not found.")
        sys.exit(1)
    
    success = process_image(input_path, output_dir)
    sys.exit(0 if success else 1)
