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
#define PARTICLE_COUNT 80     // Number of particles for corner hit effects (increased)
#define PARTICLE_LIFETIME 200 // How long particles live (in frames) - balanced for visibility
#define CORNER_THRESHOLD 15   // Distance from corner to trigger effect (pixels)
#define PARTICLE_REDRAW_COUNT 0 // Number of times to redraw particles per frame for better visibility
#define DEBUG_PARTICLE_EFFECT 0 // Set to 1 to show particle effects on all bounces
#define DEBUG_FIRST_CORNER 1    // Set to 1 to aim logo at top-left corner for first hit, 0 for random edge hit
#define DEBUG_LOGS 0 // Set to 1 to enable diagnostic logs

// Task handle
TaskHandle_t bounce_task_handle = NULL;

// Corner position enum
typedef enum {
    CORNER_NONE = 0,
    CORNER_TOP_LEFT,
    CORNER_TOP_RIGHT,
    CORNER_BOTTOM_LEFT,
    CORNER_BOTTOM_RIGHT
} CornerPosition;

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

// Function declarations
void create_corner_effect(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, float x, float y, CornerPosition corner);
CornerPosition is_near_corner(float x, float y, LogoState *logo);

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
void create_corner_effect(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, float x, float y, CornerPosition corner) {
    if (DEBUG_LOGS) {
        printf("CREATING EFFECT: pos=(%f,%f), corner=%d\n", x, y, corner);
    }
    
    // Calculate the corner position coordinates
    float corner_x = x;
    float corner_y = y;
    float half_width = (LOGO_WIDTH * LOGO_SCALE) / 2;
    float half_height = (LOGO_HEIGHT * LOGO_SCALE) / 2;
    
    // Adjust coordinates based on which corner was hit
    switch (corner) {
        case CORNER_TOP_LEFT:
            corner_x = x - half_width;
            corner_y = y - half_height;
            break;
        case CORNER_TOP_RIGHT:
            corner_x = x + half_width;
            corner_y = y - half_height;
            break;
        case CORNER_BOTTOM_LEFT:
            corner_x = x - half_width;
            corner_y = y + half_height;
            break;
        case CORNER_BOTTOM_RIGHT:
            corner_x = x + half_width;
            corner_y = y + half_height;
            break;
        default:
            // If no specific corner, use the center of the logo
            break;
    }
    
    // Activate all particles
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        particles[i].x = corner_x;
        particles[i].y = corner_y;
        
        // Random velocity in all directions
        float angle = ((float)rand() / RAND_MAX) * 2 * PI;
        // Use original particle speed - these worked well
        float speed = 2.0f + ((float)rand() / RAND_MAX) * 6.0f;
        
        particles[i].vx = cos(angle) * speed;
        particles[i].vy = sin(angle) * speed;
        // Add a small amount of randomness to lifetime
        particles[i].lifetime = PARTICLE_LIFETIME - 20 + (rand() % 40);
        particles[i].active = true;
    }
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
            
            // Deactivate particles when their lifetime expires, fall off screen, or barely moving
            if (particles[i].lifetime <= 0 || 
                particles[i].y > 300 || 
                // Add a slight variation based on particle index
                (particles[i].y > 250 && fabs(particles[i].vy) < (0.15f + (i % 10) * 0.01f))) {
                particles[i].active = false;
            }
            
            // Bounce off side edges
            // Also reposition particles to prevent them from wrapping around
            if (particles[i].x <= 0) {
                particles[i].x = 0.1f; // Move slightly inside the boundary
                particles[i].vx = fabs(particles[i].vx) * 0.8f; // Force positive x velocity
            } else if (particles[i].x >= 255) {
                particles[i].x = 254.9f; // Move slightly inside the boundary
                particles[i].vx = -fabs(particles[i].vx) * 0.8f; // Force negative x velocity
            }
            
            // Bounce off top only
            if (particles[i].y <= 0) {
                particles[i].y = 0.1f; // Move slightly inside the boundary
                particles[i].vy = fabs(particles[i].vy) * 0.6f; // Force positive y velocity (downward)
            } 
            // For bottom edge - bounce with slight randomization
            else if (particles[i].y >= 255) {
                // Move slightly inside boundary
                particles[i].y = 254.0f;
                
                // Bounce with 40% of incoming velocity, plus a small random factor
                particles[i].vy = -particles[i].vy * (0.4f + ((float)(rand() % 10) / 100.0f));
                
                // Minimum bounce velocity
                if (particles[i].vy > -0.2f) {
                    particles[i].vy = -0.2f - ((float)(rand() % 10) / 100.0f);
                }
            }
            
            // Gradually slow down horizontal movement for more natural effect
            // Add just a tiny bit of variation
            particles[i].vx *= (0.99f - ((float)(i % 5) / 1000.0f));
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

// Function to check if we're near a corner and return which corner
CornerPosition is_near_corner(float x, float y, LogoState *logo) {
    float half_width = (LOGO_WIDTH * LOGO_SCALE) / 2;
    float half_height = (LOGO_HEIGHT * LOGO_SCALE) / 2;
    
    // Calculate distances to each edge
    float left_dist = x - half_width;
    float right_dist = SCREEN_WIDTH - (x + half_width);
    float top_dist = y - half_height;
    float bottom_dist = SCREEN_HEIGHT - (y + half_height);
    
    // Use the defined corner threshold
    float threshold = CORNER_THRESHOLD;
    
    // Check top-left corner
    if (left_dist < threshold && top_dist < threshold)
        return CORNER_TOP_LEFT;
    
    // Check top-right corner
    if (right_dist < threshold && top_dist < threshold)
        return CORNER_TOP_RIGHT;
    
    // Check bottom-left corner
    if (left_dist < threshold && bottom_dist < threshold)
        return CORNER_BOTTOM_LEFT;
    
    // Check bottom-right corner
    if (right_dist < threshold && bottom_dist < threshold)
        return CORNER_BOTTOM_RIGHT;
    
    return CORNER_NONE;
}


// Draw the DVD logo at the current position - using generated path segments
void draw_dvd_logo(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, LogoState *logo) {
    float cx = logo->x;
    float cy = logo->y;
    
    // Draw each path segment from the generated header file
    for (int path_idx = 0; path_idx < DVD_LOGO_PATH_COUNT; path_idx++) {
        const float* path_points = dvd_logo_paths[path_idx];
        int point_count = dvd_logo_path_lengths[path_idx];
        
        if (point_count < 2) continue; // Skip empty segments
        
        // Calculate points for this segment
        for (int i = 0; i < point_count - 1; i++) {
            // Get current and next points
            float x1 = path_points[i*2] * LOGO_WIDTH * LOGO_SCALE + cx - (LOGO_WIDTH * LOGO_SCALE / 2);
            float y1 = path_points[i*2+1] * LOGO_HEIGHT * LOGO_SCALE + cy - (LOGO_HEIGHT * LOGO_SCALE / 2);
            float x2 = path_points[(i+1)*2] * LOGO_WIDTH * LOGO_SCALE + cx - (LOGO_WIDTH * LOGO_SCALE / 2);
            float y2 = path_points[(i+1)*2+1] * LOGO_HEIGHT * LOGO_SCALE + cy - (LOGO_HEIGHT * LOGO_SCALE / 2);
            
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
        
        // Close the path by connecting last point to first point
        if (point_count >= 3) {
            // Get last and first points
            float x1 = path_points[(point_count-1)*2] * LOGO_WIDTH * LOGO_SCALE + cx - (LOGO_WIDTH * LOGO_SCALE / 2);
            float y1 = path_points[(point_count-1)*2+1] * LOGO_HEIGHT * LOGO_SCALE + cy - (LOGO_HEIGHT * LOGO_SCALE / 2);
            float x2 = path_points[0] * LOGO_WIDTH * LOGO_SCALE + cx - (LOGO_WIDTH * LOGO_SCALE / 2);
            float y2 = path_points[1] * LOGO_HEIGHT * LOGO_SCALE + cy - (LOGO_HEIGHT * LOGO_SCALE / 2);
            
            // Flip Y coordinates for correct orientation
            y1 = 255 - y1;
            y2 = 255 - y2;
            
            // Calculate distance for proper interpolation
            float dx = x2 - x1;
            float dy = y2 - y1;
            float distance = sqrtf(dx*dx + dy*dy);
            
            // Calculate number of steps based on distance
            int steps = (int)(distance * 2.5f);
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
    }
}

// Update logo position and handle bouncing
// Returns true if the logo bounced off any wall during this update
bool update_logo_position(LogoState *logo) {
    float half_width = (LOGO_WIDTH * LOGO_SCALE) / 2;
    float half_height = (LOGO_HEIGHT * LOGO_SCALE) / 2;
    bool hit_corner = false;
    bool bounced = false;
    
    // Track old velocity for debugging
    // (old position not needed)
    float old_vx = logo->vx;
    float old_vy = logo->vy;
    
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
    
    // If we hit a corner, record it but don't do anything special
    // The main loop will handle particle effects
    if (hit_corner && DEBUG_LOGS) {
        printf("CORNER HIT in update_position: (%f,%f) vx=%f->%f, vy=%f->%f\n", 
               logo->x, logo->y, old_vx, logo->vx, old_vy, logo->vy);
    } else if (bounced && DEBUG_LOGS) {
        printf("WALL HIT in update_position: (%f,%f) vx=%f->%f, vy=%f->%f\n", 
               logo->x, logo->y, old_vx, logo->vx, old_vy, logo->vy);
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
    
    // Initialize logo position and velocity
    float vx, vy, x_pos, y_pos;
    
    if (DEBUG_FIRST_CORNER) {
        // Initialize logo to hit top-left corner on first bounce
        
        // The problem is that the logo is a rectangle, not a square
        // To hit the corner exactly, we need to adjust the direction based on the 
        // different speeds at which it reaches the left and top edges
        
        // Start with a velocity in the general direction of the top-left corner
        vx = -1.0f; // Moving left
        vy = -1.0f; // Moving up
        
        // Position more to the right than to the bottom, to compensate for the rectangle shape
        // This creates an asymmetrical starting position that will lead to a corner hit
        float offset_factor = (float)LOGO_WIDTH / (float)LOGO_HEIGHT;
        x_pos = SCREEN_WIDTH / 2 + (offset_factor - 1.0f) * 30;  // Shift right based on aspect ratio
        y_pos = SCREEN_HEIGHT / 2;
        
        printf("DEBUG_FIRST_CORNER: Aiming for top-left corner hit\n");
    } else {
        // Initialize logo in center with random direction
        x_pos = SCREEN_WIDTH / 2;
        y_pos = SCREEN_HEIGHT / 2;
        
        // Random angle between 0 and 2π
        float angle = ((float)rand() / RAND_MAX) * 2 * PI;
        vx = cosf(angle);
        vy = sinf(angle);
        
        printf("Starting with random direction: vx=%.2f, vy=%.2f\n", vx, vy);
    }
    
    // Position with the calculated offset to ensure corner hit
    LogoState logo = {
        .x = x_pos,
        .y = y_pos,
        .vx = vx,                // Moving left
        .vy = vy,                // Moving up
        .last_corner_hit = 0,
        .color = 0
    };
    
    printf("DVD logo bounce animation started\n");
    
    // Main animation loop
    while (1) {
        uint32_t current_time = xTaskGetTickCount() / portTICK_PERIOD_MS;
        static bool particles_active = false;
        
        // Update logo position and check if it bounced
        bool bounced = update_logo_position(&logo);
        
        // Check if we're near a corner or just bounced off an edge
        CornerPosition corner = is_near_corner(logo.x, logo.y, &logo);
        
        // Log bounce and corner status
        if (DEBUG_LOGS && bounced) {
            printf("BOUNCE: pos=(%f,%f), corner=%d\n", logo.x, logo.y, corner);
        }
        
        // Determine if we should show particle effects - ONLY when velocity changes (bounce)
        bool show_particles = false;
        
        if (bounced) {  // Only when we actually bounce
            if (DEBUG_PARTICLE_EFFECT) {
                // In debug mode, show particles on any bounce
                show_particles = true;
                if (DEBUG_LOGS) {
                    printf("DEBUG MODE: Showing particles on bounce\n");
                }
            } else {
                // In normal mode, only show particles at corners
                show_particles = (corner != CORNER_NONE);
                if (DEBUG_LOGS && corner != CORNER_NONE) {
                    printf("NORMAL MODE: Corner hit, showing particles\n");
                }
            }
        }
        
        // Check if particles are currently active
        particles_active = false;
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            if (particles[i].active) {
                particles_active = true;
                break;
            }
        }
        
        // Show particles ONLY on bounce - no time check
        if (show_particles) {
            if (DEBUG_LOGS) {
                printf("CREATING PARTICLES: time=%lu, last_hit=%lu\n", 
                       current_time, logo.last_corner_hit);
            }
            // If we need to determine which edge was hit (in debug mode)
            if (DEBUG_PARTICLE_EFFECT && corner == CORNER_NONE) {
                // Calculate which edge was hit
                float half_width = (LOGO_WIDTH * LOGO_SCALE) / 2;
                float half_height = (LOGO_HEIGHT * LOGO_SCALE) / 2;
                
                // Determine which edge is closest
                float left_dist = logo.x - half_width;
                float right_dist = SCREEN_WIDTH - (logo.x + half_width);
                float top_dist = logo.y - half_height;
                float bottom_dist = SCREEN_HEIGHT - (logo.y + half_height);
                
                // Find the minimum distance to determine which edge/corner
                if (left_dist <= right_dist && left_dist <= top_dist && left_dist <= bottom_dist) {
                    // Left edge is closest
                    corner = (top_dist <= bottom_dist) ? CORNER_TOP_LEFT : CORNER_BOTTOM_LEFT;
                } 
                else if (right_dist <= left_dist && right_dist <= top_dist && right_dist <= bottom_dist) {
                    // Right edge is closest
                    corner = (top_dist <= bottom_dist) ? CORNER_TOP_RIGHT : CORNER_BOTTOM_RIGHT;
                }
                else if (top_dist <= left_dist && top_dist <= right_dist && top_dist <= bottom_dist) {
                    // Top edge is closest
                    corner = (left_dist <= right_dist) ? CORNER_TOP_LEFT : CORNER_TOP_RIGHT;
                }
                else {
                    // Bottom edge is closest
                    corner = (left_dist <= right_dist) ? CORNER_BOTTOM_LEFT : CORNER_BOTTOM_RIGHT;
                }
            }
            
            // Create particle effect at the corner of the logo
            create_corner_effect(dac_handle_x, dac_handle_y, logo.x, logo.y, corner);
            logo.last_corner_hit = current_time;
        }
        
        // Draw the DVD logo
        draw_dvd_logo(dac_handle_x, dac_handle_y, &logo);
        
        // Draw active particles multiple times per frame to make them more visible
        if (particles_active) {
            // First update them once (physics, lifetime, etc.)
            update_and_draw_particles(dac_handle_x, dac_handle_y);
            
            // Then draw them repeatedly without updating, to increase brightness
            for (int i = 0; i < PARTICLE_REDRAW_COUNT; i++) {
                // Only draw, don't update
                for (int j = 0; j < PARTICLE_COUNT; j++) {
                    if (particles[j].active) {
                        // Flip Y coordinate to fix orientation
                        float display_y = 255 - particles[j].y;
                        
                        // Draw the particle
                        uint8_t x_val = (uint8_t)fmin(fmax(particles[j].x, 0), 255);
                        uint8_t y_val = (uint8_t)fmin(fmax(display_y, 0), 255);
                        
                        // Apply inversion if configured
                        if (INVERT_X) {
                            x_val = 255 - x_val;
                        }
                        if (INVERT_Y) {
                            y_val = 255 - y_val;
                        }
                        
                        // Calculate size based on remaining lifetime
                        int size = 1;
                        if (particles[j].lifetime > PARTICLE_LIFETIME * 0.7f) {
                            size = 3;  // Full size at start
                        } else if (particles[j].lifetime > PARTICLE_LIFETIME * 0.4f) {
                            size = 2;  // Medium size in middle
                        } else {
                            size = 1;  // Smallest at end
                        }
                        
                        // Always draw center point
                        fast_dac_update(dac_handle_x, dac_handle_y, x_val, y_val);
                        
                        // Draw expanded pattern based on size
                        if (size >= 2) {
                            // Horizontal and vertical points
                            fast_dac_update(dac_handle_x, dac_handle_y, x_val + 1, y_val);
                            fast_dac_update(dac_handle_x, dac_handle_y, x_val - 1, y_val);
                            fast_dac_update(dac_handle_x, dac_handle_y, x_val, y_val + 1);
                            fast_dac_update(dac_handle_x, dac_handle_y, x_val, y_val - 1);
                        }
                        
                        // Extra brightness for center point
                        fast_dac_update(dac_handle_x, dac_handle_y, x_val, y_val);
                    }
                }
            }
        } else {
            // No active particles, just draw once
            update_and_draw_particles(dac_handle_x, dac_handle_y);
        }
        
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
