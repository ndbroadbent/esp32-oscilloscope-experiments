#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_system.h"
#include "esp_log.h"
#include "masha2_image.h"  // Include our generated image header

#define DAC_CHAN_X DAC_CHAN_0 // GPIO25
#define DAC_CHAN_Y DAC_CHAN_1 // GPIO26
#define DAC_MAX_VALUE 255

// Drawing parameters
#define SCREEN_DIMS 128     // pixel image resolution
#define MAX_INTENSITY 3     // Number of intensity levels (3 for better performance)
#define NUM_SHADES 3        // Number of shades to use
#define OPTIMIZE_DRAWING 1  // Enable optimizations
#define DRAW_IMAGE 1        // Draw an image instead of the test pattern
#define USE_DRAM_BUFFER 1   // Use DRAM for frame buffer instead of IRAM

// Task handle for the drawing task
TaskHandle_t grayscale_task_handle = NULL;

// A simple task that periodically yields to keep FreeRTOS happy
void idle_task_grayscale(void *pvParameters) 
{
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// We'll use a frame buffer to store intensity values - placed in DRAM for speed
DRAM_ATTR static uint8_t frame_buffer[SCREEN_DIMS][SCREEN_DIMS] __attribute__((aligned(32))) = {0};

// Function to map coordinates from virtual grid to DAC values
inline uint8_t map_to_dac(uint8_t val) {
    // Use full DAC range for better pixel separation
    return (val * DAC_MAX_VALUE) / (SCREEN_DIMS - 1);
}

// Function to set a pixel intensity in the frame buffer - force inline for speed
inline __attribute__((always_inline)) void set_pixel(uint16_t x, uint16_t y, uint8_t intensity) {
    if (x < SCREEN_DIMS && y < SCREEN_DIMS) {
        frame_buffer[y][x] = intensity;
    }
}

// Function to fill a square in the frame buffer
void fill_square(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t intensity) {
    for (uint16_t y = y1; y <= y2 && y < SCREEN_DIMS; y++) {
        for (uint16_t x = x1; x <= x2 && x < SCREEN_DIMS; x++) {
            set_pixel(x, y, intensity);
        }
    }
}

// Function to draw a circle with specified intensity
void draw_circle(int center_x, int center_y, int radius, uint8_t intensity) {
    // Use the midpoint circle algorithm
    for (int y = 0; y < SCREEN_DIMS; y++) {
        for (int x = 0; x < SCREEN_DIMS; x++) {
            int dx = x - center_x;
            int dy = y - center_y;
            int distance_squared = dx * dx + dy * dy;
            
            // Check if the point is within the circle
            if (distance_squared <= radius * radius) {
                set_pixel(x, y, intensity);
            }
        }
    }
}

// High-performance function to draw the entire frame using horizontal scan lines
// Function placed in IRAM for maximum performance
// Store DAC values in DRAM for fast access 
DRAM_ATTR static uint8_t dac_x_values[SCREEN_DIMS] __attribute__((aligned(32)));
DRAM_ATTR static uint8_t dac_y_values[SCREEN_DIMS] __attribute__((aligned(32)));
DRAM_ATTR static bool tables_initialized = false;

// Define alternate DAC values to reduce vertical artifacts
// Avoid certain DAC values that might cause issues
DRAM_ATTR static uint8_t stabilized_dac_values[SCREEN_DIMS] __attribute__((aligned(32)));

// High-performance function to draw the entire frame using horizontal scan lines
// Function placed in IRAM for maximum performance
void IRAM_ATTR draw_frame(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y) {
    // One-time initialization of DAC value tables
    if (!tables_initialized) {
        // Precompute all DAC values for full contrast
        for (int i = 0; i < SCREEN_DIMS; i++) {
            uint8_t value = (i * DAC_MAX_VALUE) / (SCREEN_DIMS - 1);
            
            // No special adjustments needed - the image data is accurate
            
            dac_x_values[i] = value;
            dac_y_values[i] = value;
        }
        tables_initialized = true;
    }
    
    // Draw using a more robust scanning approach that carefully controls beam movement
    // First draw brightest levels, then progressively dimmer ones
    for (int8_t pass = MAX_INTENSITY; pass > 0; pass--) {
        // For each row
        for (int16_t y = 0; y < SCREEN_DIMS; y++) {
            // Get direct pointer to row data for faster access
            const uint8_t* row = frame_buffer[y];
            
            // Fast scan to find if this row has any content at this intensity level
            bool row_has_content = false;
            for (int16_t check_x = 0; check_x < SCREEN_DIMS; check_x++) {
                if (row[check_x] >= pass) {
                    row_has_content = true;
                    break;
                }
            }
            
            // Skip entire row if it has no pixels at this intensity
            if (!row_has_content) continue;
            
            // Only set the Y DAC value once we're sure we'll draw something
            // This prevents vertical line artifacts from beam movement
            dac_oneshot_output_voltage(dac_y, dac_y_values[y]);
            
            // Scan across the row
            int16_t x = 0;
            while (x < SCREEN_DIMS) {
                // Skip all black pixels (zeros) efficiently
                while (x < SCREEN_DIMS && row[x] < pass) {
                    x++;
                }
                
                // If we've reached the end of the row, exit
                if (x >= SCREEN_DIMS) {
                    break;
                }
                
                // We found an actual segment of non-black pixels to draw
                int16_t x_start = x;
                
                // Scan to find the end of this segment of lit pixels
                while (x < SCREEN_DIMS && row[x] >= pass) {
                    x++;
                }
                
                // Calculate segment length and prepare to draw
                int16_t segment_length = x - x_start;
                
                // Only proceed if segment has meaningful length
                // This prevents single-pixel drawing which can cause artifacts
                if (segment_length <= 0) continue;
                
                // Draw all pixels in this segment without moving the beam unnecessarily
                int16_t draw_x = x_start;
                
                // Use unrolled loop for longer segments (4 pixels at a time)
                while (segment_length >= 4) {
                    dac_oneshot_output_voltage(dac_x, dac_x_values[draw_x++]);
                    dac_oneshot_output_voltage(dac_x, dac_x_values[draw_x++]);
                    dac_oneshot_output_voltage(dac_x, dac_x_values[draw_x++]);
                    dac_oneshot_output_voltage(dac_x, dac_x_values[draw_x++]);
                    segment_length -= 4;
                }
                
                // Handle any remaining pixels one by one
                while (segment_length > 0) {
                    dac_oneshot_output_voltage(dac_x, dac_x_values[draw_x++]);
                    segment_length--;
                }
            }
        }
    }
    
    // Reset beam at end
    dac_oneshot_output_voltage(dac_x, 0);
    dac_oneshot_output_voltage(dac_y, 0);
}

// Function to load the image data into the frame buffer with scaling and optimization
void setup_grayscale_image() {
    // Clear frame buffer
    memset(frame_buffer, 0, sizeof(frame_buffer));
    
    // Calculate scaling factors if image dimensions don't match buffer dimensions
    float scale_x = (float)IMAGE_WIDTH / SCREEN_DIMS;
    float scale_y = (float)IMAGE_HEIGHT / SCREEN_DIMS;
    
    // Copy image data with scaling and intensity adjustment
    // Optimize for 128x128 buffer with anti-aliasing effect
    for (int y = 0; y < SCREEN_DIMS; y++) {
        // Map to image coordinates with vertical flip
        int img_y = (IMAGE_HEIGHT - 1) - (int)(y * scale_y);
        
        // Ensure bounds
        if (img_y < 0) img_y = 0;
        if (img_y >= IMAGE_HEIGHT) img_y = IMAGE_HEIGHT - 1;
        
        for (int x = 0; x < SCREEN_DIMS; x++) {
            // Map to image coordinates 
            int img_x = (int)(x * scale_x);
            
            // Ensure bounds
            if (img_x < 0) img_x = 0;
            if (img_x >= IMAGE_WIDTH) img_x = IMAGE_WIDTH - 1;
            
            // Get intensity value from image data (0-4)
            uint8_t image_intensity = grayscale_image[img_y][img_x];
            
            // Scale to our intensity range (0-MAX_INTENSITY)
            // Adding 0.5 for rounding
            uint8_t intensity = (image_intensity * MAX_INTENSITY + 0.5) / (IMAGE_INTENSITY_LEVELS - 1);
            
            // Set pixel in frame buffer - direct access for speed
            frame_buffer[y][x] = intensity;
        }
    }
    
    printf("Loaded and scaled image to %dx%d in frame buffer\n", SCREEN_DIMS, SCREEN_DIMS);
}

// Function to create a test pattern of different shades
void setup_grayscale_test() {
    // Clear frame buffer
    memset(frame_buffer, 0, sizeof(frame_buffer));
    
    // Calculate uniform 2x2 grid with no internal margins
    const uint8_t grid_size = 2; // 2x2 grid
    const uint8_t square_size = SCREEN_DIMS / grid_size; // Exact division for uniform grid
    
    // Create intensity values for the 4 squares (in order: top-left, top-right, bottom-left, bottom-right)
    uint8_t intensities[4] = {
        MAX_INTENSITY,         // Brightest (top-left)
        MAX_INTENSITY * 3/4,   // Medium-bright (top-right)
        MAX_INTENSITY * 2/4,   // Medium (bottom-left)
        MAX_INTENSITY * 1/4    // Dim (bottom-right)
    };
    
    // Draw the 2x2 grid with 4 different intensities
    for (uint8_t row = 0; row < 2; row++) {
        for (uint8_t col = 0; col < 2; col++) {
            // Calculate the square's coordinates
            uint8_t x1 = col * square_size;
            uint8_t y1 = row * square_size;
            uint8_t x2 = x1 + square_size - 1;
            uint8_t y2 = y1 + square_size - 1;
            
            // Calculate index (0-3) for the intensity array
            uint8_t idx = row * 2 + col;
            
            // Fill the square with this intensity
            fill_square(x1, y1, x2, y2, intensities[idx]);
        }
    }
}

// Configure high-speed DAC settings (must be declared at module level for speed)
DRAM_ATTR static dac_oneshot_config_t dac_config_x = {
    .chan_id = DAC_CHAN_X,
};

DRAM_ATTR static dac_oneshot_config_t dac_config_y = {
    .chan_id = DAC_CHAN_Y,
};

// Main grayscale drawing task placed in IRAM for high speed execution
void IRAM_ATTR grayscale_task(void *pvParameters)
{
    // Initialize DAC channels with max performance settings
    dac_oneshot_handle_t dac_handle_x = NULL;
    dac_oneshot_handle_t dac_handle_y = NULL;
    
    // Configure DACs for maximum performance
    dac_oneshot_new_channel(&dac_config_x, &dac_handle_x);
    dac_oneshot_new_channel(&dac_config_y, &dac_handle_y);
    
    printf("Drawing grayscale image on oscilloscope using optimized routine...\n");
    
    // Load the image data into the frame buffer
    // This is a one-time operation at startup
    printf("Loading image data...\n");
    setup_grayscale_image();
    
    // Disable all debug message logging to improve performance
    esp_log_level_set("*", ESP_LOG_ERROR);
    
    // Preload memory caches before main loop
    // Draw one test frame to ensure everything is loaded
    draw_frame(dac_handle_x, dac_handle_y);
    
    // Main drawing loop - runs continuously at maximum speed
    // This is the critical section for performance
    while (1) {
        // Draw frame as fast as possible with no delays
        draw_frame(dac_handle_x, dac_handle_y);
        
        // Minimal cooperative yield for watchdog but return immediately
        __asm__ volatile ("nop"); // Prevent compiler optimization from removing the yield
        taskYIELD();
    }
}

// Function to stop the grayscale demo
void stop_grayscale_demo(void) {
    if (grayscale_task_handle != NULL) {
        vTaskDelete(grayscale_task_handle);
        grayscale_task_handle = NULL;
    }
}

// Main entry point for the grayscale demo - optimized for performance
void app_main_grayscale(void)
{
    printf("Starting grayscale demo on oscilloscope (optimized build)...\n");
    
    // Core 1 is reserved only for maintenance/watchdog tasks
    // Prevent any unnecessary tasks from running on Core 1
    TaskHandle_t idle_handle = NULL;
    xTaskCreatePinnedToCore(
        idle_task_grayscale,
        "idle_grayscale",
        1024,                       // Minimum stack size
        NULL,
        1,                          // Low priority
        &idle_handle,
        1                           // Run on Core 1
    );
    
    // Core 0 is dedicated to drawing - create task with highest priority
    // and large stack to avoid any performance issues
    xTaskCreatePinnedToCore(
        grayscale_task,
        "grayscale_task",
        16384,                      // Extra large stack to avoid any overhead
        NULL,
        configMAX_PRIORITIES - 1,   // Maximum priority
        &grayscale_task_handle,
        0                           // Run on Core 0
    );
    
    // Wait for key press to exit
    printf("\nPress any key to return to menu...\n");
    char c;
    scanf("%c", &c);
    
    // Clean up
    if (idle_handle != NULL) {
        vTaskDelete(idle_handle);
    }
    stop_grayscale_demo();
}
