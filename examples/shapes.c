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
#define SHAPE_CHANGE_TIME_MS 750  // Time to display each shape in ms
#define POINTS_PER_SHAPE 200       // Drawing resolution
#define PI 3.14159265358979323846

// Task handle for the drawing task
TaskHandle_t shapes_task_handle = NULL;

// A simple task that periodically yields to keep FreeRTOS happy
void idle_task_shapes(void *pvParameters) 
{
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// Function to draw a square pattern
void draw_square(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, int iterations) {
    for (int j = 0; j < iterations; j++) {
        // Left edge: bottom to top (0,0) to (0,size-1)
        for (int i = 0; i < DAC_MAX_VALUE; i++) {
            dac_oneshot_output_voltage(dac_x, 0);
            dac_oneshot_output_voltage(dac_y, i);
        }
        
        // Top edge: left to right (0,size-1) to (size-1,size-1)
        for (int i = 0; i < DAC_MAX_VALUE; i++) {
            dac_oneshot_output_voltage(dac_x, i);
            dac_oneshot_output_voltage(dac_y, DAC_MAX_VALUE-1);
        }
        
        // Right edge: top to bottom (size-1,size-1) to (size-1,0)
        for (int i = DAC_MAX_VALUE-1; i >= 0; i--) {
            dac_oneshot_output_voltage(dac_x, DAC_MAX_VALUE-1);
            dac_oneshot_output_voltage(dac_y, i);
        }
        
        // Bottom edge: right to left (size-1,0) to (0,0)
        // Don't include the last point (0,0) to avoid the bright spot at the corner
        for (int i = DAC_MAX_VALUE-1; i > 0; i--) {
            dac_oneshot_output_voltage(dac_x, i);
            dac_oneshot_output_voltage(dac_y, 0);
        }
    }
}

// Function to draw a proper equilateral triangle pattern pointing downward
void draw_triangle(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, int iterations) {
    // Define the three points of the triangle
    // Center the triangle in the display
    uint8_t margin = 10;
    uint8_t center_x = DAC_MAX_VALUE / 2;
    uint8_t size = (DAC_MAX_VALUE - 2 * margin) / 2;
    
    // Calculate the vertices of an equilateral triangle pointing DOWN
    // Bottom vertex (point)
    uint8_t x1 = center_x;
    uint8_t y1 = DAC_MAX_VALUE - margin;
    
    // Top left vertex
    uint8_t x2 = center_x - size;
    uint8_t y2 = margin;
    
    // Top right vertex
    uint8_t x3 = center_x + size;
    uint8_t y3 = margin;
    
    for (int j = 0; j < iterations; j++) {
        // First edge: Top left to bottom point (x2,y2) to (x1,y1)
        for (int i = 0; i < POINTS_PER_SHAPE/3; i++) {
            float progress = (float)i / (POINTS_PER_SHAPE/3);
            uint8_t x = x2 + (x1 - x2) * progress;
            uint8_t y = y2 + (y1 - y2) * progress;
            dac_oneshot_output_voltage(dac_x, x);
            dac_oneshot_output_voltage(dac_y, y);
        }
        
        // Second edge: Bottom point to top right (x1,y1) to (x3,y3)
        for (int i = 0; i < POINTS_PER_SHAPE/3; i++) {
            float progress = (float)i / (POINTS_PER_SHAPE/3);
            uint8_t x = x1 + (x3 - x1) * progress;
            uint8_t y = y1 + (y3 - y1) * progress;
            dac_oneshot_output_voltage(dac_x, x);
            dac_oneshot_output_voltage(dac_y, y);
        }
        
        // Third edge: Top right to top left (x3,y3) to (x2,y2)
        for (int i = 0; i < POINTS_PER_SHAPE/3; i++) {
            float progress = (float)i / (POINTS_PER_SHAPE/3);
            uint8_t x = x3 + (x2 - x3) * progress;
            uint8_t y = y3 + (y2 - y3) * progress;
            dac_oneshot_output_voltage(dac_x, x);
            dac_oneshot_output_voltage(dac_y, y);
        }
    }
}

// Function to draw a circle pattern
void draw_circle(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, int iterations) {
    uint8_t mid = DAC_MAX_VALUE / 2;
    uint8_t radius = DAC_MAX_VALUE / 2 - 10;  // Smaller to fit in DAC range
    
    for (int j = 0; j < iterations; j++) {
        for (int i = 0; i < POINTS_PER_SHAPE; i++) {
            float angle = (float)i / POINTS_PER_SHAPE * 2 * PI;
            uint8_t x = mid + radius * cosf(angle);
            uint8_t y = mid + radius * sinf(angle);
            dac_oneshot_output_voltage(dac_x, x);
            dac_oneshot_output_voltage(dac_y, y);
        }
    }
}

// Function to draw an X pattern
void draw_x(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, int iterations) {
    uint8_t margin = 10;
    
    for (int j = 0; j < iterations; j++) {
        // First diagonal (top-left to bottom-right)
        for (int i = 0; i < POINTS_PER_SHAPE/2; i++) {
            float progress = (float)i / (POINTS_PER_SHAPE/2);
            uint8_t x = margin + progress * (DAC_MAX_VALUE - 2 * margin);
            uint8_t y = margin + progress * (DAC_MAX_VALUE - 2 * margin);
            dac_oneshot_output_voltage(dac_x, x);
            dac_oneshot_output_voltage(dac_y, y);
        }
        
        // Second diagonal (bottom-left to top-right)
        for (int i = 0; i < POINTS_PER_SHAPE/2; i++) {
            float progress = (float)i / (POINTS_PER_SHAPE/2);
            uint8_t x = margin + progress * (DAC_MAX_VALUE - 2 * margin);
            uint8_t y = DAC_MAX_VALUE - margin - progress * (DAC_MAX_VALUE - 2 * margin);
            dac_oneshot_output_voltage(dac_x, x);
            dac_oneshot_output_voltage(dac_y, y);
        }
    }
}

// Draw different shapes in sequence
void draw_shapes_task(void *pvParameters)
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
    
    printf("Starting shape sequence on oscilloscope...\n");
    
    // Get the current time
    TickType_t last_change = xTaskGetTickCount();
    int current_shape = 0;  // 0 = square, 1 = triangle, 2 = circle, 3 = X
    
    // Main drawing loop - runs forever with shape changes
    while (1) {
        // Check if it's time to change shapes
        TickType_t now = xTaskGetTickCount();
        if ((now - last_change) * portTICK_PERIOD_MS >= SHAPE_CHANGE_TIME_MS) {
            current_shape = (current_shape + 1) % 4;  // Cycle through shapes
            last_change = now;
            
            switch (current_shape) {
                case 0: printf("Drawing square\n"); break;
                case 1: printf("Drawing triangle\n"); break;
                case 2: printf("Drawing circle\n"); break;
                case 3: printf("Drawing X\n"); break;
            }
        }
        
        // Draw the current shape
        switch (current_shape) {
            case 0:
                draw_square(dac_handle_x, dac_handle_y, 50);
                break;
            case 1:
                draw_triangle(dac_handle_x, dac_handle_y, 50);
                break;
            case 2:
                draw_circle(dac_handle_x, dac_handle_y, 50);
                break;
            case 3:
                draw_x(dac_handle_x, dac_handle_y, 50);
                break;
        }
        
        // Brief yield occasionally to reset watchdog
        taskYIELD();
    }
}

// Function to stop the shapes demo
void stop_shapes_demo(void) {
    if (shapes_task_handle != NULL) {
        vTaskDelete(shapes_task_handle);
        shapes_task_handle = NULL;
    }
}

// Main entry point for the shapes demo
void app_main_shapes(void)
{
    printf("Starting oscilloscope pattern display sequence...\n");
    
    // Create a simple idle task on Core 1 to keep system happy
    TaskHandle_t idle_handle = NULL;
    xTaskCreatePinnedToCore(
        idle_task_shapes,
        "idle_task_shapes",
        2048,
        NULL,
        1,
        &idle_handle,
        1  // Core 1
    );
    
    // Create our drawing task on Core 0
    xTaskCreatePinnedToCore(
        draw_shapes_task,
        "draw_shapes_task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,  // Maximum priority
        &shapes_task_handle,
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
    stop_shapes_demo();
}
