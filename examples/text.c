#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_system.h"
#include "hershey_fonts.h"  // Include standard Hershey font library

#define DAC_CHAN_X DAC_CHAN_0 // GPIO25
#define DAC_CHAN_Y DAC_CHAN_1 // GPIO26
#define DAC_MAX_VALUE 255
// #define POINTS_PER_STROKE 3   // Points for each stroke segment
#define CHAR_SPACING 0         // No additional space between characters
#define PI 3.14159265358979323846

// Orientation control - set these to 1 to invert axes if needed for your specific oscilloscope
#define INVERT_X 1  // Set to 1 to invert X axis
#define INVERT_Y 0  // Set to 1 to invert Y axis (fixes top/bottom orientation)

// Task handle for the drawing task
TaskHandle_t text_task_handle = NULL;

// Font drawing settings
#define FONT_SCALE 25          // Scale factor for the font
#define INTERP_POINTS 6        // Number of points to interpolate between vertices

// Set to reduce the number of points used in rendering
#define OPTIMIZE_POINTS 1  // Set to 1 to enable point reduction, 0 for original behavior

// Debug delay settings - uncomment to enable visualization
// #define DEBUG_DELAY
#define DEBUG_DELAY_CYCLES 25000  // Only used if DEBUG_DELAY is defined

// A simple task that periodically yields to keep FreeRTOS happy
void idle_task_text(void *pvParameters) 
{
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// Structure to pass DAC handles to the drawing callback
typedef struct {
    dac_oneshot_handle_t dac_x;
    dac_oneshot_handle_t dac_y;
    int last_x;
    int last_y;
    bool pen_was_down;
} dac_draw_ctx_t;

// DAC drawing callback function for the Hershey font renderer
void dac_draw_func(int x, int y, hershey_pen_t pen, void *ctx) {
    static int point_counter = 0;
    point_counter++;
    
    dac_draw_ctx_t *dac_ctx = (dac_draw_ctx_t *)ctx;
    
    // Print coordinate information only if debugging enabled
    #ifdef DEBUG_DELAY
    printf("Point #%d: (%d,%d) - Pen %s\n", 
           point_counter, x, y, 
           (pen == HERSHEY_PEN_UP) ? "UP" : "DOWN");
    
    if (dac_ctx->pen_was_down) {
        printf("  Delta from previous: dx=%d, dy=%d\n", 
               x - dac_ctx->last_x, y - dac_ctx->last_y);
    }
    #endif
    
    // Scale from Hershey coordinates to DAC range (0-255)
    int dac_x = x;
    int dac_y = y;
    
    // Ensure values are within DAC range (0-255)
    uint8_t x_dac = (dac_x > DAC_MAX_VALUE) ? DAC_MAX_VALUE : ((dac_x < 0) ? 0 : dac_x);
    uint8_t y_dac = (dac_y > DAC_MAX_VALUE) ? DAC_MAX_VALUE : ((dac_y < 0) ? 0 : dac_y);
    
    // Apply inversion if configured
    if (INVERT_X) {
        x_dac = DAC_MAX_VALUE - x_dac;
    }
    if (INVERT_Y) {
        y_dac = DAC_MAX_VALUE - y_dac;
    }
    
    // If pen is up, just move to the position without drawing
    if (pen == HERSHEY_PEN_UP) {
        #ifdef DEBUG_DELAY
        printf("  -> Moving to position with pen up\n");
        #endif
        dac_oneshot_output_voltage(dac_ctx->dac_x, x_dac);
        dac_oneshot_output_voltage(dac_ctx->dac_y, y_dac);
        // Add delay if debugging is enabled
        #ifdef DEBUG_DELAY
        for (volatile int d = 0; d < DEBUG_DELAY_CYCLES * 4; d++) { }  // Longer delay for pen-up
        #endif
        dac_ctx->pen_was_down = false;
        dac_ctx->last_x = dac_x;
        dac_ctx->last_y = dac_y;
        return;
    }
    
    // If pen is down and we were previously drawing,
    // let's use a simple, consistent approach for all lines
    if (dac_ctx->pen_was_down) {
        #ifdef DEBUG_DELAY
        printf("  -> Drawing line from previous point...\n");
        #endif
        
        // Calculate the number of points to use based on distance
        int dx = dac_x - dac_ctx->last_x;
        int dy = dac_y - dac_ctx->last_y;
        
        // Use Euclidean distance
        float distance = sqrt(dx*dx + dy*dy);
        
        // Adjust number of interpolation points based on optimization setting
        #if OPTIMIZE_POINTS
        int points = (int)(distance * 0.2); // Much fewer points (1 per 5 pixels)
        if (points > 8) points = 8;  // Hard cap at 8 points
        #else
        int points = (int)(distance * 0.8); // Roughly 1 point per pixel
        #endif
        
        if (points < 1) points = 1;
        
        #ifdef DEBUG_DELAY
        printf("  -> Line distance: %.2f, using %d points for interpolation\n", 
               distance, points);
        #endif
        
        // Draw the line with uniform interpolation
        for (int i = 0; i <= points; i++) {
            float t = (float)i / points;
            int interp_x = dac_ctx->last_x + t * dx;
            int interp_y = dac_ctx->last_y + t * dy;
            
            #ifdef DEBUG_DELAY
            printf("    > Interp point %d/%d: (%d,%d)\n", 
                   i, points, interp_x, interp_y);
            #endif
            
            // Ensure values are within DAC range
            uint8_t x_val = (interp_x > DAC_MAX_VALUE) ? DAC_MAX_VALUE : ((interp_x < 0) ? 0 : interp_x);
            uint8_t y_val = (interp_y > DAC_MAX_VALUE) ? DAC_MAX_VALUE : ((interp_y < 0) ? 0 : interp_y);
            
            // Apply inversion if configured
            if (INVERT_X) {
                x_val = DAC_MAX_VALUE - x_val;
            }
            if (INVERT_Y) {
                y_val = DAC_MAX_VALUE - y_val;
            }
            
            // Output to DAC with debug info if enabled
            #ifdef DEBUG_DELAY
            printf("      DAC output: X=%d, Y=%d\n", x_val, y_val);
            #endif
            dac_oneshot_output_voltage(dac_ctx->dac_x, x_val);
            dac_oneshot_output_voltage(dac_ctx->dac_y, y_val);
            
            // Add delay if debugging is enabled
            #ifdef DEBUG_DELAY
            for (volatile int d = 0; d < DEBUG_DELAY_CYCLES; d++) { }
            #endif
        }
    } else {
        // First point after pen up, just output it without interpolation
        #ifdef DEBUG_DELAY
        printf("  -> First point after pen up, direct output\n");
        #endif
        dac_oneshot_output_voltage(dac_ctx->dac_x, x_dac);
        dac_oneshot_output_voltage(dac_ctx->dac_y, y_dac);
        #ifdef DEBUG_DELAY
        printf("      DAC output: X=%d, Y=%d\n", x_dac, y_dac);
        #endif
    }
    
    // Update last position and pen state
    dac_ctx->last_x = dac_x;
    dac_ctx->last_y = dac_y;
    dac_ctx->pen_was_down = true;
}

// Draw text string using the Hershey font library
void draw_text(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, 
             const char* text, uint8_t x_pos, uint8_t y_pos, uint8_t size) {
    
    // Initialize the drawing context
    dac_draw_ctx_t ctx = {
        .dac_x = dac_x,
        .dac_y = dac_y,
        .last_x = 0,
        .last_y = 0,
        .pen_was_down = false
    };
    
    // Use the Hershey font library to draw the string
    hershey_draw_string(x_pos, y_pos, text, size, CHAR_SPACING, dac_draw_func, &ctx);
}

// Draw text with "WORLD" on top line and "HELLO" on bottom line
void draw_hello_world(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, int iterations) {
    uint8_t char_size = 25;   // Size of each character
    // uint8_t top_y = 0;       // Y position for top line
    // uint8_t bottom_y = 170;   // Y position for bottom line (not used)
    uint8_t start_x = 0;      // Starting X position (use full screen width)
    
    // Draw multiple times for brighter display
    for (int i = 0; i < iterations; i++) {
        draw_text(dac_x, dac_y, "03:", 0, 140, 36);
        draw_text(dac_x, dac_y, "09", 145, 140, 36);
        draw_text(dac_x, dac_y, "May 21", 10, 20, 24);
        
        // Yield occasionally to prevent watchdog timeouts
        if (i % 10 == 0) {
            taskYIELD();
        }
    }
}

// Draw text task
void draw_text_task(void *pvParameters)
{
    // Initialize DAC channels
    dac_oneshot_handle_t dac_handle_x = NULL;
    dac_oneshot_handle_t dac_handle_y = NULL;
    
    // DAC configuration for X channel
    dac_oneshot_config_t dac_config_x = {
        .chan_id = DAC_CHAN_X,
    };
    esp_err_t err_x = dac_oneshot_new_channel(&dac_config_x, &dac_handle_x);
    if (err_x != ESP_OK) {
        printf("Failed to initialize DAC X channel: %d\n", err_x);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } else {
        printf("DAC X channel initialized successfully\n");
    }
    
    // DAC configuration for Y channel
    dac_oneshot_config_t dac_config_y = {
        .chan_id = DAC_CHAN_Y,
    };
    esp_err_t err_y = dac_oneshot_new_channel(&dac_config_y, &dac_handle_y);
    if (err_y != ESP_OK) {
        printf("Failed to initialize DAC Y channel: %d\n", err_y);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } else {
        printf("DAC Y channel initialized successfully\n");
    }
    
    printf("Starting text display on oscilloscope...\n");
    
    // Initialize the Hershey font library
    esp_err_t font_init_res = hershey_fonts_init();
    if (font_init_res != ESP_OK) {
        printf("Failed to initialize Hershey font library: %d\n", font_init_res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } else {
        printf("Hershey font library initialized successfully\n");
    }
    
    // Test the DAC outputs with a simple pattern before using the font
    printf("Testing DAC output with simple pattern...\n");
    for (int i = 0; i < 20; i++) {
        // Generate a simple square pattern to test DACs
        for (int x = 0; x < 255; x += 10) {
            dac_oneshot_output_voltage(dac_handle_x, x);
            dac_oneshot_output_voltage(dac_handle_y, 128);
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
        
        for (int y = 0; y < 255; y += 10) {
            dac_oneshot_output_voltage(dac_handle_x, 128);
            dac_oneshot_output_voltage(dac_handle_y, y);
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }
    
    // Main drawing loop - runs forever
    printf("Starting main drawing loop...\n");
    while (1) {
        // Draw hello world text with moderate iterations for balanced visibility
        draw_hello_world(dac_handle_x, dac_handle_y, 3);
        
        // Brief yield occasionally to reset watchdog
        taskYIELD();
    }
}

// Function to stop the text demo
void stop_text_demo(void) {
    if (text_task_handle != NULL) {
        vTaskDelete(text_task_handle);
        text_task_handle = NULL;
    }
}

// Main entry point for the text demo
void app_main_text(void)
{
    printf("Starting oscilloscope text display...\n");
    
    // Create a simple idle task on Core 1 to keep system happy
    TaskHandle_t idle_handle = NULL;
    xTaskCreatePinnedToCore(
        idle_task_text,
        "idle_task_text",
        2048,
        NULL,
        1,
        &idle_handle,
        1  // Core 1
    );
    
    // Create our drawing task on Core 0
    xTaskCreatePinnedToCore(
        draw_text_task,
        "draw_text_task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,  // Maximum priority
        &text_task_handle,
        0  // Core 0
    );
    
    // Since we're running directly, we don't exit
    printf("\nText display is running...\n");
    
    // We don't exit this function, let the tasks run
    while(1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
