#ifndef HERSHEY_FONTS_H
#define HERSHEY_FONTS_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Hershey Simplex Font
 * 
 * This is an implementation that uses the standard Hershey simplex font
 * data from hershey-ascii.h
 * 
 * Original source: https://github.com/markfickett/arduinohershey
 */

// Debug mode - define to show bounding boxes around characters
// #define HERSHEY_DEBUG_BOXES

// Hershey font data is imported from hershey-ascii.h directly in the implementation file

// Stroke type - pen up or down
typedef enum {
    HERSHEY_PEN_UP = 0,
    HERSHEY_PEN_DOWN = 1
} hershey_pen_t;

/**
 * @brief Initialize the Hershey font system
 * 
 * @return ESP_OK on success
 */
esp_err_t hershey_fonts_init(void);

/**
 * @brief Draw a character using the Hershey Simplex font
 * 
 * @param x_pos X position to draw the character
 * @param y_pos Y position to draw the character
 * @param c Character to draw
 * @param scale Scale factor for the character
 * @param draw_func Function to call to draw each point (e.g., output to DAC)
 * @param ctx Context to pass to the draw function
 * @return Width of the character drawn
 */
int hershey_draw_char(int x_pos, int y_pos, char c, int scale, 
                    void (*draw_func)(int x, int y, hershey_pen_t pen, void *ctx), 
                    void *ctx);

/**
 * @brief Draw a string using the Hershey Simplex font
 * 
 * @param x_pos X position to start drawing the string
 * @param y_pos Y position to draw the string
 * @param str String to draw
 * @param scale Scale factor for the characters
 * @param char_spacing Spacing between characters (in addition to character width)
 * @param draw_func Function to call to draw each point (e.g., output to DAC)
 * @param ctx Context to pass to the draw function
 */
void hershey_draw_string(int x_pos, int y_pos, const char *str, int scale, int char_spacing, 
                       void (*draw_func)(int x, int y, hershey_pen_t pen, void *ctx),
                       void *ctx);

#endif /* HERSHEY_FONTS_H */
