# ESP32 Oscilloscope Vector Graphics

This project uses an ESP32's DAC pins to display vector graphics on an analog oscilloscope in X/Y mode. The DAC outputs from the ESP32 are used to control the X and Y deflections of the oscilloscope beam, creating various vector patterns.

## Hardware Setup

- ESP32 WROOM-32 development board
- Analog oscilloscope with X/Y mode
- Connections:
  - ESP32 GPIO25 (DAC_CHAN_0) → X input on oscilloscope 
  - ESP32 GPIO26 (DAC_CHAN_1) → Y input on oscilloscope
  - ESP32 GND → Oscilloscope GND
- It's recommended to use a small resistor (e.g. 1kΩ) in series with each connection for protection

## Building and Flashing

This project uses the ESP-IDF framework. To build and flash:

```
idf.py build
idf.py -p [PORT] flash
idf.py -p [PORT] monitor  # To view the menu
```

## Available Demos

The project includes multiple demos:

1. **Shapes Demo** - Displays a sequence of geometric shapes (square, triangle, circle, X)
2. **Text Demo** - Displays "HELLO" on the top line and "WORLD" on the bottom line

## Usage

You can select which demo to run at compile time using the ESP-IDF's configuration system:

```
# Configure project and select which demo to run
idf.py menuconfig
# Navigate to: ESP32 Oscilloscope Demo Configuration → Select Demo to Run
# Choose either "Shapes Demo" or "Text Demo"
```

Alternatively, you can set the configuration directly from the command line:

```bash
# For the shapes demo
idf.py -D DEMO_MODE=DEMO_SHAPES build

# For the text demo
idf.py -D DEMO_MODE=DEMO_TEXT build
```

Then flash the ESP32:

```
idf.py flash monitor
```

## Technical Details

- Uses ESP32 DAC pins providing 8-bit resolution (0-255) for both X and Y
- DAC values range from 0-255, giving a 256×256 resolution
- Shapes are drawn using connected line segments
- Text is rendered using vector strokes

## Adding New Demos

To add a new demo:

1. Create a new file in the `examples` directory
2. Implement the demo using the DAC output functions
3. Add an entry point function with the naming convention `app_main_yourfeature()`
4. Update `main.c` to include your new demo in the menu
5. Update `main/CMakeLists.txt` to include your new source file

## Notes

- The oscilloscope should be set to X/Y mode (sometimes called "vector" mode)
- Adjust the brightness and focus of your oscilloscope for the best display
- If your display appears upside-down or reversed:
  1. Edit `examples/text.c` 
  2. Find the `INVERT_X` and `INVERT_Y` definitions (around line 17)
  3. Change them from 0 to 1 as needed to correct the orientation
  4. Rebuild and flash the firmware