#!/usr/bin/env python3
"""
Extract a limited number of frames from a GIF to fit within partition size.
This script extracts frames from a GIF and creates header files for the ESP32 oscilloscope.
"""

import os
import subprocess
import tempfile
import time
import shutil
import sys
import argparse

# Default configuration
DEFAULT_INPUT_FILE = "/Users/ndbroadbent/code/esp32-scope/pictures/animation.gif"
DEFAULT_OUTPUT_DIR = "/Users/ndbroadbent/code/esp32-scope/processed"
DEFAULT_INTENSITY_LEVELS = 5
DEFAULT_FPS = 10
DEFAULT_WIDTH = 128
DEFAULT_HEIGHT = 128
DEFAULT_MAX_FRAMES = 45  # 4.5 seconds at 10 FPS

def ensure_directory(directory):
    """Ensure directory exists"""
    if not os.path.exists(directory):
        os.makedirs(directory)

def extract_limited_frames(input_file, output_dir, width, height, fps, intensity_levels, max_frames, prefix=None):
    """Extract a limited number of frames from GIF"""
    print(f"Processing {input_file}...")
    
    # Ensure output directory exists
    ensure_directory(output_dir)
    
    # Create temporary directory for extracted frames
    with tempfile.TemporaryDirectory() as temp_dir:
        # Use ffmpeg to extract frames
        try:
            print(f"Extracting up to {max_frames} frames with ffmpeg...")
            cmd = [
                'ffmpeg', '-i', input_file, 
                '-vf', f'scale={width}:{height},fps={fps}', 
                '-frames:v', str(max_frames),
                f'{temp_dir}/frame_%03d.png'
            ]
            subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except (subprocess.SubprocessError, FileNotFoundError):
            print("Error: Cannot extract GIF frames. Please install ffmpeg.")
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
            try:
                # Convert to grayscale and get pixel data with ImageMagick
                h_file_path = os.path.join(output_dir, f"frame_{i}.h")
                
                cmd = [
                    'convert', frame_file, '-depth', '8', '-colorspace', 'gray', 
                    '-resize', f'{width}x{height}!', '-rotate', '180', 'txt:-'
                ]
                result = subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                
                # Parse the output (format: x,y: (r,g,b) #RRGGBB)
                lines = result.stdout.decode('utf-8').split('\n')
                
                # Initialize frame data as all zeros
                frame_data = [[0 for _ in range(width)] for _ in range(height)]
                
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
                        color_part = parts[1].split(' ')[0].strip('()')
                        if ',' in color_part:
                            values = [int(v) for v in color_part.split(',')]
                            if len(values) >= 3:
                                r, g, b = values[0], values[1], values[2]
                                # Calculate brightness from RGB
                                brightness = (r + g + b) / 3
                                intensity = min(int(brightness * (intensity_levels - 1) / 255), intensity_levels - 1)
                            else:
                                intensity = int(int(values[0]) * (intensity_levels - 1) / 255)
                        else:
                            # Single grayscale value
                            gray = int(color_part)
                            intensity = int(gray * (intensity_levels - 1) / 255)
                        
                        if 0 <= x < width and 0 <= y < height:
                            frame_data[y][x] = intensity
                    except ValueError:
                        pass
                
                # Write the header file
                with open(h_file_path, 'w') as f:
                    f.write(f"// Generated animation frame {i+1} of {frame_count}\n")
                    f.write(f"// {width}x{height} pixels, {intensity_levels} intensity levels\n\n")
                    f.write(f"#ifndef FRAME_{i}_H\n")
                    f.write(f"#define FRAME_{i}_H\n\n")
                    f.write("#include <stdint.h>\n\n")
                    f.write(f"#define FRAME_WIDTH {width}\n")
                    f.write(f"#define FRAME_HEIGHT {height}\n")
                    f.write(f"#define FRAME_INTENSITY_LEVELS {intensity_levels}\n\n")
                    
                    f.write(f"// Frame {i+1} data - intensity values (0 = black, {intensity_levels-1} = bright)\n")
                    f.write(f"const uint8_t frame_{i}[FRAME_HEIGHT][FRAME_WIDTH] = {{\n")
                    
                    for y in range(height):
                        f.write("    {")
                        for x in range(width):
                            f.write(f"{frame_data[y][x]}")
                            if x < width - 1:
                                f.write(", ")
                        f.write("}")
                        if y < height - 1:
                            f.write(",")
                        f.write("\n")
                    
                    f.write("};\n\n")
                    f.write(f"#endif // FRAME_{i}_H\n")
                
                print(f"Processed frame {i+1}/{frame_count}")
            except Exception as e:
                print(f"Error processing frame {i+1}: {e}")
            
        # Create animation.h file
        create_animation_header(frame_count, output_dir, width, height, fps, intensity_levels, prefix)
        
        return frame_files

def create_animation_header(num_frames, output_dir, width, height, fps, intensity_levels, prefix=None):
    """Create the main animation header file"""
    h_file_path = os.path.join(output_dir, "animation.h")
    
    with open(h_file_path, 'w') as f:
        f.write("// Animation data\n")
        f.write(f"// {num_frames} frames, {width}x{height} pixels, {intensity_levels} intensity levels\n\n")
        f.write("#ifndef ANIMATION_H\n")
        f.write("#define ANIMATION_H\n\n")
        f.write("#include <stdint.h>\n\n")
        
        # Include all frame headers
        for i in range(num_frames):
            f.write(f"#include \"frame_{i}.h\"\n")
        
        f.write("\n#define NUM_FRAMES " + str(num_frames) + "\n")
        f.write(f"#define ANIMATION_FPS {fps}\n")
        f.write(f"#define FRAME_INTENSITY_LEVELS {intensity_levels}\n\n")
        
        # Create array of frame pointers for easy access
        f.write("const uint8_t* const animation_frames[NUM_FRAMES][FRAME_HEIGHT] = {\n")
        for i in range(num_frames):
            f.write(f"    // Frame {i+1}\n")
            f.write("    {\n")
            for y in range(height):
                f.write(f"        frame_{i}[{y}],\n")
            f.write("    },\n")
        f.write("};\n\n")
        
        f.write("#endif // ANIMATION_H\n")

def main():
    """Main function to extract limited frames and create header files"""
    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Extract frames from a GIF animation for ESP32 oscilloscope')
    parser.add_argument('--input', '-i', default=DEFAULT_INPUT_FILE, 
                        help=f'Input GIF file (default: {DEFAULT_INPUT_FILE})')
    parser.add_argument('--output', '-o', default=DEFAULT_OUTPUT_DIR, 
                        help=f'Output directory (default: {DEFAULT_OUTPUT_DIR})')
    parser.add_argument('--width', '-w', type=int, default=DEFAULT_WIDTH, 
                        help=f'Width of output frames (default: {DEFAULT_WIDTH})')
    parser.add_argument('--height', '-H', type=int, default=DEFAULT_HEIGHT, 
                        help=f'Height of output frames (default: {DEFAULT_HEIGHT})')
    parser.add_argument('--fps', '-f', type=int, default=DEFAULT_FPS, 
                        help=f'Frames per second (default: {DEFAULT_FPS})')
    parser.add_argument('--intensity', '-l', type=int, default=DEFAULT_INTENSITY_LEVELS, 
                        help=f'Number of intensity levels (default: {DEFAULT_INTENSITY_LEVELS})')
    parser.add_argument('--max-frames', '-m', type=int, default=DEFAULT_MAX_FRAMES, 
                        help=f'Maximum number of frames to extract (default: {DEFAULT_MAX_FRAMES})')
    parser.add_argument('--prefix', '-p', default='anim', 
                        help='Prefix for frame variable names (default: anim)')
    parser.add_argument('--clean', '-c', action='store_true',
                        help='Clean previous animation files before processing')
    
    args = parser.parse_args()
    
    start_time = time.time()
    
    # Check if input file exists
    if not os.path.exists(args.input):
        print(f"Error: Input file '{args.input}' not found.")
        return 1
    
    # Ensure output directory exists
    ensure_directory(args.output)
    
    # Clean output directory if requested
    if args.clean:
        print(f"Cleaning animation files in {args.output}...")
        for file in os.listdir(args.output):
            if file.startswith("frame_") or file == "animation.h":
                file_path = os.path.join(args.output, file)
                if os.path.isfile(file_path):
                    os.unlink(file_path)
                    print(f"Deleted {file}")
    
    # Extract frames from the GIF
    frames = extract_limited_frames(
        args.input, args.output, args.width, args.height, 
        args.fps, args.intensity, args.max_frames, args.prefix
    )
    
    if not frames:
        print("Error: Failed to extract frames")
        return 1
    
    # Report success
    elapsed = time.time() - start_time
    print(f"\nSuccess! Processed {len(frames)} frames in {elapsed:.2f} seconds")
    print(f"Frame rate: {args.fps} FPS")
    print(f"Header files saved to {os.path.abspath(args.output)}")
    print("\nUse these files in your ESP32 oscilloscope project:")
    print(f"  - {os.path.join(args.output, 'animation.h')}")
    print(f"  - frame_*.h files")
    return 0

if __name__ == "__main__":
    sys.exit(main())