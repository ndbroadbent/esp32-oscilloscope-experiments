#!/usr/bin/env python3
"""
Generate Matrix digital rain animation frames for ESP32 oscilloscope

This script creates grayscale frames (128x128) with 5 intensity levels
to simulate the Matrix digital rain effect.
"""

import os
import random
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import time

# Configuration
NUM_FRAMES = 10          # Number of frames in the animation
WIDTH = 128              # Image width
HEIGHT = 128             # Image height
INTENSITY_LEVELS = 5     # Grayscale levels (0-4)
CHARACTERS = [chr(i) for i in range(33, 127)] + list("あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをん")
DROP_CHANCE = 0.02       # Chance of a new drop starting at any column
SPEED_MIN = 1            # Minimum speed of drops
SPEED_MAX = 4            # Maximum speed of drops
TRAIL_MIN = 6            # Minimum trail length
TRAIL_MAX = 20           # Maximum trail length
FONT_SIZE = 10           # Character size
OUTPUT_DIR = "../processed"  # Output directory

# Create output directory if it doesn't exist
os.makedirs(OUTPUT_DIR, exist_ok=True)

def create_matrix_frame(width, height, existing_drops=None):
    """Create a single frame of Matrix digital rain"""
    if existing_drops is None:
        existing_drops = []
    
    # Create a black image
    frame = np.zeros((height, width), dtype=np.uint8)
    
    # Process existing drops and create new ones
    new_drops = []
    
    # Potentially start new drops
    for col in range(0, width, FONT_SIZE):
        if random.random() < DROP_CHANCE and all(d[0] != col for d in existing_drops):
            # Start a new drop: (column, position, speed, trail_length)
            trail_length = random.randint(TRAIL_MIN, TRAIL_MAX)
            new_drop = (col, -trail_length, random.randint(SPEED_MIN, SPEED_MAX), trail_length)
            existing_drops.append(new_drop)
    
    # Process all drops
    for drop in existing_drops:
        col, pos, speed, trail = drop
        
        # Calculate new position
        new_pos = pos + speed
        
        # Only keep the drop if it's still on screen (with its trail)
        if new_pos - trail < height:
            # Draw the trail with decreasing intensity
            for i in range(trail):
                trail_pos = new_pos - i
                if 0 <= trail_pos < height:
                    # Head of the trail is brightest, tail is dimmest
                    intensity = int(INTENSITY_LEVELS * (1 - i / trail))
                    frame[trail_pos, col] = intensity
            
            # Keep the drop with updated position
            new_drops.append((col, new_pos, speed, trail))
    
    return frame, new_drops

def save_frame_as_h_file(frame, frame_num, total_frames):
    """Save the frame data as a C header file"""
    h_file_path = os.path.join(OUTPUT_DIR, f"matrix_frame_{frame_num}.h")
    
    with open(h_file_path, 'w') as f:
        f.write(f"// Generated Matrix animation frame {frame_num} of {total_frames}\n")
        f.write(f"// 128x128 pixels, 5 intensity levels\n\n")
        f.write(f"#ifndef MATRIX_FRAME_{frame_num}_H\n")
        f.write(f"#define MATRIX_FRAME_{frame_num}_H\n\n")
        f.write("#define MATRIX_FRAME_WIDTH 128\n")
        f.write("#define MATRIX_FRAME_HEIGHT 128\n")
        f.write("#define MATRIX_FRAME_INTENSITY_LEVELS 5\n\n")
        
        f.write(f"// Frame {frame_num} data - intensity values (0 = black, 4 = bright)\n")
        f.write(f"const uint8_t matrix_frame_{frame_num}[MATRIX_FRAME_HEIGHT][MATRIX_FRAME_WIDTH] = {{\n")
        
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
        f.write("#endif // MATRIX_FRAME_{frame_num}_H\n")

def create_frame_index_h_file(num_frames):
    """Create a header file with an array of pointers to all frames"""
    h_file_path = os.path.join(OUTPUT_DIR, "matrix_frames.h")
    
    with open(h_file_path, 'w') as f:
        f.write(f"// Matrix animation frames index\n")
        f.write(f"// {num_frames} frames, 128x128 pixels, 5 intensity levels\n\n")
        f.write("#ifndef MATRIX_FRAMES_H\n")
        f.write("#define MATRIX_FRAMES_H\n\n")
        
        # Include all frame header files
        for i in range(num_frames):
            f.write(f"#include \"matrix_frame_{i}.h\"\n")
        
        f.write("\n#define MATRIX_NUM_FRAMES " + str(num_frames) + "\n")
        f.write("#define MATRIX_FPS 10\n\n")
        
        # Create array of frame pointers
        f.write("const uint8_t* const matrix_frames[MATRIX_NUM_FRAMES][MATRIX_FRAME_HEIGHT] = {\n")
        for i in range(num_frames):
            f.write(f"    // Frame {i}\n")
            f.write("    {\n")
            for y in range(HEIGHT):
                f.write(f"        matrix_frame_{i}[{y}],\n")
            f.write("    },\n")
        f.write("};\n\n")
        
        f.write("#endif // MATRIX_FRAMES_H\n")

def main():
    print(f"Generating {NUM_FRAMES} frames of Matrix digital rain animation...")
    start_time = time.time()
    
    # Generate all frames
    drops = []
    for i in range(NUM_FRAMES):
        frame, drops = create_matrix_frame(WIDTH, HEIGHT, drops)
        save_frame_as_h_file(frame, i, NUM_FRAMES)
        print(f"Generated frame {i+1}/{NUM_FRAMES}")
    
    # Create the index file
    create_frame_index_h_file(NUM_FRAMES)
    
    elapsed = time.time() - start_time
    print(f"Done! Generated {NUM_FRAMES} frames in {elapsed:.2f} seconds")
    print(f"Header files saved to {os.path.abspath(OUTPUT_DIR)}")

if __name__ == "__main__":
    main()