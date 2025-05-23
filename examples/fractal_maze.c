#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_system.h"

#define DAC_CHAN_X DAC_CHAN_0 // GPIO25
#define DAC_CHAN_Y DAC_CHAN_1 // GPIO26
#define DAC_MAX_VALUE 255
#define MAX_DEPTH 8  // Increased depth for more intricate patterns
#define MIN_SIZE 5   // Smaller minimum size for finer details
#define PI 3.14159265358979323846

// Set the active pattern (0=Sierpinski, 1=Spiral, 2=Dragon, 3=Hilbert)
#define ACTIVE_PATTERN 3

// Task handle for the drawing task
TaskHandle_t fractal_maze_task_handle = NULL;

// Direction constants
typedef enum {
    DIR_UP,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT
} Direction;

// Structure for a point
typedef struct {
    int x;
    int y;
} Point;

// Function to move from current point in a specified direction
Point move_point(Point current, Direction dir, int steps) {
    Point new_point = current;
    
    switch (dir) {
        case DIR_UP:
            new_point.y = (new_point.y - steps < 0) ? 0 : new_point.y - steps;
            break;
        case DIR_RIGHT:
            new_point.x = (new_point.x + steps > DAC_MAX_VALUE) ? DAC_MAX_VALUE : new_point.x + steps;
            break;
        case DIR_DOWN:
            new_point.y = (new_point.y + steps > DAC_MAX_VALUE) ? DAC_MAX_VALUE : new_point.y + steps;
            break;
        case DIR_LEFT:
            new_point.x = (new_point.x - steps < 0) ? 0 : new_point.x - steps;
            break;
    }
    
    return new_point;
}

// Draw a line from point a to point b
void draw_line(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, Point a, Point b) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    int steps = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
    
    if (steps == 0) {
        // Just draw the point if a and b are the same
        // Draw the same point multiple times for visibility
        for (int i = 0; i < 10; i++) {
            dac_oneshot_output_voltage(dac_x, a.x);
            dac_oneshot_output_voltage(dac_y, a.y);
        }
        return;
    }
    
    float x_increment = dx / (float)steps;
    float y_increment = dy / (float)steps;
    
    float x = a.x;
    float y = a.y;
    
    // Draw the line multiple times to make it more visible
    for (int repeat = 0; repeat < 3; repeat++) {
        x = a.x;
        y = a.y;
        
        for (int i = 0; i <= steps; i++) {
            dac_oneshot_output_voltage(dac_x, (uint8_t)x);
            dac_oneshot_output_voltage(dac_y, (uint8_t)y);
            
            x += x_increment;
            y += y_increment;
        }
    }
}

// Draw a Sierpinski triangle fractal with more levels
void draw_sierpinski(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, 
                     Point p1, Point p2, Point p3, int depth) {
    // Always draw the outer triangle first
    draw_line(dac_x, dac_y, p1, p2);
    draw_line(dac_x, dac_y, p2, p3);
    draw_line(dac_x, dac_y, p3, p1);
    
    // Base case
    if (depth <= 0) {
        return;
    }
    
    // Calculate midpoints
    Point mid1 = {(p1.x + p2.x) / 2, (p1.y + p2.y) / 2};
    Point mid2 = {(p2.x + p3.x) / 2, (p2.y + p3.y) / 2};
    Point mid3 = {(p3.x + p1.x) / 2, (p3.y + p1.y) / 2};
    
    // Draw the internal triangle that forms the gasket
    draw_line(dac_x, dac_y, mid1, mid2);
    draw_line(dac_x, dac_y, mid2, mid3);
    draw_line(dac_x, dac_y, mid3, mid1);
    
    // Recursively draw the three smaller triangles up to a reasonable depth
    if (depth > 1) {
        draw_sierpinski(dac_x, dac_y, p1, mid1, mid3, depth - 1);
        draw_sierpinski(dac_x, dac_y, mid1, p2, mid2, depth - 1);
        draw_sierpinski(dac_x, dac_y, mid3, mid2, p3, depth - 1);
    }
}

// Draw a spiral pattern
void draw_spiral(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, 
                Point center, float max_radius, float spacing, float revolutions) {
    float angle_step = 0.05;
    float radius_step = spacing / (2 * PI);
    float radius = 0;
    
    for (float angle = 0; radius < max_radius; angle += angle_step) {
        radius = angle * radius_step;
        
        int x = center.x + radius * cosf(angle);
        int y = center.y + radius * sinf(angle);
        
        if (x >= 0 && x <= DAC_MAX_VALUE && y >= 0 && y <= DAC_MAX_VALUE) {
            dac_oneshot_output_voltage(dac_x, x);
            dac_oneshot_output_voltage(dac_y, y);
        }
    }
}

// Draw a dragon curve fractal that fills more of the screen
void draw_dragon_curve(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y,
                      Point start, int size, int iterations) {
    // Pre-generate dragon curve turns
    bool turns[iterations];
    for (int i = 0; i < iterations; i++) {
        turns[i] = false;
    }
    
    // Increase scale factor to make the dragon curve much larger
    int scale = 7;  // Larger scale
    
    // Offset the starting point to center the curve better
    Point current = {DAC_MAX_VALUE/2 - 50, DAC_MAX_VALUE/2 - 50};  // Center of screen with offset
    int direction = 0; // 0 = right, 1 = up, 2 = left, 3 = down
    
    // Start point
    dac_oneshot_output_voltage(dac_x, current.x);
    dac_oneshot_output_voltage(dac_y, current.y);
    
    // Store all points first to determine bounds
    #define MAX_DRAGON_POINTS 4096
    Point points[MAX_DRAGON_POINTS];
    int num_points = 1;
    points[0] = current;
    
    // Pre-calculate all points
    for (int i = 0; i < (1 << iterations) - 1 && num_points < MAX_DRAGON_POINTS; i++) {
        // Calculate the dragon curve turn sequence
        int j = 0;
        int n = i + 1;
        while ((n & 1) == 0) {
            j++;
            n >>= 1;
        }
        bool turn = ((n & 2) == 0) ^ turns[j];
        turns[j] = turn;
        
        // Turn direction (true = right, false = left)
        if (turn) {
            direction = (direction + 1) % 4;
        } else {
            direction = (direction + 3) % 4;
        }
        
        // Move in the current direction with scaling
        switch (direction) {
            case 0: current.x += scale; break; // Right
            case 1: current.y -= scale; break; // Up
            case 2: current.x -= scale; break; // Left
            case 3: current.y += scale; break; // Down
        }
        
        // Add point to array
        points[num_points++] = current;
    }
    
    // Find min/max bounds
    int min_x = DAC_MAX_VALUE, min_y = DAC_MAX_VALUE;
    int max_x = 0, max_y = 0;
    
    for (int i = 0; i < num_points; i++) {
        if (points[i].x < min_x) min_x = points[i].x;
        if (points[i].y < min_y) min_y = points[i].y;
        if (points[i].x > max_x) max_x = points[i].x;
        if (points[i].y > max_y) max_y = points[i].y;
    }
    
    // Calculate scaling factors to center and fit on screen
    float width = max_x - min_x;
    float height = max_y - min_y;
    float scale_x = (DAC_MAX_VALUE - 40) / width;
    float scale_y = (DAC_MAX_VALUE - 40) / height;
    float scale_factor = (scale_x < scale_y) ? scale_x : scale_y;
    
    // Center offset
    int offset_x = (DAC_MAX_VALUE - width * scale_factor) / 2;
    int offset_y = (DAC_MAX_VALUE - height * scale_factor) / 2;
    
    // Rescale and draw all points
    for (int i = 1; i < num_points; i++) {
        Point p1, p2;
        
        // Scale and translate points
        p1.x = (points[i-1].x - min_x) * scale_factor + offset_x;
        p1.y = (points[i-1].y - min_y) * scale_factor + offset_y;
        p2.x = (points[i].x - min_x) * scale_factor + offset_x;
        p2.y = (points[i].y - min_y) * scale_factor + offset_y;
        
        // Draw line between points
        draw_line(dac_x, dac_y, p1, p2);
    }
}

// Generate a Hilbert curve (space-filling fractal) - improved version
void draw_hilbert(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y,
                 int x0, int y0, int size) {
    // Increase order for more detail, but stay within memory limits
    int order = 6;
    int total = 1 << (order * 2);  // Total number of points
    
    // Points array to store coordinates
    Point points[total];
    
    // Pre-generate Hilbert curve points
    for (int i = 0; i < total; i++) {
        // Convert index to (x,y) coordinates using Hilbert indexing
        int x = 0, y = 0;
        for (int j = 0; j < order; j++) {
            int mask = 1 << j;
            int idx = (i >> (j * 2)) & 3;
            
            switch (idx) {
                case 0: { int t = x; x = y; y = t; break; }
                case 1: { y += mask; break; }
                case 2: { x += mask; y += mask; break; }
                case 3: { int t = mask - 1 - x; x = mask - 1 - y; y = t; x += mask; break; }
            }
        }
        
        // Use most of the screen with some margin
        int margin = 20;
        int usable_size = DAC_MAX_VALUE - 2 * margin;
        
        // Scale and translate to center in screen
        points[i].x = margin + (x * usable_size) / (1 << order);
        points[i].y = margin + (y * usable_size) / (1 << order);
    }
    
    // Draw the curve by connecting points with lines for better visibility
    // Draw multiple times for better persistence
    for (int repeat = 0; repeat < 3; repeat++) {
        for (int i = 1; i < total; i++) {
            if (points[i-1].x >= 0 && points[i-1].x <= DAC_MAX_VALUE && 
                points[i-1].y >= 0 && points[i-1].y <= DAC_MAX_VALUE &&
                points[i].x >= 0 && points[i].x <= DAC_MAX_VALUE && 
                points[i].y >= 0 && points[i].y <= DAC_MAX_VALUE) {
                draw_line(dac_x, dac_y, points[i-1], points[i]);
            }
        }
    }
}

// Main fractal generation function that rotates through different patterns
void generate_fractal_maze(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, 
                          Point center, int size, int fractal_type) {
    int margin = 20; // Keep a margin from the edges
    int effective_size = size - 2 * margin;
    
    static int last_type = -1;
    if (fractal_type != last_type) {
        // Only print when changing patterns
        last_type = fractal_type;
    }
    
    switch (fractal_type % 4) {
        case 0: {
            // Sierpinski Triangle - more levels
            Point p1 = {center.x, center.y - effective_size/2};
            Point p2 = {center.x - effective_size/2, center.y + effective_size/2};
            Point p3 = {center.x + effective_size/2, center.y + effective_size/2};
            
            draw_sierpinski(dac_x, dac_y, p1, p2, p3, 5); // More levels for detail
            break;
        }
        case 1: {
            // Spiral pattern
            draw_spiral(dac_x, dac_y, center, effective_size/2, 4.0, 10.0);
            break;
        }
        case 2: {
            // Dragon curve - now auto-centered
            Point start = {0, 0}; // Starting point doesn't matter, will be auto-centered
            draw_dragon_curve(dac_x, dac_y, start, effective_size/2, 12);
            break;
        }
        case 3: {
            // Hilbert curve - only drawing once since the function now draws multiple times internally
            draw_hilbert(dac_x, dac_y, 0, 0, 0); // parameters ignored in new implementation
            break;
        }
    }
}

// A simple task that periodically yields to keep FreeRTOS happy
void idle_task_fractal_maze(void *pvParameters) 
{
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// Main drawing task for the fractal maze
void draw_fractal_maze_task(void *pvParameters)
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
    
    printf("Starting fractal pattern display on oscilloscope...\n");
    
    // Center point for drawing fractals
    Point center = {DAC_MAX_VALUE/2, DAC_MAX_VALUE/2};
    
    // Seed the random number generator
    srand(42);  // Fixed seed for reproducible patterns
    
    // Main drawing loop - runs forever with just the selected pattern
    printf("Starting Hilbert Curve pattern display\n");
    
    // Use the predefined pattern from ACTIVE_PATTERN
    const int pattern_to_show = ACTIVE_PATTERN;
    
    switch (pattern_to_show) {
        case 0:
            printf("Showing Sierpinski Triangle pattern\n");
            break;
        case 1:
            printf("Showing Spiral pattern\n");
            break;
        case 2:
            printf("Showing Dragon Curve pattern\n");
            break;
        case 3:
            printf("Showing Hilbert Curve pattern\n");
            break;
    }
    
    // Main loop - just keep drawing the same pattern
    while (1) {
        // Draw the selected pattern continuously
        generate_fractal_maze(dac_handle_x, dac_handle_y, center, DAC_MAX_VALUE, pattern_to_show);
        
        // Brief yield to reset watchdog
        taskYIELD();
    }
}

// Function to stop the fractal maze demo
void stop_fractal_maze_demo(void) {
    if (fractal_maze_task_handle != NULL) {
        vTaskDelete(fractal_maze_task_handle);
        fractal_maze_task_handle = NULL;
    }
}

// Main entry point for the fractal maze demo
void app_main_fractal_maze(void)
{
    printf("Starting fractal maze pattern on oscilloscope...\n");
    
    // Create a simple idle task on Core 1 to keep system happy
    TaskHandle_t idle_handle = NULL;
    xTaskCreatePinnedToCore(
        idle_task_fractal_maze,
        "idle_task_fractal_maze",
        2048,
        NULL,
        1,
        &idle_handle,
        1  // Core 1
    );
    
    // Create our drawing task on Core 0
    xTaskCreatePinnedToCore(
        draw_fractal_maze_task,
        "draw_fractal_maze_task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,  // Maximum priority
        &fractal_maze_task_handle,
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
    stop_fractal_maze_demo();
}
