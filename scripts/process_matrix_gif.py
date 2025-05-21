#!/usr/bin/env python3
"""
Process Matrix GIF animation for ESP32 oscilloscope display

This script:
1. Loads the matrix.gif file
2. Resizes to 128x128 if needed
3. Converts to grayscale with 5 intensity levels
4. Generates C header files containing the animation frames
"""

import os
import sys
import time
from PIL import Image, ImageSequence
import numpy as np

# Configuration
INPUT_FILE = "../pictures/matrix.gif"
OUTPUT_DIR = "../processed"
WIDTH = 128
HEIGHT = 128
INTENSITY_LEVELS = 5
FPS = 10  # Target frames per second

def ensure_dir(directory):
    """Create directory if it doesn't exist"""
    if not os.path.exists(directory):
        os.makedirs(directory)

def process_gif():
    """Process GIF file and extract frames as header files"""
    # Make sure output directory exists
    ensure_dir(OUTPUT_DIR)
    
    print(f"Processing {INPUT_FILE}...")
    
    try:
        # Open the GIF file
        with Image.open(INPUT_FILE) as gif:
            # Get basic info
            frame_count = 0
            for frame in ImageSequence.Iterator(gif):
                frame_count += 1
            
            print(f"GIF info: {gif.size[0]}x{gif.size[1]}, {frame_count} frames")
            
            # Process each frame
            frames = []
            for i, frame in enumerate(ImageSequence.Iterator(gif)):
                # Convert to RGB mode
                rgb_frame = frame.convert('RGB')
                
                # Resize to target dimensions
                if rgb_frame.size != (WIDTH, HEIGHT):
                    rgb_frame = rgb_frame.resize((WIDTH, HEIGHT), Image.LANCZOS)
                
                # Convert to grayscale
                gray_frame = rgb_frame.convert('L')
                
                # Convert to numpy array
                frame_data = np.array(gray_frame)
                
                # Scale to the target intensity levels (0-4)
                # Map 0-255 to 0-(INTENSITY_LEVELS-1)
                frame_data = (frame_data * (INTENSITY_LEVELS - 1) / 255).astype(np.uint8)
                
                # Store the processed frame
                frames.append(frame_data)
                
                # Print progress
                print(f"Processed frame {i+1}/{frame_count}")
            
            # Generate header files
            generate_frame_headers(frames)
            
            return len(frames)
    
    except Exception as e:
        print(f"Error processing GIF: {e}")
        return 0

def generate_frame_headers(frames):
    """Generate C header files for each frame"""
    num_frames = len(frames)
    
    # Generate individual frame header files
    for i, frame in enumerate(frames):
        h_file_path = os.path.join(OUTPUT_DIR, f"matrix_frame_{i}.h")
        
        with open(h_file_path, 'w') as f:
            f.write(f"// Generated Matrix animation frame {i+1} of {num_frames}\n")
            f.write(f"// 128x128 pixels, {INTENSITY_LEVELS} intensity levels\n\n")
            f.write(f"#ifndef MATRIX_FRAME_{i}_H\n")
            f.write(f"#define MATRIX_FRAME_{i}_H\n\n")
            f.write("#include <stdint.h>\n\n")
            f.write("#define MATRIX_FRAME_WIDTH 128\n")
            f.write("#define MATRIX_FRAME_HEIGHT 128\n")
            f.write(f"#define MATRIX_FRAME_INTENSITY_LEVELS {INTENSITY_LEVELS}\n\n")
            
            f.write(f"// Frame {i+1} data - intensity values (0 = black, {INTENSITY_LEVELS-1} = bright)\n")
            f.write(f"const uint8_t matrix_frame_{i}[MATRIX_FRAME_HEIGHT][MATRIX_FRAME_WIDTH] = {{\n")
            
            for y in range(HEIGHT):
                f.write("    {")
                for x in range(WIDTH):
                    f.write(f"{frame[y, x]}")
                    if x < WIDTH - 1:
                        f.write(", ")
                f.write("}")
                if y < HEIGHT - 1:
                    f.write(",")
                f.write("\n")
            
            f.write("};\n\n")
            f.write(f"#endif // MATRIX_FRAME_{i}_H\n")
    
    # Generate master header file with frame array
    h_file_path = os.path.join(OUTPUT_DIR, "matrix_animation.h")
    
    with open(h_file_path, 'w') as f:
        f.write("// Matrix animation data\n")
        f.write(f"// {num_frames} frames, 128x128 pixels, {INTENSITY_LEVELS} intensity levels\n\n")
        f.write("#ifndef MATRIX_ANIMATION_H\n")
        f.write("#define MATRIX_ANIMATION_H\n\n")
        f.write("#include <stdint.h>\n\n")
        
        # Include all frame headers
        for i in range(num_frames):
            f.write(f"#include \"matrix_frame_{i}.h\"\n")
        
        f.write("\n#define MATRIX_NUM_FRAMES " + str(num_frames) + "\n")
        f.write(f"#define MATRIX_FPS {FPS}\n\n")
        
        # Create array of frame pointers for easy access
        f.write("const uint8_t* const matrix_frames[MATRIX_NUM_FRAMES][MATRIX_FRAME_HEIGHT] = {\n")
        for i in range(num_frames):
            f.write(f"    // Frame {i+1}\n")
            f.write("    {\n")
            for y in range(HEIGHT):
                f.write(f"        matrix_frame_{i}[{y}],\n")
            f.write("    },\n")
        f.write("};\n\n")
        
        f.write("#endif // MATRIX_ANIMATION_H\n")

def main():
    start_time = time.time()
    
    # Process the GIF file
    num_frames = process_gif()
    
    if num_frames > 0:
        elapsed = time.time() - start_time
        print(f"\nSuccess! Processed {num_frames} frames in {elapsed:.2f} seconds")
        print(f"Frame rate: {FPS} FPS")
        print(f"Header files saved to {os.path.abspath(OUTPUT_DIR)}")
    else:
        print("Error: Failed to process GIF file")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())