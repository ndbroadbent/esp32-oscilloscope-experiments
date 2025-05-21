#!/usr/bin/env python3
"""
Script to extract frames from matrix.gif using only standard libraries
and convert them to C header files for ESP32 oscilloscope display
"""

import os
import sys
import subprocess
import tempfile
import time

# Configuration
INPUT_FILE = "/Users/ndbroadbent/code/esp32-scope/pictures/matrix.gif"
OUTPUT_DIR = "/Users/ndbroadbent/code/esp32-scope/processed"
INTENSITY_LEVELS = 5
FPS = 10  # Target frames per second
WIDTH = 128
HEIGHT = 128

def ensure_directory(directory):
    """Ensure directory exists"""
    if not os.path.exists(directory):
        os.makedirs(directory)

def extract_gif_frames():
    """Extract frames from GIF using system tools"""
    print(f"Processing {INPUT_FILE}...")
    
    # Ensure output directory exists
    ensure_directory(OUTPUT_DIR)
    
    # Create temporary directory for extracted frames
    with tempfile.TemporaryDirectory() as temp_dir:
        # Use ffmpeg to extract frames (if available)
        try:
            print("Extracting frames with ffmpeg...")
            cmd = [
                'ffmpeg', '-i', INPUT_FILE, 
                '-vf', f'scale={WIDTH}:{HEIGHT}', 
                f'{temp_dir}/frame_%03d.png'
            ]
            subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except (subprocess.SubprocessError, FileNotFoundError):
            print("ffmpeg not available or failed. Trying with convert (ImageMagick)...")
            try:
                # Try ImageMagick's convert as an alternative
                cmd = [
                    'convert', INPUT_FILE, 
                    '-coalesce', '-resize', f'{WIDTH}x{HEIGHT}', 
                    f'{temp_dir}/frame_%03d.png'
                ]
                subprocess.run(cmd, check=True)
            except (subprocess.SubprocessError, FileNotFoundError):
                print("Error: Cannot extract GIF frames. Please install ffmpeg or ImageMagick.")
                return []
        
        # Get all extracted frame files
        frame_files = sorted([os.path.join(temp_dir, f) for f in os.listdir(temp_dir) if f.endswith('.png')])
        frame_count = len(frame_files)
        
        if frame_count == 0:
            print("Error: No frames extracted from GIF.")
            return []
            
        print(f"Successfully extracted {frame_count} frames.")
        
        # Process each frame to create header files
        for i, frame_file in enumerate(frame_files):
            # Get frame dimensions and pixel data
            process_frame_to_header(frame_file, i, frame_count)
            print(f"Processed frame {i+1}/{frame_count}")
            
        # Create index header file
        create_animation_header(frame_count)
        
        return frame_files

def get_pixel_brightness(r, g, b):
    """Calculate brightness from RGB values (0-255 to 0-4)"""
    # Simple average of RGB values, scaled to intensity levels
    return min(int(((r + g + b) / 3) * (INTENSITY_LEVELS - 1) / 255), INTENSITY_LEVELS - 1)

def process_frame_to_header(frame_file, frame_num, total_frames):
    """Convert a frame image to a C header file with intensity values"""
    # Use Python's built-in modules to read the image data
    h_file_path = os.path.join(OUTPUT_DIR, f"matrix_frame_{frame_num}.h")
    
    # Extract image data using system tools to avoid needing PIL
    try:
        # Use convert to get the pixel data as text
        cmd = [
            'convert', frame_file, '-depth', '8', '-colorspace', 'gray', 
            '-resize', f'{WIDTH}x{HEIGHT}!', 'txt:-'
        ]
        result = subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        # Parse the output (which is in the format: x,y: (r,g,b) #RRGGBB)
        lines = result.stdout.decode('utf-8').split('\n')
        
        # Initialize frame data as all zeros
        frame_data = [[0 for _ in range(WIDTH)] for _ in range(HEIGHT)]
        
        # Skip the first line (header) and process pixel data
        for line in lines[1:]:
            if not line.strip():
                continue
            
            parts = line.split(': ')
            if len(parts) < 2:
                continue
                
            coords = parts[0].split(',')
            if len(coords) < 2:
                continue
                
            try:
                x, y = int(coords[0]), int(coords[1])
                
                # Extract grayscale value
                # Format could be "(r,g,b)" or "(gray)"
                color_part = parts[1].split(' ')[0].strip('()')
                if ',' in color_part:
                    values = [int(v) for v in color_part.split(',')]
                    if len(values) >= 3:
                        r, g, b = values[0], values[1], values[2]
                        intensity = get_pixel_brightness(r, g, b)
                    else:
                        intensity = int(int(values[0]) * (INTENSITY_LEVELS - 1) / 255)
                else:
                    # Single grayscale value
                    gray = int(color_part)
                    intensity = int(gray * (INTENSITY_LEVELS - 1) / 255)
                
                if 0 <= x < WIDTH and 0 <= y < HEIGHT:
                    frame_data[y][x] = intensity
            except ValueError:
                pass
                
        # Write the header file
        with open(h_file_path, 'w') as f:
            f.write(f"// Generated Matrix animation frame {frame_num+1} of {total_frames}\n")
            f.write(f"// {WIDTH}x{HEIGHT} pixels, {INTENSITY_LEVELS} intensity levels\n\n")
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
                    f.write(f"{frame_data[y][x]}")
                    if x < WIDTH - 1:
                        f.write(", ")
                f.write("}")
                if y < HEIGHT - 1:
                    f.write(",")
                f.write("\n")
            
            f.write("};\n\n")
            f.write(f"#endif // MATRIX_FRAME_{frame_num}_H\n")
        
        return True
    except subprocess.SubprocessError:
        print(f"Warning: Could not process frame {frame_num+1} with ImageMagick")
        return False

def create_animation_header(num_frames):
    """Create the main animation header file"""
    h_file_path = os.path.join(OUTPUT_DIR, "matrix_animation.h")
    
    with open(h_file_path, 'w') as f:
        f.write("// Matrix animation data\n")
        f.write(f"// {num_frames} frames, {WIDTH}x{HEIGHT} pixels, {INTENSITY_LEVELS} intensity levels\n\n")
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
    """Main function to extract GIF frames and create header files"""
    start_time = time.time()
    
    # Check if input file exists
    if not os.path.exists(INPUT_FILE):
        print(f"Error: Input file '{INPUT_FILE}' not found.")
        return 1
    
    # Extract frames from the GIF
    frames = extract_gif_frames()
    
    if not frames:
        return 1
    
    # Report success
    elapsed = time.time() - start_time
    print(f"\nSuccess! Processed {len(frames)} frames in {elapsed:.2f} seconds")
    print(f"Frame rate: {FPS} FPS")
    print(f"Header files saved to {os.path.abspath(OUTPUT_DIR)}")
    return 0

if __name__ == "__main__":
    sys.exit(main())