#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "dvd_logo_points.h"  // Include the generated header file

#define DAC_CHAN_X DAC_CHAN_0 // GPIO25
#define DAC_CHAN_Y DAC_CHAN_1 // GPIO26
#define DAC_MAX_VALUE 255
#define PI 3.14159265358979323846

// Inversion settings - adjust if needed for your oscilloscope
#define INVERT_X 1  // Set to 1 to invert X axis
#define INVERT_Y 0  // Set to 1 to invert Y axis

// DVD logo size and scaling
#define LOGO_SCALE 1.0     // Scale factor for the logo (1.0 = 100% of original size, ~3x bigger than before)
#define LOGO_WIDTH 130     // Original width after scaling
#define LOGO_HEIGHT 60     // Original height after scaling
#define SCREEN_WIDTH 255   // Screen width in DAC values
#define SCREEN_HEIGHT 255  // Screen height in DAC values

// Animation settings
#define ANIMATION_SPEED 8  // Pixels per frame (much faster movement)
#define PARTICLE_COUNT 40     // Number of particles for corner hit effects (increased)
#define PARTICLE_LIFETIME 200 // How long particles live (in frames) - doubled for more visibility
#define CORNER_THRESHOLD 15   // Distance from corner to trigger effect (pixels)
#define CORNER_PAUSE_TIME 5000 // Time to pause in corner when particles are firing (ms)
#define DEBUG_PARTICLE_EFFECT 1 // Set to 0 for normal mode, 1 to show particle effects on all bounces

// Task handle
TaskHandle_t bounce_task_handle = NULL;

// Define a particle struct for corner hit effects
typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    int lifetime;
    bool active;
} Particle;

// Array of particles for corner effects
Particle particles[PARTICLE_COUNT];

// Structure to track DVD logo position and velocity
typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    uint32_t last_corner_hit;
    uint8_t color;
} LogoState;

// Manually defined DVD logo paths with clean breaks between segments
// Each path segment is a separate array of x,y coordinates scaled 0-1

// First D - outer path
const float dvd_first_d_outer[] = {
    0.10, 0.20,  // Top left
    0.25, 0.20,  // Top right
    0.30, 0.25,  // Top curve
    0.32, 0.35,  // Right side
    0.30, 0.45,  // Bottom curve
    0.25, 0.50,  // Bottom right
    0.10, 0.50,  // Bottom left
    0.10, 0.20   // Back to start
};
#define FIRST_D_OUTER_POINTS (sizeof(dvd_first_d_outer) / (2 * sizeof(float)))

// First D - inner cutout
const float dvd_first_d_inner[] = {
    0.15, 0.25,  // Inner top left
    0.22, 0.25,  // Inner top right
    0.25, 0.30,  // Inner top curve
    0.25, 0.40,  // Inner right side
    0.22, 0.45,  // Inner bottom curve
    0.15, 0.45,  // Inner bottom left
    0.15, 0.25   // Back to inner start
};
#define FIRST_D_INNER_POINTS (sizeof(dvd_first_d_inner) / (2 * sizeof(float)))

// V shape
const float dvd_v_shape[] = {
    0.35, 0.20,  // Left top
    0.45, 0.50,  // Bottom middle
    0.55, 0.20   // Right top
};
#define V_SHAPE_POINTS (sizeof(dvd_v_shape) / (2 * sizeof(float)))

// Second D - outer path
const float dvd_second_d_outer[] = {
    0.60, 0.20,  // Top left
    0.75, 0.20,  // Top right
    0.80, 0.25,  // Top curve
    0.82, 0.35,  // Right side
    0.80, 0.45,  // Bottom curve
    0.75, 0.50,  // Bottom right
    0.60, 0.50,  // Bottom left
    0.60, 0.20   // Back to start
};
#define SECOND_D_OUTER_POINTS (sizeof(dvd_second_d_outer) / (2 * sizeof(float)))

// Second D - inner cutout
const float dvd_second_d_inner[] = {
    0.65, 0.25,  // Inner top left
    0.72, 0.25,  // Inner top right
    0.75, 0.30,  // Inner top curve
    0.75, 0.40,  // Inner right side
    0.72, 0.45,  // Inner bottom curve
    0.65, 0.45,  // Inner bottom left
    0.65, 0.25   // Back to inner start
};
#define SECOND_D_INNER_POINTS (sizeof(dvd_second_d_inner) / (2 * sizeof(float)))

// Outer oval
const float dvd_outer_oval[] = {
    0.45, 0.70,  // Center top
    0.55, 0.70,  // Right top
    0.65, 0.72,  // Right upper
    0.75, 0.75,  // Right
    0.80, 0.80,  // Right lower
    0.75, 0.85,  // Bottom right
    0.65, 0.88,  // Bottom
    0.55, 0.90,  // Bottom left
    0.45, 0.90,  // Left bottom
    0.35, 0.88,  // Left
    0.25, 0.85,  // Left upper
    0.20, 0.80,  // Top left
    0.25, 0.75,  // Top
    0.35, 0.72,  // Top right
    0.45, 0.70   // Back to start
};
#define OUTER_OVAL_POINTS (sizeof(dvd_outer_oval) / (2 * sizeof(float)))

// Inner oval
const float dvd_inner_oval[] = {
    0.45, 0.75,  // Center top
    0.55, 0.75,  // Right top
    0.62, 0.77,  // Right upper
    0.67, 0.80,  // Right
    0.62, 0.83,  // Right lower
    0.55, 0.85,  // Bottom right
    0.45, 0.85,  // Bottom left
    0.35, 0.83,  // Left bottom
    0.30, 0.80,  // Left
    0.35, 0.77,  // Left upper
    0.45, 0.75   // Back to start
};
#define INNER_OVAL_POINTS (sizeof(dvd_inner_oval) / (2 * sizeof(float)))

// Fast DAC update - attempts to make X and Y updates as close to atomic as possible
static inline void fast_dac_update(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, uint8_t x_val, uint8_t y_val) {
    // Update both DACs as quickly as possible to minimize time between updates
    portDISABLE_INTERRUPTS();
    dac_oneshot_output_voltage(dac_x, x_val);
    dac_oneshot_output_voltage(dac_y, y_val);
    portENABLE_INTERRUPTS();
}

// Initialize particles for corner hit effect
void init_particles() {
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        particles[i].active = false;
        particles[i].lifetime = 0;
    }
}

// Create particle explosion effect when hitting a corner
void create_corner_effect(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, float x, float y) {
    // Activate all particles
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        particles[i].x = x;
        particles[i].y = y;
        
        // Random velocity in all directions
        float angle = ((float)rand() / RAND_MAX) * 2 * PI;
        // Use original particle speed - these worked well
        float speed = 2.0f + ((float)rand() / RAND_MAX) * 6.0f;
        
        particles[i].vx = cos(angle) * speed;
        particles[i].vy = sin(angle) * speed;
        particles[i].lifetime = PARTICLE_LIFETIME;
        particles[i].active = true;
    }
    
    // No firework burst effect - removed
}

// Update and draw all active particles
void update_and_draw_particles(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y) {
    // First pass: update all particles
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        if (particles[i].active) {
            // Update position
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;
            
            // Apply gravity - keep original value
            particles[i].vy += 0.25f;
            
            // Decay lifetime
            particles[i].lifetime--;
            
            // Only deactivate particles when they fall below the screen
            // This lets them gradually fall off rather than suddenly disappearing
            if (particles[i].y > 300 || particles[i].lifetime <= 0) {
                particles[i].active = false;
            }
            
            // Bounce off side edges, but allow falling off bottom
            if (particles[i].x <= 0 || particles[i].x >= 255) {
                particles[i].vx = -particles[i].vx * 0.8f;
            }
            // Only bounce off top, let them fall through bottom
            if (particles[i].y <= 0) {
                particles[i].vy = -particles[i].vy * 0.6f; // Less bounce
            }
            
            // Gradually slow down horizontal movement for more natural effect
            particles[i].vx *= 0.99f;
        }
    }
    
    // Second pass: draw all active particles with size based on lifetime
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        if (particles[i].active) {
            // Flip Y coordinate to fix orientation
            float display_y = 255 - particles[i].y;
            
            // Draw the particle
            uint8_t x_val = (uint8_t)fmin(fmax(particles[i].x, 0), 255);
            uint8_t y_val = (uint8_t)fmin(fmax(display_y, 0), 255);
            
            // Apply inversion if configured
            if (INVERT_X) {
                x_val = 255 - x_val;
            }
            if (INVERT_Y) {
                y_val = 255 - y_val;
            }
            
            // Calculate size based on remaining lifetime - particles get smaller as they age
            int size = 1;
            if (particles[i].lifetime > PARTICLE_LIFETIME * 0.7f) {
                size = 3;  // Full size at start
            } else if (particles[i].lifetime > PARTICLE_LIFETIME * 0.4f) {
                size = 2;  // Medium size in middle
            } else {
                size = 1;  // Smallest at end
            }
            
            // Always draw center point
            fast_dac_update(dac_x, dac_y, x_val, y_val);
            
            // Draw expanded pattern based on size
            if (size >= 2) {
                // Horizontal and vertical points
                fast_dac_update(dac_x, dac_y, x_val + 1, y_val);
                fast_dac_update(dac_x, dac_y, x_val - 1, y_val);
                fast_dac_update(dac_x, dac_y, x_val, y_val + 1);
                fast_dac_update(dac_x, dac_y, x_val, y_val - 1);
                
                // Diagonal points
                fast_dac_update(dac_x, dac_y, x_val + 1, y_val + 1);
                fast_dac_update(dac_x, dac_y, x_val - 1, y_val - 1);
                fast_dac_update(dac_x, dac_y, x_val + 1, y_val - 1);
                fast_dac_update(dac_x, dac_y, x_val - 1, y_val + 1);
            }
            
            // Add outer points for largest size
            if (size >= 3) {
                fast_dac_update(dac_x, dac_y, x_val + 2, y_val);
                fast_dac_update(dac_x, dac_y, x_val - 2, y_val);
                fast_dac_update(dac_x, dac_y, x_val, y_val + 2);
                fast_dac_update(dac_x, dac_y, x_val, y_val - 2);
            }
            
            // Extra brightness for center point
            fast_dac_update(dac_x, dac_y, x_val, y_val);
        }
    }
}

// Function to check if we're near a corner
bool is_near_corner(float x, float y, LogoState *logo) {
    float half_width = (LOGO_WIDTH * LOGO_SCALE) / 2;
    float half_height = (LOGO_HEIGHT * LOGO_SCALE) / 2;
    
    // Check top-left corner
    if (x - half_width < CORNER_THRESHOLD && y - half_height < CORNER_THRESHOLD)
        return true;
    
    // Check top-right corner
    if (SCREEN_WIDTH - (x + half_width) < CORNER_THRESHOLD && y - half_height < CORNER_THRESHOLD)
        return true;
    
    // Check bottom-left corner
    if (x - half_width < CORNER_THRESHOLD && SCREEN_HEIGHT - (y + half_height) < CORNER_THRESHOLD)
        return true;
    
    // Check bottom-right corner
    if (SCREEN_WIDTH - (x + half_width) < CORNER_THRESHOLD && SCREEN_HEIGHT - (y + half_height) < CORNER_THRESHOLD)
        return true;
    
    return false;
}

// Helper function to draw a segment of the DVD logo
void draw_logo_segment(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, 
                      const float *points, int num_points, float cx, float cy) {
    // Draw the segment with proper loops to close the path
    for (int i = 0; i < num_points - 1; i++) {
        // Get current and next point
        float x1 = points[i*2] * LOGO_WIDTH * LOGO_SCALE + cx - (LOGO_WIDTH * LOGO_SCALE / 2);
        float y1 = points[i*2+1] * LOGO_HEIGHT * LOGO_SCALE + cy - (LOGO_HEIGHT * LOGO_SCALE / 2);
        float x2 = points[(i+1)*2] * LOGO_WIDTH * LOGO_SCALE + cx - (LOGO_WIDTH * LOGO_SCALE / 2);
        float y2 = points[(i+1)*2+1] * LOGO_HEIGHT * LOGO_SCALE + cy - (LOGO_HEIGHT * LOGO_SCALE / 2);
        
        // Flip Y coordinates for correct orientation
        y1 = 255 - y1;
        y2 = 255 - y2;
        
        // Calculate distance for proper interpolation
        float dx = x2 - x1;
        float dy = y2 - y1;
        float distance = sqrtf(dx*dx + dy*dy);
        
        // Calculate number of steps based on distance (more steps for longer lines)
        int steps = (int)(distance * 2.5f); // 2.5 points per pixel
        if (steps < 1) steps = 1;
        
        // Interpolate points along the line
        for (int j = 0; j <= steps; j++) {
            float t = (float)j / steps;
            float x = x1 + t * dx;
            float y = y1 + t * dy;
            
            // Ensure coordinates are within DAC range
            uint8_t x_val = (uint8_t)fmin(fmax(x, 0), 255);
            uint8_t y_val = (uint8_t)fmin(fmax(y, 0), 255);
            
            // Apply inversion if configured
            if (INVERT_X) {
                x_val = 255 - x_val;
            }
            if (INVERT_Y) {
                y_val = 255 - y_val;
            }
            
            // Draw point
            fast_dac_update(dac_x, dac_y, x_val, y_val);
        }
    }
    
    // Connect last point to first point if needed (for closed shapes)
    if (num_points > 2) {
        float x1 = points[(num_points-1)*2] * LOGO_WIDTH * LOGO_SCALE + cx - (LOGO_WIDTH * LOGO_SCALE / 2);
        float y1 = points[(num_points-1)*2+1] * LOGO_HEIGHT * LOGO_SCALE + cy - (LOGO_HEIGHT * LOGO_SCALE / 2);
        float x2 = points[0] * LOGO_WIDTH * LOGO_SCALE + cx - (LOGO_WIDTH * LOGO_SCALE / 2);
        float y2 = points[1] * LOGO_HEIGHT * LOGO_SCALE + cy - (LOGO_HEIGHT * LOGO_SCALE / 2);
        
        // Flip Y coordinates
        y1 = 255 - y1;
        y2 = 255 - y2;
        
        float dx = x2 - x1;
        float dy = y2 - y1;
        float distance = sqrtf(dx*dx + dy*dy);
        
        int steps = (int)(distance * 2.5f);
        if (steps < 1) steps = 1;
        
        for (int j = 0; j <= steps; j++) {
            float t = (float)j / steps;
            float x = x1 + t * dx;
            float y = y1 + t * dy;
            
            uint8_t x_val = (uint8_t)fmin(fmax(x, 0), 255);
            uint8_t y_val = (uint8_t)fmin(fmax(y, 0), 255);
            
            if (INVERT_X) {
                x_val = 255 - x_val;
            }
            if (INVERT_Y) {
                y_val = 255 - y_val;
            }
            
            fast_dac_update(dac_x, dac_y, x_val, y_val);
        }
    }
}

// Draw the DVD logo at the current position - using manually defined segments
void draw_dvd_logo(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, LogoState *logo) {
    float cx = logo->x;
    float cy = logo->y;
    
    // Draw each segment of the logo separately
    draw_logo_segment(dac_x, dac_y, dvd_first_d_outer, FIRST_D_OUTER_POINTS, cx, cy);
    draw_logo_segment(dac_x, dac_y, dvd_first_d_inner, FIRST_D_INNER_POINTS, cx, cy);
    draw_logo_segment(dac_x, dac_y, dvd_v_shape, V_SHAPE_POINTS, cx, cy);
    draw_logo_segment(dac_x, dac_y, dvd_second_d_outer, SECOND_D_OUTER_POINTS, cx, cy);
    draw_logo_segment(dac_x, dac_y, dvd_second_d_inner, SECOND_D_INNER_POINTS, cx, cy);
    draw_logo_segment(dac_x, dac_y, dvd_outer_oval, OUTER_OVAL_POINTS, cx, cy);
    draw_logo_segment(dac_x, dac_y, dvd_inner_oval, INNER_OVAL_POINTS, cx, cy);
}

// Update logo position and handle bouncing
// Returns true if the logo bounced off any wall during this update
bool update_logo_position(LogoState *logo) {
    float half_width = (LOGO_WIDTH * LOGO_SCALE) / 2;
    float half_height = (LOGO_HEIGHT * LOGO_SCALE) / 2;
    bool hit_corner = false;
    bool bounced = false;
    
    // Force a speed reset to ensure it's moving at the right speed
    // We use the sign of current velocity but force the magnitude
    float vx_sign = (logo->vx >= 0) ? 1.0f : -1.0f;
    float vy_sign = (logo->vy >= 0) ? 1.0f : -1.0f;
    
    // Set speed - slow it down in all modes
    float speed = 1.0f;  // 10x slower than original 20.0f
    logo->vx = vx_sign * speed;
    logo->vy = vy_sign * speed;
    
    // Update position
    logo->x += logo->vx;
    logo->y += logo->vy;
    
    // Check for left/right wall collision
    if (logo->x - half_width <= 0) {
        logo->x = half_width;
        logo->vx = -logo->vx;
        bounced = true;
        
        // Check if we hit a corner
        if (logo->y - half_height <= 0 || logo->y + half_height >= SCREEN_HEIGHT) {
            hit_corner = true;
        }
    } else if (logo->x + half_width >= SCREEN_WIDTH) {
        logo->x = SCREEN_WIDTH - half_width;
        logo->vx = -logo->vx;
        bounced = true;
        
        // Check if we hit a corner
        if (logo->y - half_height <= 0 || logo->y + half_height >= SCREEN_HEIGHT) {
            hit_corner = true;
        }
    }
    
    // Check for top/bottom wall collision
    if (logo->y - half_height <= 0) {
        logo->y = half_height;
        logo->vy = -logo->vy;
        bounced = true;
        
        // Already checked for corner in the x collision check
    } else if (logo->y + half_height >= SCREEN_HEIGHT) {
        logo->y = SCREEN_HEIGHT - half_height;
        logo->vy = -logo->vy;
        bounced = true;
        
        // Already checked for corner in the x collision check
    }
    
    // If we hit a corner and it's been at least 30 seconds since the last hit
    uint32_t current_time = xTaskGetTickCount() / portTICK_PERIOD_MS;
    if (hit_corner && (current_time - logo->last_corner_hit > 30000)) {
        logo->last_corner_hit = current_time;
        logo->color = (logo->color + 1) % 7; // Change color (even though we're monochrome)
        // Don't print anything
    }
    
    return bounced;
}

// Main task to display the bouncing DVD logo
void bouncing_dvd_task(void *pvParameters) {
    printf("Starting DVD logo bounce example...\n");
    
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
    
    // Seed the random number generator
    srand(time(NULL));
    
    // Initialize particles
    init_particles();
    
    // Initialize logo state with guaranteed fast speed
    LogoState logo = {
        .x = SCREEN_WIDTH / 2,
        .y = SCREEN_HEIGHT / 2,
        .vx = 5.0f,             // Directly set to a reasonable value
        .vy = 5.0f,             // Directly set to a reasonable value
        .last_corner_hit = 0,
        .color = 0
    };
    
    printf("DVD logo bounce animation started\n");
    
    // Main animation loop
    while (1) {
        uint32_t current_time = xTaskGetTickCount() / portTICK_PERIOD_MS;
        // bool corner_hit = false; // Removed unused variable
        static uint32_t corner_hit_start_time = 0;
        static bool in_corner_pause = false;
        
        // For debug mode - show particle effect on ANY bounce
        if (DEBUG_PARTICLE_EFFECT) {
            // Update logo position and check if it bounced
            bool bounced = update_logo_position(&logo);
            
            // Create particle effect if the logo bounced or if enough time has passed
            if (bounced || (current_time - logo.last_corner_hit > 1000)) {
                // Create particles at the logo position - FORCE CREATION EVERY TIME
                for (int i = 0; i < PARTICLE_COUNT; i++) {
                    particles[i].x = logo.x;
                    particles[i].y = logo.y;
                    
                    // Random velocity in all directions
                    float angle = ((float)rand() / RAND_MAX) * 2 * PI;
                    float speed = 2.0f + ((float)rand() / RAND_MAX) * 6.0f;
                    
                    particles[i].vx = cos(angle) * speed;
                    particles[i].vy = sin(angle) * speed;
                    particles[i].lifetime = PARTICLE_LIFETIME;
                    particles[i].active = true;
                }
                
                logo.last_corner_hit = current_time;
            }
        } 
        // Normal mode
        else {
            // If we're not currently paused in a corner
            if (!in_corner_pause) {
                // Update logo position and handle bouncing (ignore return value in normal mode)
                update_logo_position(&logo);
                
                // Check if we're near a corner
                if (is_near_corner(logo.x, logo.y, &logo)) {
                    if (current_time - logo.last_corner_hit > 5000) { // Only trigger every 5 seconds
                        // Start corner pause
                        in_corner_pause = true;
                        corner_hit_start_time = current_time;
                        
                        // Create particle effect
                        create_corner_effect(dac_handle_x, dac_handle_y, logo.x, logo.y);
                        logo.last_corner_hit = current_time;
                    }
                }
            } 
            // If we're currently paused in a corner
            else {
                // Check if pause time has elapsed
                if (current_time - corner_hit_start_time > CORNER_PAUSE_TIME) {
                    // Resume normal movement
                    in_corner_pause = false;
                } 
                // If still in pause, create new particles periodically
                else if (current_time - logo.last_corner_hit > 500) {
                    create_corner_effect(dac_handle_x, dac_handle_y, logo.x, logo.y);
                    logo.last_corner_hit = current_time;
                }
            }
        }
        
        // Draw the DVD logo
        draw_dvd_logo(dac_handle_x, dac_handle_y, &logo);
        
        // Update and draw particles
        update_and_draw_particles(dac_handle_x, dac_handle_y);
        
        // Remove the delay entirely to maximize frame rate
        // This allows the task to run as fast as possible
        taskYIELD();
    }
}

// Main entry point for the DVD logo bounce example
void app_main_dvd_bounce(void) {
    printf("Initializing DVD logo bounce example...\n");
    
    // Create the bouncing DVD logo task
    xTaskCreatePinnedToCore(
        bouncing_dvd_task,
        "bouncing_dvd_task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,  // Maximum priority
        &bounce_task_handle,
        0  // Core 0
    );
    
    // Since we're running directly, we don't exit
    printf("\nDVD logo bounce is running...\n");
    
    // Keep the app_main function running
    while(1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
