#!/bin/bash
# Script to convert a GIF to an oscilloscope animation

# Default values
DEFAULT_INPUT_FILE=""
DEFAULT_FPS=10
DEFAULT_INTENSITY=5
DEFAULT_MAX_FRAMES=45

# Help text
usage() {
  echo "Usage: $0 -i <input.gif> [-o <output_dir>] [-f <fps>] [-l <intensity_levels>] [-m <max_frames>] [-p <prefix>] [-c]"
  echo
  echo "Options:"
  echo "  -i <input.gif>      Input GIF file (required)"
  echo "  -o <output_dir>     Output directory (default: ./processed)"
  echo "  -f <fps>            Frames per second (default: $DEFAULT_FPS)"
  echo "  -l <intensity>      Number of intensity levels (default: $DEFAULT_INTENSITY)"
  echo "  -m <max_frames>     Maximum number of frames (default: $DEFAULT_MAX_FRAMES)"
  echo "  -p <prefix>         Prefix for frame variable names (not used anymore)"
  echo "  -c                  Clean previous animation files before processing"
  echo "  -h                  Show this help message"
  exit 1
}

# Parse command line arguments
while getopts "i:o:f:l:m:p:ch" opt; do
  case ${opt} in
    i )
      INPUT_FILE=$OPTARG
      ;;
    o )
      OUTPUT_DIR=$OPTARG
      ;;
    f )
      FPS=$OPTARG
      ;;
    l )
      INTENSITY=$OPTARG
      ;;
    m )
      MAX_FRAMES=$OPTARG
      ;;
    p )
      PREFIX=$OPTARG
      ;;
    c )
      CLEAN="--clean"
      ;;
    h )
      usage
      ;;
    \? )
      usage
      ;;
  esac
done

# Check for required input file
if [ -z "$INPUT_FILE" ]; then
  echo "Error: Input GIF file is required"
  usage
fi

# Build the command
CMD="python3 /Users/ndbroadbent/code/esp32-scope/scripts/extract_limited_frames.py"
CMD="$CMD --input $INPUT_FILE"

if [ ! -z "$OUTPUT_DIR" ]; then
  CMD="$CMD --output $OUTPUT_DIR"
fi

if [ ! -z "$FPS" ]; then
  CMD="$CMD --fps $FPS"
fi

if [ ! -z "$INTENSITY" ]; then
  CMD="$CMD --intensity $INTENSITY"
fi

if [ ! -z "$MAX_FRAMES" ]; then
  CMD="$CMD --max-frames $MAX_FRAMES"
fi

if [ ! -z "$PREFIX" ]; then
  CMD="$CMD --prefix $PREFIX"
fi

if [ ! -z "$CLEAN" ]; then
  CMD="$CMD $CLEAN"
fi

# Execute the command
echo "Running: $CMD"
eval $CMD

# Instructions for using the animation
if [ $? -eq 0 ]; then
  echo
  echo "To use this animation in your ESP32 project:"
  echo "1. Make sure animation.h is in your include path"
  echo "2. Include animation.h in your project"
  echo "3. The animation will be available as:"
  echo "   - NUM_FRAMES: Total number of frames"
  echo "   - ANIMATION_FPS: Frames per second"
  echo "   - animation_frames[frame_num][y][x]: The animation data"
  echo
  echo "Build your project with: idf.py build"
fi

exit $?