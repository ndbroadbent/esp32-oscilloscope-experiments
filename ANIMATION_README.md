# ESP32 Oscilloscope Animation System

This document explains how to use the generic animation system for the ESP32 oscilloscope.

## Overview

The animation system allows you to display animated GIFs on the oscilloscope. The system consists of:

1. Python scripts to extract frames from GIFs and convert them to C header files
2. A generic animation.c file that displays the animation frames
3. A framework for easily swapping in different animations

## Creating a New Animation

### Step 1: Prepare your GIF

- Use a GIF with dimensions close to 128x128 pixels
- Keep the file size smaller for better performance
- Simple animations with good contrast work best

### Step 2: Convert the GIF to animation header files

Use the `create_animation.sh` script:

```bash
./scripts/create_animation.sh -i your_animation.gif -p your_prefix
```

Options:
- `-i <input.gif>`: Input GIF file (required)
- `-o <output_dir>`: Output directory (default: ./processed)
- `-f <fps>`: Frames per second (default: 10)
- `-l <intensity>`: Number of intensity levels (default: 5)
- `-m <max_frames>`: Maximum number of frames (default: 45)
- `-p <prefix>`: Prefix for frame variable names (default: anim)
- `-c`: Clean previous animation files before processing

This script will:
1. Extract frames from the GIF using ffmpeg
2. Convert each frame to a C header file with grayscale intensity values
3. Create a main `animation.h` file that includes all frames

### Step 3: Configure the ESP32 project

1. In the ESP-IDF configuration menu (`idf.py menuconfig`), select the "Animation Demo" under the "Select Demo to Run" option

2. Build and flash your project:
```bash
idf.py build
idf.py -p PORT flash
```

## How It Works

The animation system:

1. Extracts frames from a GIF and converts them to 128x128 grayscale images
2. Quantizes each pixel to a specified number of intensity levels (usually 3-5)
3. Stores the frames in C header files as arrays
4. Uses optimized beam control to draw each frame on the oscilloscope
5. Manages frame timing to achieve the desired FPS

## Performance Considerations

- More frames = larger binary size. The ESP32 has limited memory, so keep animations short (max ~45 frames)
- Higher frame rates require more processing power. 10 FPS works well for most animations
- Fewer intensity levels (3-5) result in better performance and less flicker
- The beam control system prevents artifacts by careful control of the oscilloscope beam

## Troubleshooting

If your animation doesn't display correctly:
- Ensure your GIF has good contrast
- Try reducing the intensity levels (`-l` option)
- Reduce the frame rate (`-f` option)
- Use a smaller GIF with fewer frames

## Examples

Convert a Matrix digital rain animation:
```bash
./scripts/create_animation.sh -i pictures/matrix.gif -p matrix -f 10 -l 3 -m 30
```

Convert a simple animated logo:
```bash
./scripts/create_animation.sh -i pictures/logo.gif -p logo -f 5 -l 4 -m 20
```