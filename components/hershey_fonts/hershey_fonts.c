#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>  // For abs()
#include <math.h>    // For sqrt()
#include "esp_err.h"
#include "hershey_fonts.h"
#include "hershey-ascii.h"

// Pen spacing settings
#define POINTS_PER_STROKE 2     // Number of points to interpolate between vertices (reduced from 10)
#define SCALE_FACTOR 12        // Scaling factor for font rendering
#define DEFAULT_WIDTH 90        // Default character width if not specified

// Initialize the Hershey font system
esp_err_t hershey_fonts_init(void) {
    // Nothing to initialize
    return ESP_OK;
}

// Draw a character using the Hershey font data
int hershey_draw_char(int x_pos, int y_pos, char c, int scale, 
                    void (*draw_func)(int x, int y, hershey_pen_t pen, void *ctx), 
                    void *ctx) {
    
    // Check if character is in range (ASCII 32-126)
    if (c < 32 || c > 126) {
        // Use space for unsupported characters
        c = 32; // space
    }
    
    // Get index in the font array
    int index = c - 32;
    
    // First two values in the array are width info
    int left_pos = simplex[index][0];
    int right_pos = simplex[index][1];
    int width = right_pos - left_pos;
    
    // If character has zero width, use default
    if (width <= 0) {
        width = DEFAULT_WIDTH;
    }
    
    // Calculate the character width - use fixed width for all characters
    int fixed_width = (20 * scale) / SCALE_FACTOR;  // Use consistent width for all characters
    int scaled_width = fixed_width;  // Ignore actual character width from font data
    // Normal character drawing mode
    // Draw character vertices
    bool pen_down = false;
    int last_x = 0, last_y = 0;
    bool first_point = true;
    
    // Start from index 2, which is the first coordinate pair
    for (int i = 2; i < 112; i += 2) {
        // Get coordinate pair
        int x = simplex[index][i];
        int y = simplex[index][i+1];
        
        // Check for end of data
        if (x == -1 && y == -1) {
            // Pen up control point
            pen_down = false;
            continue;
        }
        
        // If both coordinates are -1 or we've run out of data, we're done
        if ((x == -1 && y == -1) || i >= 112 || (x == 0 && y == 0 && i > 2)) {
            break;
        }
        
        // Scale and translate the coordinates 
        // Don't adjust for left_pos here - the caller will position the character appropriately
        int scaled_x = x_pos + (x * scale) / SCALE_FACTOR;
        int scaled_y = y_pos + (y * scale) / SCALE_FACTOR;
        
            // No need for extra bounds tracking anymore
        
        // Draw the point or line
        if (first_point) {
            // First point, just move to it
            draw_func(scaled_x, scaled_y, HERSHEY_PEN_UP, ctx);
            first_point = false;
            
            // CRITICAL FIX: We need to set pen_down to true after the first point
            // so subsequent points get connected with lines
            pen_down = true;
        } else if (pen_down) {
            // Get the distance between points
            int dx = scaled_x - last_x;
            int dy = scaled_y - last_y;
            
            // Use Euclidean distance to determine number of points
            float distance = sqrt(dx*dx + dy*dy);
            
            // Use just enough interpolation points to ensure smooth lines
            // Adjusted coefficient to reduce total points
            int points = (int)(distance * 0.3); // Reduced from 1.5 to 0.3
            
            // Set minimum and maximum points
            if (points < POINTS_PER_STROKE) points = POINTS_PER_STROKE;
            if (points > 30) points = 30; // Cap maximum points per segment
            
            // Draw the line with interpolation
            for (int j = 0; j <= points; j++) {
                float t = (float)j / points;
                int interp_x = last_x + t * dx;
                int interp_y = last_y + t * dy;
                
                draw_func(interp_x, interp_y, HERSHEY_PEN_DOWN, ctx);
            }
        } else {
            // This is a new point after a pen-up command (-1,-1)
            draw_func(scaled_x, scaled_y, HERSHEY_PEN_UP, ctx);
            pen_down = true;  // Set pen down for next point
        }
        
        // Update last position
        last_x = scaled_x;
        last_y = scaled_y;
    }
    
#ifdef HERSHEY_DEBUG_BOXES
    // Now draw the bounding box around the character after we've drawn the character
    int char_height = (21 * scale) / SCALE_FACTOR;
    
    // Draw a rectangle around the character's box with proper line interpolation
    // Top left corner - start here
    draw_func(x_pos, y_pos, HERSHEY_PEN_UP, ctx);
    
    // Draw top edge - interpolate 10 points along this line
    for (int i = 1; i <= 10; i++) {
        int x = x_pos + (i * scaled_width) / 10;
        draw_func(x, y_pos, HERSHEY_PEN_DOWN, ctx);
    }
    
    // Draw right edge - interpolate 10 points along this line
    for (int i = 1; i <= 10; i++) {
        int y = y_pos + (i * char_height) / 10;
        draw_func(x_pos + scaled_width, y, HERSHEY_PEN_DOWN, ctx);
    }
    
    // Draw bottom edge - interpolate 10 points along this line
    for (int i = 1; i <= 10; i++) {
        int x = x_pos + scaled_width - (i * scaled_width) / 10;
        draw_func(x, y_pos + char_height, HERSHEY_PEN_DOWN, ctx);
    }
    
    // Draw left edge - interpolate 10 points along this line
    for (int i = 1; i <= 10; i++) {
        int y = y_pos + char_height - (i * char_height) / 10;
        draw_func(x_pos, y, HERSHEY_PEN_DOWN, ctx);
    }
#endif

    // Return the scaled width of the character
    return scaled_width;
}

// Function to draw just a vertical line (for problem characters)
void draw_vertical_stroke(int x_pos, int y_pos, int height, int scale, 
                         void (*draw_func)(int x, int y, hershey_pen_t pen, void *ctx), 
                         void *ctx) {
    // Draw vertical line with 5 parallel strokes for visibility
    for (int offset = -2; offset <= 2; offset++) {
        int x = x_pos + offset;
        
        // Move to start position
        draw_func(x, y_pos, HERSHEY_PEN_UP, ctx);
        
        // Draw densely packed points along the line
        int num_points = height * 3; // Very dense point spacing
        for (int j = 0; j <= num_points; j++) {
            int y = y_pos + (j * height) / num_points;
            draw_func(x, y, HERSHEY_PEN_DOWN, ctx);
        }
    }
}

// Helper for adding missing parts of characters that don't render well
void enhance_character(char c, int x_pos, int y_pos, int scale,
                     void (*draw_func)(int x, int y, hershey_pen_t pen, void *ctx),
                     void *ctx) {
    // Character-specific enhancement for problematic vertical strokes
    int height = (21 * scale) / SCALE_FACTOR;
    
    switch (c) {
        case 'I':
            // Draw a heavy vertical line for 'I'
            draw_vertical_stroke(x_pos + (scale/2), y_pos, height, scale, draw_func, ctx);
            break;
            
        case 'L':
            // Draw a heavy vertical line for 'L'
            draw_vertical_stroke(x_pos + (4 * scale) / SCALE_FACTOR, y_pos, height, scale, draw_func, ctx);
            break;
            
        case 'K':
            // Draw a heavy vertical line for the stem of 'K'
            draw_vertical_stroke(x_pos + (4 * scale) / SCALE_FACTOR, y_pos, height, scale, draw_func, ctx);
            break;
            
        case 'H':
            // Enhance both verticals of 'H'
            draw_vertical_stroke(x_pos + (4 * scale) / SCALE_FACTOR, y_pos, height, scale, draw_func, ctx);
            draw_vertical_stroke(x_pos + (15 * scale) / SCALE_FACTOR, y_pos, height, scale, draw_func, ctx);
            break;
            
        // Add other problematic characters here if needed
    }
}

// Draw a string using the Hershey font
void hershey_draw_string(int x_pos, int y_pos, const char *str, int scale, int char_spacing, 
                       void (*draw_func)(int x, int y, hershey_pen_t pen, void *ctx),
                       void *ctx) {
    int current_x = x_pos;
    int fixed_width = (20 * scale) / SCALE_FACTOR;  // Use consistent fixed width for all characters
    
    // Draw each character in the string
    for (int i = 0; i < strlen(str); i++) {
        char c = str[i];
        
        // Calculate the character offset to center it within its fixed-width box
        int char_offset = 0;
        
        if (c != ' ') {  // Don't center spaces
            // Get index in the font array
            int index = c - 32;
            
            // Get actual character width from font data
            int left_pos = simplex[index][0];
            int right_pos = simplex[index][1];
            int actual_width = right_pos - left_pos;
            
            // If width data looks valid, center the character in fixed space
            if (actual_width > 0) {
                        // Need to find the actual leftmost and rightmost points used in this character
                int min_x = 999, max_x = -999;
                
                // Scan through all points for this character to find actual bounds
                for (int j = 2; j < 112; j += 2) {
                    int px = simplex[index][j];
                    
                    // Skip end markers
                    if (px == -1) continue;
                    if (j >= 112 || (px == 0 && simplex[index][j+1] == 0 && j > 2)) break;
                    
                    if (px < min_x) min_x = px;
                    if (px > max_x) max_x = px;
                }
                
                // Now we know the actual width in font units
                int actual_width_font = max_x - min_x;
                int actual_scaled_width = (actual_width_font * scale) / SCALE_FACTOR;
                
                // Calculate the leftmost point's position when rendered normally 
                int leftmost_point_pos = (min_x * scale) / SCALE_FACTOR;
                
                // Calculate how much to offset to center the character
                char_offset = (fixed_width - actual_scaled_width) / 2 - leftmost_point_pos;
            }
        }
        
        // Draw the character at the current position with proper centering
        hershey_draw_char(current_x + char_offset, y_pos, c, scale, draw_func, ctx);
        
        // Move to the next position with fixed spacing for monospace appearance
        current_x += fixed_width + char_spacing;
    }
}
