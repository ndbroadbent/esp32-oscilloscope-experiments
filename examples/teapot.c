#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_system.h"

// DAC settings
#define DAC_CHAN_X DAC_CHAN_0 // GPIO25
#define DAC_CHAN_Y DAC_CHAN_1 // GPIO26
#define DAC_MAX_VALUE 255

// Oscilloscope orientation control
#define INVERT_X 1  // Set to 1 to invert X axis
#define INVERT_Y 0  // Set to 0 for normal orientation

// Task handle for the drawing task
TaskHandle_t teapot_task_handle = NULL;

// 3D rendering settings
#define SCREEN_SCALE 100.0f  // Scale the 3D model to fit screen
#define SCREEN_CENTER_X 128  // Center of X axis
#define SCREEN_CENTER_Y 128  // Center of Y axis

// Rotation control
#define ROTATION_SPEED_X 0.008f  // Radians per frame
#define ROTATION_SPEED_Y 0.012f  // Radians per frame
#define ROTATION_SPEED_Z 0.004f  // Radians per frame

// Animation control
#define ANIMATION_DELAY_MS 0   // Time between frames in milliseconds

// Structure to pass DAC handles to the drawing callback
typedef struct {
    dac_oneshot_handle_t dac_x;
    dac_oneshot_handle_t dac_y;
} dac_draw_ctx_t;

// Structure for 3D point
typedef struct {
    float x, y, z;
} Point3D;

// Structure for 2D projected point
typedef struct {
    int x, y;
} Point2D;

// Structure for 3D line (edge)
typedef struct {
    int start_idx;
    int end_idx;
} Line3D;

// A simple task that periodically yields to keep FreeRTOS happy
void idle_task_teapot(void *pvParameters) 
{
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// Utah teapot vertices data
// This is a simplified version of the Utah teapot with fewer vertices
#include "teapot_data.h"

// Draw a line between two points
void draw_line(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, 
               int x1, int y1, int x2, int y2) {
    // Calculate the distance between points
    int dx = x2 - x1;
    int dy = y2 - y1;
    
    // Calculate number of steps based on the longer distance
    int steps = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
    
    // Ensure we have at least one step
    if (steps < 1) steps = 1;
    
    // For very long lines, limit the steps to avoid spending too much time
    if (steps > 30) steps = 30;
    
    // Calculate increment for each step
    float x_inc = (float)dx / steps;
    float y_inc = (float)dy / steps;
    
    // Draw the line with interpolation
    float x = x1, y = y1;
    
    // Much shorter delay for faster drawing
    int delay_val = (steps > 15) ? 0 : 5;
    
    // Don't draw the very first point - this avoids a bright spot at line start
    x += x_inc;
    y += y_inc;
    
    // Draw remaining points
    for (int i = 1; i <= steps; i++) {
        // Ensure values are within DAC range
        uint8_t x_dac = (x > DAC_MAX_VALUE) ? DAC_MAX_VALUE : ((x < 0) ? 0 : (uint8_t)x);
        uint8_t y_dac = (y > DAC_MAX_VALUE) ? DAC_MAX_VALUE : ((y < 0) ? 0 : (uint8_t)y);
        
        // Apply inversion if configured
        if (INVERT_X) {
            x_dac = DAC_MAX_VALUE - x_dac;
        }
        if (INVERT_Y) {
            y_dac = DAC_MAX_VALUE - y_dac;
        }
        
        // Output to DAC
        dac_oneshot_output_voltage(dac_x, x_dac);
        dac_oneshot_output_voltage(dac_y, y_dac);
        
        // Small delay for oscilloscope visibility - adaptive based on line length
        // Shorter delay means we spend less time on each point, creating more even brightness
        for (volatile int d = 0; d < delay_val; d++) { }
        
        // Move to next point
        x += x_inc;
        y += y_inc;
    }
    
    // Don't stay on the endpoint too long
    // This helps avoid bright spots at line endpoints
}

// Rotate a 3D point around the X axis
void rotate_x(Point3D *point, float angle) {
    float y = point->y;
    float z = point->z;
    point->y = y * cosf(angle) - z * sinf(angle);
    point->z = y * sinf(angle) + z * cosf(angle);
}

// Rotate a 3D point around the Y axis
void rotate_y(Point3D *point, float angle) {
    float x = point->x;
    float z = point->z;
    point->x = x * cosf(angle) + z * sinf(angle);
    point->z = -x * sinf(angle) + z * cosf(angle);
}

// Rotate a 3D point around the Z axis
void rotate_z(Point3D *point, float angle) {
    float x = point->x;
    float y = point->y;
    point->x = x * cosf(angle) - y * sinf(angle);
    point->y = x * sinf(angle) + y * cosf(angle);
}

// Project a 3D point to 2D screen coordinates
Point2D project(Point3D point) {
    Point2D result;
    
    // Use perspective projection with a fixed viewing distance
    const float viewing_distance = 3.0f;
    float z_factor = viewing_distance / (viewing_distance + point.z);
    
    // Project and scale to screen coordinates
    result.x = SCREEN_CENTER_X + (int)(point.x * SCREEN_SCALE * z_factor);
    result.y = SCREEN_CENTER_Y + (int)(point.y * SCREEN_SCALE * z_factor);
    
    return result;
}

// Draw the teapot wireframe
void draw_teapot(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, 
                float angle_x, float angle_y, float angle_z) {
    // Create a copy of vertices that we can modify
    Point3D rotated_vertices[NUM_VERTICES];
    for (int i = 0; i < NUM_VERTICES; i++) {
        rotated_vertices[i].x = teapot_vertices[i][0];
        rotated_vertices[i].y = teapot_vertices[i][1];
        rotated_vertices[i].z = teapot_vertices[i][2];
        
        // Apply rotations
        rotate_x(&rotated_vertices[i], angle_x);
        rotate_y(&rotated_vertices[i], angle_y);
        rotate_z(&rotated_vertices[i], angle_z);
    }
    
    // Project all points to 2D
    Point2D projected_points[NUM_VERTICES];
    for (int i = 0; i < NUM_VERTICES; i++) {
        projected_points[i] = project(rotated_vertices[i]);
    }
    
    // Draw all the edges
    for (int i = 0; i < NUM_EDGES; i++) {
        int start_idx = teapot_edges[i][0];
        int end_idx = teapot_edges[i][1];
        
        // Draw line between projected points
        draw_line(dac_x, dac_y,
                 projected_points[start_idx].x, projected_points[start_idx].y,
                 projected_points[end_idx].x, projected_points[end_idx].y);
    }
}

// Animation loop task
void teapot_animation_task(void *pvParameters) {
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
    
    printf("Starting teapot animation on oscilloscope...\n");
    
    // Test the DAC outputs with a simple pattern before showing teapot
    printf("Testing DAC output with simple pattern...\n");
    for (int i = 0; i < 10; i++) {
        // Draw a square
        for (int t = 0; t < 255; t += 5) {
            // Top edge
            dac_oneshot_output_voltage(dac_handle_x, t);
            dac_oneshot_output_voltage(dac_handle_y, 50);
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
        for (int t = 50; t < 200; t += 5) {
            // Right edge
            dac_oneshot_output_voltage(dac_handle_x, 255);
            dac_oneshot_output_voltage(dac_handle_y, t);
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
        for (int t = 255; t > 0; t -= 5) {
            // Bottom edge
            dac_oneshot_output_voltage(dac_handle_x, t);
            dac_oneshot_output_voltage(dac_handle_y, 200);
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
        for (int t = 200; t > 50; t -= 5) {
            // Left edge
            dac_oneshot_output_voltage(dac_handle_x, 0);
            dac_oneshot_output_voltage(dac_handle_y, t);
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }
    
    // Animation variables
    float angle_x = 0.0f;
    float angle_y = 0.0f;
    float angle_z = 0.0f;
    
    // Main animation loop - runs forever
    printf("Starting main animation loop...\n");
    while (1) {
        // Draw the teapot wireframe - we avoid returning the beam to center at the end
        // by drawing edges in a more continuous path and not having large jumps
        // We'll use our own simplified rendering approach rather than the edges[] array
        
        // Create a copy of vertices that we can modify for this frame
        Point3D rotated_vertices[NUM_VERTICES];
        for (int i = 0; i < NUM_VERTICES; i++) {
            rotated_vertices[i].x = teapot_vertices[i][0];
            rotated_vertices[i].y = teapot_vertices[i][1];
            rotated_vertices[i].z = teapot_vertices[i][2];
            
            // Apply rotations
            rotate_x(&rotated_vertices[i], angle_x);
            rotate_y(&rotated_vertices[i], angle_y);
            rotate_z(&rotated_vertices[i], angle_z);
        }
        
        // Project all points to 2D
        Point2D projected_points[NUM_VERTICES];
        for (int i = 0; i < NUM_VERTICES; i++) {
            projected_points[i] = project(rotated_vertices[i]);
        }
        
        // Draw all the edges, but we'll use a different strategy to avoid returning to center
        // Instead of drawing edges in the order they're defined, we'll try to draw connected paths 
        // where the end of one edge connects to the start of the next
        
        // Start drawing from the top rings since they're the most recognizable part of the teapot
        // Top ring - this is the rim of the teapot body 
        for (int i = 37; i <= 44; i++) {
            int next_i = (i < 44) ? i + 1 : 37;
            draw_line(dac_handle_x, dac_handle_y,
                     projected_points[i].x, projected_points[i].y,
                     projected_points[next_i].x, projected_points[next_i].y);
        }
        
        // Lid lower ring
        for (int i = 46; i <= 53; i++) {
            int next_i = (i < 53) ? i + 1 : 46;
            draw_line(dac_handle_x, dac_handle_y,
                     projected_points[i].x, projected_points[i].y,
                     projected_points[next_i].x, projected_points[next_i].y);
        }
        
        // Lid upper ring
        for (int i = 54; i <= 61; i++) {
            int next_i = (i < 61) ? i + 1 : 54;
            draw_line(dac_handle_x, dac_handle_y,
                     projected_points[i].x, projected_points[i].y,
                     projected_points[next_i].x, projected_points[next_i].y);
        }
        
        // Connect lid rings to lid top
        for (int i = 0; i < 8; i++) {
            int lu = 54 + i;  // Lid upper ring
            draw_line(dac_handle_x, dac_handle_y,
                    projected_points[lu].x, projected_points[lu].y,
                    projected_points[62].x, projected_points[62].y);
        }
        
        // Connect top ring to lid
        for (int i = 0; i < 8; i++) {
            int t = 37 + i;  // Top ring
            int l = 46 + i;  // Lid lower ring
            draw_line(dac_handle_x, dac_handle_y,
                    projected_points[t].x, projected_points[t].y,
                    projected_points[l].x, projected_points[l].y);
        }
        
        // Connect lid rings
        for (int i = 0; i < 8; i++) {
            int ll = 46 + i;  // Lid lower ring
            int lu = 54 + i;  // Lid upper ring
            draw_line(dac_handle_x, dac_handle_y,
                    projected_points[ll].x, projected_points[ll].y,
                    projected_points[lu].x, projected_points[lu].y);
        }
        
        // Now draw the body rings
        // Bottom ring (continuous path)
        for (int i = 5; i <= 12; i++) {
            int next_i = (i < 12) ? i + 1 : 5;  // Loop back to start
            draw_line(dac_handle_x, dac_handle_y,
                     projected_points[i].x, projected_points[i].y,
                     projected_points[next_i].x, projected_points[next_i].y);
        }
        
        // Lower middle ring
        for (int i = 13; i <= 20; i++) {
            int next_i = (i < 20) ? i + 1 : 13;
            draw_line(dac_handle_x, dac_handle_y,
                     projected_points[i].x, projected_points[i].y,
                     projected_points[next_i].x, projected_points[next_i].y);
        }
        
        // Middle ring
        for (int i = 21; i <= 28; i++) {
            int next_i = (i < 28) ? i + 1 : 21;
            draw_line(dac_handle_x, dac_handle_y,
                     projected_points[i].x, projected_points[i].y,
                     projected_points[next_i].x, projected_points[next_i].y);
        }
        
        // Upper middle ring
        for (int i = 29; i <= 36; i++) {
            int next_i = (i < 36) ? i + 1 : 29;
            draw_line(dac_handle_x, dac_handle_y,
                     projected_points[i].x, projected_points[i].y,
                     projected_points[next_i].x, projected_points[next_i].y);
        }
        
        // Connect vertical segments avoiding jumps as much as possible
        // By starting from one ring and going to the next
        for (int i = 0; i < 8; i++) {
            // Indices for the rings
            int b = 5 + i;  // Bottom ring
            int lm = 13 + i; // Lower middle ring
            int m = 21 + i;  // Middle ring
            int um = 29 + i; // Upper middle ring
            int t = 37 + i;  // Top ring
            
            // Connect rings vertically in one continuous path up and down
            draw_line(dac_handle_x, dac_handle_y,
                     projected_points[b].x, projected_points[b].y,
                     projected_points[lm].x, projected_points[lm].y);
            
            draw_line(dac_handle_x, dac_handle_y,
                     projected_points[lm].x, projected_points[lm].y,
                     projected_points[m].x, projected_points[m].y);
            
            draw_line(dac_handle_x, dac_handle_y,
                     projected_points[m].x, projected_points[m].y,
                     projected_points[um].x, projected_points[um].y);
            
            draw_line(dac_handle_x, dac_handle_y,
                     projected_points[um].x, projected_points[um].y,
                     projected_points[t].x, projected_points[t].y);
        }
        
        // The lid connections are already drawn from the beginning
        
        // Draw spout as a more detailed 3D shape with multiple paths
        // Draw the central spine of the spout
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[25].x, projected_points[25].y,
                projected_points[63].x, projected_points[63].y);
        
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[63].x, projected_points[63].y,
                projected_points[65].x, projected_points[65].y);
        
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[65].x, projected_points[65].y,
                projected_points[68].x, projected_points[68].y);
        
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[68].x, projected_points[68].y,
                projected_points[71].x, projected_points[71].y);
        
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[71].x, projected_points[71].y,
                projected_points[74].x, projected_points[74].y);
        
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[74].x, projected_points[74].y,
                projected_points[76].x, projected_points[76].y);
                
        // Draw spout bottom ring
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[64].x, projected_points[64].y,
                projected_points[65].x, projected_points[65].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[65].x, projected_points[65].y,
                projected_points[66].x, projected_points[66].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[66].x, projected_points[66].y,
                projected_points[67].x, projected_points[67].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[67].x, projected_points[67].y,
                projected_points[68].x, projected_points[68].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[68].x, projected_points[68].y,
                projected_points[69].x, projected_points[69].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[69].x, projected_points[69].y,
                projected_points[64].x, projected_points[64].y);
                
        // Draw spout middle ring
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[70].x, projected_points[70].y,
                projected_points[71].x, projected_points[71].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[71].x, projected_points[71].y,
                projected_points[72].x, projected_points[72].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[72].x, projected_points[72].y,
                projected_points[73].x, projected_points[73].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[73].x, projected_points[73].y,
                projected_points[74].x, projected_points[74].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[74].x, projected_points[74].y,
                projected_points[75].x, projected_points[75].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[75].x, projected_points[75].y,
                projected_points[70].x, projected_points[70].y);
                
        // Connect rings vertically for 3D effect
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[64].x, projected_points[64].y,
                projected_points[70].x, projected_points[70].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[66].x, projected_points[66].y,
                projected_points[72].x, projected_points[72].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[69].x, projected_points[69].y,
                projected_points[75].x, projected_points[75].y);
        
        // Draw handle as a more detailed 3D shape
        // Connect to the body
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[21].x, projected_points[21].y,
                projected_points[77].x, projected_points[77].y);
        
        // Draw the main handle curve - outer edge
        for (int i = 77; i < 92; i++) {
            draw_line(dac_handle_x, dac_handle_y,
                    projected_points[i].x, projected_points[i].y,
                    projected_points[i+1].x, projected_points[i+1].y);
        }
        
        // Connect back to the body
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[92].x, projected_points[92].y,
                projected_points[37].x, projected_points[37].y);
        
        // Add some width to the handle by drawing a second parallel path
        // Create handle inner edge points by offsetting from outer edge
        Point2D handle_inner[16]; // For the inner curve of the handle
        
        // Generate points for the inner edge of the handle
        // First and last points connect to body
        handle_inner[0].x = projected_points[77].x + 5;
        handle_inner[0].y = projected_points[77].y;
        
        handle_inner[15].x = projected_points[92].x + 5;
        handle_inner[15].y = projected_points[92].y;
        
        // Middle points form the inner curve
        for (int i = 1; i < 15; i++) {
            // Create an inner point that's offset from the outer edge
            // This creates a handle with thickness
            float angle = atan2f(projected_points[77+i].y - projected_points[37].y, 
                               projected_points[77+i].x - projected_points[37].x);
            
            // Offset perpendicular to the curve direction
            handle_inner[i].x = projected_points[77+i].x - 5*sinf(angle);
            handle_inner[i].y = projected_points[77+i].y + 5*cosf(angle);
        }
        
        // Draw the inner edge of the handle
        for (int i = 0; i < 15; i++) {
            draw_line(dac_handle_x, dac_handle_y,
                    handle_inner[i].x, handle_inner[i].y,
                    handle_inner[i+1].x, handle_inner[i+1].y);
        }
        
        // Connect inner and outer edges at several points for 3D effect
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[77].x, projected_points[77].y,
                handle_inner[0].x, handle_inner[0].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[80].x, projected_points[80].y,
                handle_inner[3].x, handle_inner[3].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[84].x, projected_points[84].y,
                handle_inner[7].x, handle_inner[7].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[88].x, projected_points[88].y,
                handle_inner[11].x, handle_inner[11].y);
                
        draw_line(dac_handle_x, dac_handle_y,
                projected_points[92].x, projected_points[92].y,
                handle_inner[15].x, handle_inner[15].y);
        
        // Update rotation angles
        angle_x += ROTATION_SPEED_X;
        angle_y += ROTATION_SPEED_Y;
        angle_z += ROTATION_SPEED_Z;
        
        // No delay between frames for maximum speed
        // The drawing itself takes long enough to create a natural frame rate
        // vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

// Function to stop the teapot demo
void stop_teapot_demo(void) {
    if (teapot_task_handle != NULL) {
        vTaskDelete(teapot_task_handle);
        teapot_task_handle = NULL;
    }
}

// Main entry point for the teapot demo
void app_main_teapot(void) {
    printf("Starting oscilloscope teapot animation...\n");
    
    // Create a simple idle task on Core 1 to keep system happy
    TaskHandle_t idle_handle = NULL;
    xTaskCreatePinnedToCore(
        idle_task_teapot,
        "idle_task_teapot",
        2048,
        NULL,
        1,
        &idle_handle,
        1  // Core 1
    );
    
    // Create our animation task on Core 0
    xTaskCreatePinnedToCore(
        teapot_animation_task,
        "teapot_animation_task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,  // Maximum priority
        &teapot_task_handle,
        0  // Core 0
    );
    
    // Since we're running directly, we don't exit
    printf("\nTeapot animation is running...\n");
    
    // We don't exit this function, let the tasks run
    while(1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
