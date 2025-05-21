# Oscilloscope Drawing Optimization Notes

## Beam Movement and Artifact Prevention

When drawing images on an oscilloscope (or other CRT-like displays), the electron beam can create visible artifacts even when passing through what should be "black" or unlit areas. This is a fundamental consideration when optimizing drawing routines.

### Key Insights

1. **Beam Movement Creates Traces**: The electron beam itself creates a visible trace as it moves through the display, even when it's supposed to be "off" or drawing black areas. This can create unwanted artifacts like visible lines in what should be blank areas.

2. **Minimize Unnecessary Movement**: To prevent artifacts, the beam should only move to areas where there's actual content to display. Avoid traversing black/empty regions.

3. **Vertical Line Artifacts**: These commonly appear when the beam is set to a specific X position and then moved vertically through Y positions that should be black/empty. Setting the Y DAC value for a row that has no visible content can create these artifacts.

### Effective Solutions

1. **Check Before Drawing**: Always verify a row has visible content before setting the Y DAC value for that row. This prevents the beam from moving through empty rows.

```c
// Check if row has content at current intensity level
bool row_has_content = false;
for (int16_t check_x = 0; check_x < SCREEN_DIMS; check_x++) {
    if (row[check_x] >= current_intensity) {
        row_has_content = true;
        break;
    }
}

// Only set Y DAC if row has content
if (row_has_content) {
    dac_oneshot_output_voltage(dac_y, dac_y_values[y]);
    // ... draw the row
}
```

2. **Segment-Based Drawing**: Draw connected segments of pixels rather than individual points. This reduces beam movement and creates more stable images.

3. **Reset Beam Position**: After drawing, reset the beam to a neutral position (usually 0,0) to avoid "parking burns" where the beam might stay in one position.

### Performance Considerations

- This approach is not only visually cleaner but also more efficient, as the beam moves less overall.
- Scanning each row to check for content adds a small overhead, but this is outweighed by the benefits of reduced beam movement and artifact elimination.
- For persistence displays (phosphor), minimizing beam movement helps maintain image stability and reduces flicker.

## Example Application: ESP32 Grayscale Drawing

These principles were successfully applied in an ESP32-based oscilloscope drawing system, where vertical line artifacts were eliminated by implementing row content checking before beam movement.

The resulting image was cleaner, with better delineation between the portrait subject and the black background, and no unwanted vertical lines in what should be empty areas of the image.