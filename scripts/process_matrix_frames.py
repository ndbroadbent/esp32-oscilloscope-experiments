#!/usr/bin/env python3
"""
Generate Matrix digital rain animation frames for ESP32 oscilloscope

This script creates a simulated Matrix digital rain effect without requiring
external libraries like PIL. It generates a series of frames directly.
"""

import os
import random
import time

# Configuration
NUM_FRAMES = 50         # 5 seconds at 10 FPS
WIDTH = 128             # Image width
HEIGHT = 128            # Image height
INTENSITY_LEVELS = 5    # Grayscale levels (0-4)
OUTPUT_DIR = "/Users/ndbroadbent/code/esp32-scope/processed"
DROP_CHANCE = 0.03      # Chance of a new drop starting in any column
MIN_SPEED = 1           # Minimum speed of drops
MAX_SPEED = 4           # Maximum speed of drops
MIN_TRAIL = 8           # Minimum trail length
MAX_TRAIL = 20          # Maximum trail length
CHAR_WIDTH = 3          # Width of each character column

# Initialize random seed for reproducibility
random.seed(42)

# Ensure output directory exists
os.makedirs(OUTPUT_DIR, exist_ok=True)

# Drops are represented as: (column, position, speed, trail_length)
drops = []

def create_frame(width, height, drops):
    """Create a single frame with the matrix digital rain effect"""
    # Initialize a black frame (all zeros)
    frame = [[0 for _ in range(width)] for _ in range(height)]
    
    # Update existing drops and potentially create new ones
    new_drops = []
    
    # Create new drops with some probability
    for col in range(0, width, CHAR_WIDTH):
        if random.random() < DROP_CHANCE and all(drop[0] != col for drop in drops):
            trail_length = random.randint(MIN_TRAIL, MAX_TRAIL)
            speed = random.randint(MIN_SPEED, MAX_SPEED)
            new_drop = (col, -trail_length, speed, trail_length)
            drops.append(new_drop)
    
    # Update and draw all drops
    for drop in drops:
        col, pos, speed, trail = drop
        
        # Update the drop position
        new_pos = pos + speed
        
        # Only keep drops that are still on screen (with trail)
        if new_pos - trail < height:
            # Draw the drop and its trail
            for i in range(trail):
                y_pos = new_pos - i
                if 0 <= y_pos < height:
                    # Calculate intensity - brightest at head, dimming along trail
                    intensity = int((1 - i/trail) * (INTENSITY_LEVELS-1))
                    if intensity > 0:
                        # Make character width columns
                        for x_offset in range(min(CHAR_WIDTH, width - col)):
                            frame[y_pos][col + x_offset] = intensity
            
            # Keep the drop with updated position
            new_drops.append((col, new_pos, speed, trail))
    
    # Replace the old drops with the updated ones
    drops.clear()
    drops.extend(new_drops)
    
    return frame

def save_frame_as_h_file(frame, frame_num):
    """Save a frame as a C header file"""
    h_file_path = os.path.join(OUTPUT_DIR, f"matrix_frame_{frame_num}.h")
    
    with open(h_file_path, 'w') as f:
        f.write(f"// Generated Matrix animation frame {frame_num+1} of {NUM_FRAMES}\n")
        f.write(f"// 128x128 pixels, {INTENSITY_LEVELS} intensity levels\n\n")
        f.write(f"#ifndef MATRIX_FRAME_{frame_num}_H\n")
        f.write(f"#define MATRIX_FRAME_{frame_num}_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("#define MATRIX_FRAME_WIDTH 128\n")
        f.write("#define MATRIX_FRAME_HEIGHT 128\n")
        f.write(f"#define MATRIX_FRAME_INTENSITY_LEVELS {INTENSITY_LEVELS}\n\n")
        
        f.write(f"// Frame {frame_num+1} data - intensity values (0 = black, {INTENSITY_LEVELS-1} = bright)\n")
        f.write(f"const uint8_t matrix_frame_{frame_num}[MATRIX_FRAME_HEIGHT][MATRIX_FRAME_WIDTH] = {{\n")
        
        for y in range(HEIGHT):
            f.write("    {")
            for x in range(WIDTH):
                f.write(f"{frame[y][x]}")
                if x < WIDTH - 1:
                    f.write(", ")
            f.write("}")
            if y < HEIGHT - 1:
                f.write(",")
            f.write("\n")
        
        f.write("};\n\n")
        f.write(f"#endif // MATRIX_FRAME_{frame_num}_H\n")

def create_animation_header(num_frames):
    """Create the main animation header file"""
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
        f.write(f"#define MATRIX_FPS 10\n\n")
        
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
    """Main function"""
    print(f"Generating {NUM_FRAMES} Matrix animation frames...")
    start_time = time.time()
    
    # Generate and save each frame
    global_drops = []
    for i in range(NUM_FRAMES):
        frame = create_frame(WIDTH, HEIGHT, global_drops)
        save_frame_as_h_file(frame, i)
        print(f"Generated frame {i+1}/{NUM_FRAMES}")
    
    # Create the animation header
    create_animation_header(NUM_FRAMES)
    
    elapsed = time.time() - start_time
    print(f"\nSuccess! Generated {NUM_FRAMES} frames in {elapsed:.2f} seconds")
    print(f"Frame rate: 10 FPS")
    print(f"Header files saved to {os.path.abspath(OUTPUT_DIR)}")

if __name__ == "__main__":
    main()