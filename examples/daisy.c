#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_system.h"

#define DAC_CHAN_X DAC_CHAN_0 // GPIO25
#define DAC_CHAN_Y DAC_CHAN_1 // GPIO26
#define DAC_MAX_VALUE 255
#define POINTS_PER_PETAL 50  // More points for smoother curves
#define POINTS_CENTER 40
#define CENTER_RADIUS (DAC_MAX_VALUE / 8 * 0.75)  // 75% of the original size
#define PI 3.14159265358979323846
#define ROTATION_SPEED 0.003  // Radians per frame - very slow rotation

// Task handle for the drawing task
TaskHandle_t daisy_task_handle = NULL;

// A simple task that periodically yields to keep FreeRTOS happy
void idle_task_daisy(void *pvParameters) 
{
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// Helper function to check if a point is inside the center circle
bool is_inside_center(float x, float y, float cx, float cy) {
    float dx = x - cx;
    float dy = y - cy;
    float distance_squared = dx * dx + dy * dy;
    
    // Add a tiny buffer to make sure we don't draw inside the circle
    float radius_with_buffer = CENTER_RADIUS + 0.5;
    return distance_squared <= (radius_with_buffer * radius_with_buffer);
}

// No separate functions needed as we've integrated everything into draw_daisy

// Main daisy drawing function with rotation
void draw_daisy(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, float rotation) {
    // Maintain one continuous beam trace for the whole flower
    
    // First draw the center circle
    float cx = DAC_MAX_VALUE / 2;
    float cy = DAC_MAX_VALUE / 2;
    
    // Draw the center circle - with the rotation applied to keep consistent with petals
    for (int i = 0; i < POINTS_CENTER; i++) {
        float angle = (float)i / POINTS_CENTER * 2 * PI + rotation;
        float x_float = cx + CENTER_RADIUS * cosf(angle);
        float y_float = cy + CENTER_RADIUS * sinf(angle);
        
        // Clamp and convert
        uint8_t x = (x_float > DAC_MAX_VALUE) ? DAC_MAX_VALUE : (x_float < 0) ? 0 : (uint8_t)x_float;
        uint8_t y = (y_float > DAC_MAX_VALUE) ? DAC_MAX_VALUE : (y_float < 0) ? 0 : (uint8_t)y_float;
        
        dac_oneshot_output_voltage(dac_x, x);
        dac_oneshot_output_voltage(dac_y, y);
    }
    
    // Define petal parameters
    uint8_t num_petals = 12; // More petals for a fuller daisy
    float petal_size = DAC_MAX_VALUE / 2 - 15; // Make petals a good length
    float petal_width = petal_size * 0.2;  // Narrower petals to accommodate more of them
    
    // Draw each petal with rotation
    for (int petal = 0; petal < num_petals; petal++) {
        float angle = petal * (2 * PI / num_petals) + rotation; // Add rotation angle
        
        // First move to starting point of petal with rotation applied
        // Using the angle of this petal to find the point on the circle
        uint8_t prev_x = cx + CENTER_RADIUS * cosf(angle);
        uint8_t prev_y = cy + CENTER_RADIUS * sinf(angle);
        
        // First side of the petal - evenly distribute points
        uint8_t last_x = 0, last_y = 0;
        bool have_last_point = false;
        
        for (int i = 0; i < POINTS_PER_PETAL; i++) {
            float t = (float)i / POINTS_PER_PETAL;
            
            // Use parametric ellipse equation for smooth curve
            float normalized_t = t * PI;
            // Modify the curve shape near the tip to avoid lingering
            float r = petal_size * 0.5 * (1.0 - cosf(normalized_t));
            // Adjust the width factor to be more even
            float width_factor = 0.6 * sinf(normalized_t);
            
            // Main curve
            float x_offset = r * cosf(angle);
            float y_offset = r * sinf(angle);
            
            // Add width to one side
            float perpendicular_angle = angle + PI/2;
            float width_x = petal_width * width_factor * cosf(perpendicular_angle);
            float width_y = petal_width * width_factor * sinf(perpendicular_angle);
            
            float x_float = cx + x_offset + width_x;
            float y_float = cy + y_offset + width_y;
            
            // Skip if inside center
            if (is_inside_center(x_float, y_float, cx, cy)) {
                continue;
            }
            
            // Clamp and output 
            uint8_t x = (x_float > DAC_MAX_VALUE) ? DAC_MAX_VALUE : (x_float < 0) ? 0 : (uint8_t)x_float;
            uint8_t y = (y_float > DAC_MAX_VALUE) ? DAC_MAX_VALUE : (y_float < 0) ? 0 : (uint8_t)y_float;
            
            // Skip if this point is the same as the last one (prevents bright spots)
            if (have_last_point && x == last_x && y == last_y) {
                continue;
            }
            
            dac_oneshot_output_voltage(dac_x, x);
            dac_oneshot_output_voltage(dac_y, y);
            
            // Save this valid point
            prev_x = x;
            prev_y = y;
            last_x = x;
            last_y = y;
            have_last_point = true;
        }
        
        // Create a more even transition at the tip by adding a slight pause
        dac_oneshot_output_voltage(dac_x, prev_x);
        dac_oneshot_output_voltage(dac_y, prev_y);
        
        // Second side of the petal (from tip back to base)
        last_x = prev_x;
        last_y = prev_y;
        have_last_point = true;
        
        for (int i = POINTS_PER_PETAL - 1; i >= 0; i--) {
            float t = (float)i / POINTS_PER_PETAL;
            
            float normalized_t = t * PI;
            float r = petal_size * 0.5 * (1.0 - cosf(normalized_t));
            float width_factor = 0.6 * sinf(normalized_t);
            
            // Main curve
            float x_offset = r * cosf(angle);
            float y_offset = r * sinf(angle);
            
            // Add width to other side
            float perpendicular_angle = angle - PI/2;
            float width_x = petal_width * width_factor * cosf(perpendicular_angle);
            float width_y = petal_width * width_factor * sinf(perpendicular_angle);
            
            float x_float = cx + x_offset + width_x;
            float y_float = cy + y_offset + width_y;
            
            // Skip if inside center
            if (is_inside_center(x_float, y_float, cx, cy)) {
                continue;
            }
            
            // Clamp and output
            uint8_t x = (x_float > DAC_MAX_VALUE) ? DAC_MAX_VALUE : (x_float < 0) ? 0 : (uint8_t)x_float;
            uint8_t y = (y_float > DAC_MAX_VALUE) ? DAC_MAX_VALUE : (y_float < 0) ? 0 : (uint8_t)y_float;
            
            // Skip if this point is the same as the last one (prevents bright spots)
            if (have_last_point && x == last_x && y == last_y) {
                continue;
            }
            
            dac_oneshot_output_voltage(dac_x, x);
            dac_oneshot_output_voltage(dac_y, y);
            
            // Update last point
            last_x = x;
            last_y = y;
        }
        
        // Draw line back to circle for continuous trace
        dac_oneshot_output_voltage(dac_x, prev_x);
        dac_oneshot_output_voltage(dac_y, prev_y);
    }
}

// Draw the daisy on the oscilloscope
void draw_daisy_task(void *pvParameters)
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
    
    printf("Drawing rotating daisy flower on oscilloscope...\n");
    
    // Track rotation angle
    float rotation_angle = 0.0f;
    
    // Main drawing loop - continuously trace the outline as fast as possible
    while (1) {
        // Draw the daisy with current rotation angle
        draw_daisy(dac_handle_x, dac_handle_y, rotation_angle);
        
        // Increment rotation angle for next frame (slow rotation)
        rotation_angle += ROTATION_SPEED;
        if (rotation_angle >= 2 * PI) {
            rotation_angle -= 2 * PI;  // Keep angle within 0-2π range
        }
        
        // Just a minimal yield to keep the watchdog happy without visible interruption
        taskYIELD();
    }
}

// Function to stop the daisy demo
void stop_daisy_demo(void) {
    if (daisy_task_handle != NULL) {
        vTaskDelete(daisy_task_handle);
        daisy_task_handle = NULL;
    }
}

// Main entry point for the daisy demo
void app_main_daisy(void)
{
    printf("Starting daisy flower oscilloscope display...\n");
    
    // Create a simple idle task on Core 1 to keep system happy
    TaskHandle_t idle_handle = NULL;
    xTaskCreatePinnedToCore(
        idle_task_daisy,
        "idle_task_daisy",
        2048,
        NULL,
        1,
        &idle_handle,
        1  // Core 1
    );
    
    // Create our drawing task on Core 0
    xTaskCreatePinnedToCore(
        draw_daisy_task,
        "draw_daisy_task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,  // Maximum priority
        &daisy_task_handle,
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
    stop_daisy_demo();
}
