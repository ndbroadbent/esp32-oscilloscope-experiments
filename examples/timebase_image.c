#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_system.h"
#include "esp_rom_sys.h"    // For esp_rom_delay_us
#include "masha2_image.h"  // Include our generated image header

#define DAC_CHAN_X DAC_CHAN_0 // GPIO25
#define DAC_CHAN_Y DAC_CHAN_1 // GPIO26
#define DAC_MAX_VALUE 255

// Drawing parameters
#define POINTS_PER_LINE 100         // How many points to output per line
#define Y_MIN 50                    // Minimum Y value (black)
#define Y_MAX 200                   // Maximum Y value (white)
#define LINE_SYNC_DELAY_US 10       // Small delay between lines

// Task handle for the drawing task
TaskHandle_t timebase_task_handle = NULL;

// A simple task that periodically yields to keep FreeRTOS happy
void idle_task_timebase(void *pvParameters) 
{
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// Function to map intensity to Y position
uint8_t map_intensity_to_y(uint8_t intensity, uint8_t max_intensity) {
    // Map intensity from 0-max to Y_MIN-Y_MAX
    float y_pos = Y_MIN + ((float)intensity / max_intensity) * (Y_MAX - Y_MIN);
    
    // Ensure within valid range
    if (y_pos < 0) y_pos = 0;
    if (y_pos > DAC_MAX_VALUE) y_pos = DAC_MAX_VALUE;
    
    return (uint8_t)y_pos;
}

// Function to output a single line of the image to match oscilloscope's horizontal sweep
void draw_scan_line(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, uint16_t line_index) {
    // Make sure we're in valid range
    if (line_index >= IMAGE_HEIGHT) return;
    
    // Flip the line index to show image right-side up
    uint16_t flipped_line = IMAGE_HEIGHT - 1 - line_index;
    
    // Set X to 0 to start the sweep - this should trigger the oscilloscope
    dac_oneshot_output_voltage(dac_x, 0);
    
    // Now output the Y values as the oscilloscope sweeps X
    // The oscilloscope's timebase will control the horizontal sweep
    // We just need to provide Y values at the right rate
    for (int i = 0; i < POINTS_PER_LINE; i++) {
        // Calculate which pixel from the image to display
        int img_x = i * IMAGE_WIDTH / POINTS_PER_LINE;
        
        if (img_x < IMAGE_WIDTH) {
            // Get intensity from the image (0 to IMAGE_INTENSITY_LEVELS-1)
            uint8_t intensity = grayscale_image[flipped_line][img_x];
            
            // Map intensity to Y position (higher intensity = higher position)
            uint8_t y_pos = map_intensity_to_y(intensity, IMAGE_INTENSITY_LEVELS - 1);
            
            // Output Y position
            dac_oneshot_output_voltage(dac_y, y_pos);
        } else {
            // Beyond image width, keep Y at minimum
            dac_oneshot_output_voltage(dac_y, Y_MIN);
        }
        
        // Let the oscilloscope control X sweep - no delay needed
        // But we do need to increment X for ESP32's output
        // Set X to match rate of oscilloscope sweep
        uint8_t x_pos = (i * DAC_MAX_VALUE) / POINTS_PER_LINE;
        dac_oneshot_output_voltage(dac_x, x_pos);
    }
    
    // Small delay between lines to let oscilloscope trigger again
    esp_rom_delay_us(LINE_SYNC_DELAY_US);
}

// Main timebase scan task
void timebase_task(void *pvParameters)
{
    // Initialize DAC channels
    dac_oneshot_handle_t dac_handle_x = NULL;
    dac_oneshot_handle_t dac_handle_y = NULL;
    
    dac_oneshot_config_t dac_config = {
        .chan_id = DAC_CHAN_X,
    };
    dac_oneshot_new_channel(&dac_config, &dac_handle_x);
    
    dac_config.chan_id = DAC_CHAN_Y;
    dac_oneshot_new_channel(&dac_config, &dac_handle_y);
    
    printf("Starting oscilloscope scan image display...\n");
    printf("Set your oscilloscope to:\n");
    printf("  - X timebase: Try different settings - start with around 10-20 us/div\n");
    printf("  - Trigger: Auto mode on Channel X (or EXT-X)\n");
    printf("  - Display: Adjust brightness/persistence as needed\n");
    
    // Limit how many lines we draw at once to maintain responsiveness
    const int LINES_PER_BATCH = 32;
    
    // Main drawing loop - continuously scan the image
    while (1) {
        // Draw the image in small batches of lines to keep system responsive
        for (int start_line = 0; start_line < IMAGE_HEIGHT; start_line += LINES_PER_BATCH) {
            int end_line = start_line + LINES_PER_BATCH;
            if (end_line > IMAGE_HEIGHT) end_line = IMAGE_HEIGHT;
            
            // Draw this batch of lines
            for (int line = start_line; line < end_line; line++) {
                draw_scan_line(dac_handle_x, dac_handle_y, line);
            }
            
            // Brief yield after each batch to keep system responsive
            taskYIELD();
        }
    }
}

// Function to stop the timebase demo
void stop_timebase_demo(void) {
    if (timebase_task_handle != NULL) {
        vTaskDelete(timebase_task_handle);
        timebase_task_handle = NULL;
    }
}

// Main entry point for the timebase demo
void app_main_timebase(void)
{
    printf("Starting timebase image scan demo on oscilloscope...\n");
    
    // Create a simple idle task on Core 1 to keep system happy
    TaskHandle_t idle_handle = NULL;
    xTaskCreatePinnedToCore(
        idle_task_timebase,
        "idle_task_timebase",
        2048,
        NULL,
        1,
        &idle_handle,
        1  // Core 1
    );
    
    // Create our drawing task on Core 0
    xTaskCreatePinnedToCore(
        timebase_task,
        "timebase_task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,  // Maximum priority
        &timebase_task_handle,
        0  // Core 0
    );
    
    // Wait for key press to exit
    printf("\nPress any key to return to menu...\n");
    char c;
    scanf("%c", &c);
    
    // Clean up tasks
    if (idle_handle != NULL) {
        vTaskDelete(idle_handle);
    }
    stop_timebase_demo();
}