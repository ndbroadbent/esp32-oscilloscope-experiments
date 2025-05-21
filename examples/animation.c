#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_system.h"
#include "esp_log.h"
#include "animation.h"  // Generated animation header from processed directory

#define DAC_CHAN_X DAC_CHAN_0  // GPIO25
#define DAC_CHAN_Y DAC_CHAN_1  // GPIO26
#define DAC_MAX_VALUE 255
#define X_SCALING_FACTOR 0.8  // Reduces X range to use more visible area (0.0-1.0)

// Drawing parameters
#define SCREEN_DIMS 128        // 128x128 pixel image resolution
#define MAX_INTENSITY 4        // Number of intensity levels (4 for better contrast)
#define OPTIMIZE_SIZE 1        // Optimize for minimal binary size

// Cache to avoid empty row scanning during draw, preventing beam pauses
DRAM_ATTR static bool row_has_content[NUM_FRAMES][SCREEN_DIMS][MAX_INTENSITY + 1] = {{{0}}};
DRAM_ATTR static bool cache_initialized = false;

// Task handle for the animation task
TaskHandle_t animation_task_handle = NULL;

// Simple idle task that periodically yields to keep FreeRTOS happy
void idle_task_animation(void *pvParameters) 
{
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// Store DAC values in DRAM for fast access 
DRAM_ATTR static uint8_t dac_x_values[SCREEN_DIMS] __attribute__((aligned(32)));
DRAM_ATTR static uint8_t dac_y_values[SCREEN_DIMS] __attribute__((aligned(32)));
DRAM_ATTR static bool tables_initialized = false;

// Initialize the empty row cache for all frames
void init_row_cache(void) {
    if (cache_initialized) return;
    
    printf("Initializing empty row cache...\n");
    
    // Scan each frame, row, and intensity level once at startup
    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        for (int pass = MAX_INTENSITY; pass > 0; pass--) {
            for (int y = 0; y < SCREEN_DIMS; y++) {
                const uint8_t* row = animation_frames[frame][y];
                
                // Check if this row has any pixels at this intensity level
                bool has_content = false;
                for (int x = 0; x < SCREEN_DIMS; x++) {
                    uint8_t intensity = row[x] > 0 ? ((row[x] * MAX_INTENSITY) / (FRAME_INTENSITY_LEVELS - 1)) : 0;
                    if (intensity >= pass) {
                        has_content = true;
                        break;
                    }
                }
                
                // Store in cache
                row_has_content[frame][y][pass] = has_content;
            }
        }
    }
    
    cache_initialized = true;
    printf("Row cache initialized, preventing beam pauses.\n");
}

// Clean, stable function to draw the current animation frame
void IRAM_ATTR draw_frame(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, int frame_num) {
    // Multi-pass rendering for proper intensity using cached row info
    for (int8_t pass = MAX_INTENSITY; pass > 0; pass--) {
        for (int16_t y = 0; y < SCREEN_DIMS; y++) {
            // Check cached value instead of scanning the row each time
            if (!row_has_content[frame_num][y][pass]) continue;
            
            // Get row data directly from animation frame
            const uint8_t* row = animation_frames[frame_num][y];
            
            // Set Y position once per row
            dac_oneshot_output_voltage(dac_y, dac_y_values[y]);
            
            // Scan the row
            for (int16_t x = 0; x < SCREEN_DIMS; x++) {
                // Only output for lit pixels at this intensity
                uint8_t intensity = row[x] > 0 ? ((row[x] * MAX_INTENSITY) / (FRAME_INTENSITY_LEVELS - 1)) : 0;
                
                if (intensity >= pass) {
                    dac_oneshot_output_voltage(dac_x, dac_x_values[x]);
                }
            }
        }
    }
}

// Configure high-speed DAC settings
DRAM_ATTR static dac_oneshot_config_t dac_config_x = {
    .chan_id = DAC_CHAN_X,
};

DRAM_ATTR static dac_oneshot_config_t dac_config_y = {
    .chan_id = DAC_CHAN_Y,
};

// Main animation task
void animation_task(void *pvParameters)
{
    // Initialize DAC channels
    dac_oneshot_handle_t dac_handle_x = NULL;
    dac_oneshot_handle_t dac_handle_y = NULL;
    
    // Configure DACs for maximum performance
    dac_oneshot_new_channel(&dac_config_x, &dac_handle_x);
    dac_oneshot_new_channel(&dac_config_y, &dac_handle_y);
    
    // Initialize DAC value tables up front (no conditionals in animation loop)
    // Precompute all DAC values for full contrast
    for (int i = 0; i < SCREEN_DIMS; i++) {
        uint8_t value = (i * DAC_MAX_VALUE) / (SCREEN_DIMS - 1);
        dac_x_values[i] = value;
        dac_y_values[i] = value;
    }
    tables_initialized = true;

    printf("Starting animation on oscilloscope...\n");
    
    // Set draws per frame based on desired FPS
    int draws_per_frame = 3; // Multiple draws for better visibility
    
    // Calculate timing to maintain consistent 10 FPS
    const int target_fps = 10;
    const TickType_t frame_delay = (1000 / target_fps) / portTICK_PERIOD_MS;
    
    printf("Animation running at %d FPS (target: 10 FPS)\n", ANIMATION_FPS);
    
    // Initialize row content cache - CRITICAL for preventing beam pauses
    init_row_cache();
    
    // Disable debug logging for performance
    esp_log_level_set("*", ESP_LOG_ERROR);
    
    // Start with the first frame
    int current_frame = 0;
    
    // Simple animation loop that maintains 10 FPS purely through repeated frame drawing
    // No delays, no waits, just continuous beam motion
    TickType_t next_frame_time = xTaskGetTickCount();
    
    while (1) {
        // Get current time
        TickType_t now = xTaskGetTickCount();
        
        // Check if it's time for the next frame
        if (now >= next_frame_time) {
            // Move to next frame
            current_frame = (current_frame + 1) % NUM_FRAMES;
            
            // Calculate next frame time
            next_frame_time = now + frame_delay;
        }
        
        // Always draw the current frame - NEVER STOP THE BEAM
        draw_frame(dac_handle_x, dac_handle_y, current_frame);
    }
}

// Function to stop the animation
void stop_animation(void) {
    if (animation_task_handle != NULL) {
        vTaskDelete(animation_task_handle);
        animation_task_handle = NULL;
    }
}

// Main entry point for the animation demo
void app_main_animation(void)
{
    printf("Starting animation demo on oscilloscope...\n");
    
    // Create idle task on Core 1
    TaskHandle_t idle_handle = NULL;
    xTaskCreatePinnedToCore(
        idle_task_animation,
        "idle_animation",
        1024,
        NULL,
        1,
        &idle_handle,
        1  // Core 1
    );
    
    // Create animation task on Core 0 with high priority
    xTaskCreatePinnedToCore(
        animation_task,
        "animation_task",
        8192,  // Larger stack for animation handling
        NULL,
        configMAX_PRIORITIES - 1,  // Max priority
        &animation_task_handle,
        0  // Core 0
    );
    
    // Wait for key press to exit
    printf("\nPress any key to return to menu...\n");
    char c;
    scanf("%c", &c);
    
    // Clean up
    if (idle_handle != NULL) {
        vTaskDelete(idle_handle);
    }
    stop_animation();
}
