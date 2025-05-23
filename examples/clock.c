#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "driver/dac_oneshot.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "hershey_fonts.h"  // Include standard Hershey font library
#include "sdkconfig.h"

#define DAC_CHAN_X DAC_CHAN_0 // GPIO25
#define DAC_CHAN_Y DAC_CHAN_1 // GPIO26
#define DAC_MAX_VALUE 255
#define CHAR_SPACING 0         // No additional space between characters
#define PI 3.14159265358979323846

// Anti-burn-in settings
#define ANTI_BURNIN_ENABLED 1           // Set to 0 to disable position shifting
#define ANTI_BURNIN_DEBUG_MODE 0        // Set to 1 to shift every second (for testing), 0 for normal minute shifts
#define ANTI_BURNIN_X_RANGE 12          // Max pixels to move horizontally (positive only to avoid cutoff)
#define ANTI_BURNIN_Y_RANGE 11          // Max pixels to move vertically (centered around 0)

// WiFi connection settings from menuconfig
#ifdef CONFIG_WIFI_SSID
#define WIFI_SSID      CONFIG_WIFI_SSID
#else
#define WIFI_SSID      "default_ssid"
#endif

#ifdef CONFIG_WIFI_PASSWORD
#define WIFI_PASSWORD  CONFIG_WIFI_PASSWORD
#else
#define WIFI_PASSWORD  "default_password"
#endif
#define MAXIMUM_RETRY  5

// Orientation control - set these to 1 to invert axes if needed for your specific oscilloscope
#define INVERT_X 1  // Set to 1 to invert X axis
#define INVERT_Y 0  // Set to 1 to invert Y axis (fixes top/bottom orientation)

// Beam settling configuration - helps eliminate curls on first points
#define BEAM_SETTLING_ENABLED 1  // Set to 0 to disable settling delay
#define BEAM_SETTLING_LOOPS 1000  // Number of delay loops to add (adjust as needed)

// Task handles
TaskHandle_t clock_task_handle = NULL;
TaskHandle_t spinner_task_handle = NULL;

// Font drawing settings
#define FONT_SCALE 25          // Scale factor for the font
#define INTERP_POINTS 6        // Number of points to interpolate between vertices

// Set to reduce the number of points used in rendering
#define OPTIMIZE_POINTS 1  // Set to 1 to enable point reduction, 0 for original behavior
#define MAX_REFRESH_RATE 0  // Set to 0 for high quality lines (more points per draw)

// Debug delay settings - uncomment to enable visualization
// #define DEBUG_DELAY
#define DEBUG_DELAY_CYCLES 25000  // Only used if DEBUG_DELAY is defined

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;

// The event group allows multiple bits for each event, but we only care about two events:
// - we are connected to the AP with an IP
// - we failed to connect after the maximum amount of retries
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "CLOCK";
static int s_retry_num = 0;
static bool time_synced = false;
static bool wifi_connected = false;
static bool spinner_running = true;
static bool spinner_active = false;

// DAC handles for sharing with spinner task
static dac_oneshot_handle_t global_dac_handle_x = NULL;
static dac_oneshot_handle_t global_dac_handle_y = NULL;

// Semaphore for synchronization between tasks
static SemaphoreHandle_t sync_semaphore = NULL;

// A simple task that periodically yields to keep FreeRTOS happy
void idle_task_clock(void *pvParameters) 
{
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// Loading spinner task runs on Core 1 while WiFi/NTP run on Core 0
void spinner_task(void *pvParameters)
{
    printf("Starting spinner task on Core 1...\n");
    
    // Quick check for DAC handles
    int retry_count = 0;
    while ((global_dac_handle_x == NULL || global_dac_handle_y == NULL) && retry_count < 5) {
        vTaskDelay(20 / portTICK_PERIOD_MS);
        retry_count++;
    }
    
    printf("Spinner task: DAC handles initialized, starting spinner\n");
    
    // Tell main task we're active and ready
    spinner_active = true;
    if (sync_semaphore != NULL) {
        xSemaphoreGive(sync_semaphore);
    }
    
    // Center coordinates
    const int center_x = 128;
    const int center_y = 128;
    const int radius = 50;  // Large radius for visibility
    
    // Draw a spinning dot until main task signals to stop
    float angle = 0.0;
    while (spinner_running) {
        // Calculate dot position around circle
        int x = center_x + (int)(radius * cos(angle));
        int y = center_y + (int)(radius * sin(angle));
        
        // Ensure values are within DAC range
        if (x < 0) x = 0;
        if (x > 255) x = 255;
        if (y < 0) y = 0;
        if (y > 255) y = 255;
        
        // Apply inversion if configured
        if (INVERT_X) {
            x = 255 - x;
        }
        if (INVERT_Y) {
            y = 255 - y;
        }
        
        // Minimize drawing iterations to reduce CPU usage
        for (int i = 0; i < 50; i++) {
            dac_oneshot_output_voltage(global_dac_handle_x, x);
            dac_oneshot_output_voltage(global_dac_handle_y, y);
        }
        
        // Small yield to allow WiFi task to get more CPU time
        taskYIELD();
        
        // Move to next position clockwise at a slower speed (3x slower)
        angle -= 0.004;  // Negative for clockwise rotation (slowed down by 3x)
        if (angle < 0) {
            angle += 2 * PI;
        }
    }
    
    // Task done, delete self
    printf("Spinner task complete\n");
    spinner_task_handle = NULL;
    vTaskDelete(NULL);
}

// Structure to pass DAC handles to the drawing callback
typedef struct {
    dac_oneshot_handle_t dac_x;
    dac_oneshot_handle_t dac_y;
    int last_x;
    int last_y;
    bool pen_was_down;
} dac_draw_ctx_t;

// Improved DAC drawing callback function for the Hershey font renderer
void dac_draw_func(int x, int y, hershey_pen_t pen, void *ctx) {
    dac_draw_ctx_t *dac_ctx = (dac_draw_ctx_t *)ctx;
    
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
        // For pen up, we want the beam to move but not linger
        dac_oneshot_output_voltage(dac_ctx->dac_x, x_dac);
        dac_oneshot_output_voltage(dac_ctx->dac_y, y_dac);
        
        dac_ctx->pen_was_down = false;
        dac_ctx->last_x = dac_x;
        dac_ctx->last_y = dac_y;
        return;
    }
    
    // If pen is down and we were previously drawing,
    // draw a line from the previous position to the current position
    if (dac_ctx->pen_was_down) {
        // Calculate the number of points to use based on distance
        int dx = dac_x - dac_ctx->last_x;
        int dy = dac_y - dac_ctx->last_y;
        
        // Use Manhattan distance for speed (no sqrt)
        int distance = abs(dx) + abs(dy);
        
        // For continuous lines, use high density interpolation
        // This creates smooth unbroken lines rather than dots
        int points;
        
        // Create dense, continuous lines for better visibility
        points = distance * 2; // 2 points per pixel distance
        
        // Add even more density for different line lengths
        if (distance > 30) {
            // For long lines, we need more points to maintain smoothness
            points = distance * 3;
        } else if (distance < 5) {
            // For very short segments, use maximum density
            points = distance * 5;
        }
        
        if (points < 1) points = 1;
        
        // Draw the line using staircase interpolation (moving one axis at a time)
        // This creates continuous lines instead of dots by avoiding diagonal jumps
        
        // First, figure out the step sizes
        int x_step = (dx != 0) ? ((dx > 0) ? 1 : -1) : 0;
        int y_step = (dy != 0) ? ((dy > 0) ? 1 : -1) : 0;
        
        // Calculate total steps needed for x and y
        int x_steps = abs(dx);
        int y_steps = abs(dy);
        
        // Current position starts at last position
        int curr_x = dac_ctx->last_x;
        int curr_y = dac_ctx->last_y;
        
        // Draw X steps first
        for (int i = 0; i < x_steps; i++) {
            curr_x += x_step;
            
            // Ensure values are within DAC range
            uint8_t x_val = (curr_x > DAC_MAX_VALUE) ? DAC_MAX_VALUE : ((curr_x < 0) ? 0 : curr_x);
            uint8_t y_val = (curr_y > DAC_MAX_VALUE) ? DAC_MAX_VALUE : ((curr_y < 0) ? 0 : curr_y);
            
            // Apply inversion if configured
            if (INVERT_X) {
                x_val = DAC_MAX_VALUE - x_val;
            }
            if (INVERT_Y) {
                y_val = DAC_MAX_VALUE - y_val;
            }
            
            // Output to DAC
            dac_oneshot_output_voltage(dac_ctx->dac_x, x_val);
            dac_oneshot_output_voltage(dac_ctx->dac_y, y_val);
        }
        
        // Then draw Y steps (after X is complete)
        for (int i = 0; i < y_steps; i++) {
            curr_y += y_step;
            
            // Ensure values are within DAC range
            uint8_t x_val = (curr_x > DAC_MAX_VALUE) ? DAC_MAX_VALUE : ((curr_x < 0) ? 0 : curr_x);
            uint8_t y_val = (curr_y > DAC_MAX_VALUE) ? DAC_MAX_VALUE : ((curr_y < 0) ? 0 : curr_y);
            
            // Apply inversion if configured
            if (INVERT_X) {
                x_val = DAC_MAX_VALUE - x_val;
            }
            if (INVERT_Y) {
                y_val = DAC_MAX_VALUE - y_val;
            }
            
            // Output to DAC
            dac_oneshot_output_voltage(dac_ctx->dac_x, x_val);
            dac_oneshot_output_voltage(dac_ctx->dac_y, y_val);
        }
    } else {
        // First point after pen up, output it with optional settling time
        dac_oneshot_output_voltage(dac_ctx->dac_x, x_dac);
        dac_oneshot_output_voltage(dac_ctx->dac_y, y_dac);
        
            // We don't need delay here anymore, it's handled at the draw_text level
    }
    
    // Update last position and pen state
    dac_ctx->last_x = dac_x;
    dac_ctx->last_y = dac_y;
    dac_ctx->pen_was_down = true;
}

// Draw text string using the Hershey font library
void draw_text(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, 
             const char* text, uint8_t x_pos, uint8_t y_pos, uint8_t size, bool add_settling_delay) {
    
    // Initialize the drawing context - start with consistent coordinates 
    // to avoid beam jumping from random locations
    static int prev_end_x = 128; // Center point
    static int prev_end_y = 128;
    
    dac_draw_ctx_t ctx = {
        .dac_x = dac_x,
        .dac_y = dac_y,
        .last_x = prev_end_x,
        .last_y = prev_end_y,
        .pen_was_down = true // Always consider the pen down - beam is always visible
    };
    
    // Add beam settling delay right at the start of drawing
    if (add_settling_delay) {
        // Go exactly to the position for 1st character, no guessing
        uint8_t start_x = x_pos;
        uint8_t start_y = y_pos; // Actual top-left starting position
        
        // Apply inversion if configured
        if (INVERT_X) {
            start_x = DAC_MAX_VALUE - start_x;
        }
        if (INVERT_Y) {
            start_y = DAC_MAX_VALUE - start_y;
        }
        
        // Pre-position beam exactly at the starting point
        dac_oneshot_output_voltage(dac_x, start_x);
        dac_oneshot_output_voltage(dac_y, start_y);
        
        // Add delay for beam to settle without moving
        for (volatile int i = 0; i < BEAM_SETTLING_LOOPS; i++) {
            // Nothing else - just wait in place
        }
    }
    
    // Use the Hershey font library to draw the string
    hershey_draw_string(x_pos, y_pos, text, size, CHAR_SPACING, dac_draw_func, &ctx);
    
    // Remember the last position where we ended up
    prev_end_x = ctx.last_x;
    prev_end_y = ctx.last_y;
}

// WiFi event handler
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"Connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_connected = true;
    }
}

// SNTP time sync notification callback
void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized with NTP server!");
    
    // Print current time to verify timezone is applied correctly
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    ESP_LOGI(TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d",
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    // Verify timezone setting
    char *tz = getenv("TZ");
    ESP_LOGI(TAG, "Current TZ setting: %s", tz ? tz : "not set");
    
    time_synced = true;
}

// Initialize WiFi as station and connect to specified AP
void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // Wait for connection or timeout
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

// Initialize SNTP for time synchronization
void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    
    // Set automatic time sync interval to 1 hour
    const int ONE_HOUR_IN_MS = 60 * 60 * 1000;
    sntp_set_sync_interval(ONE_HOUR_IN_MS);
    
    // Register callback for time synchronization notifications
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    
    // Initialize SNTP module
    esp_sntp_init();
    
    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    
    // Wait for time sync (up to 15 seconds)
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (timeinfo.tm_year < (2023 - 1900)) {
        ESP_LOGW(TAG, "Time not synchronized yet");
    } else {
        ESP_LOGI(TAG, "Time synchronized: %04d-%02d-%02d %02d:%02d:%02d",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        time_synced = true;
    }
}

// Get hour string in format "HH:" 
void get_hour_string(char *buffer, size_t buffer_size)
{
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (time_synced) {
        // Use 24-hour format for correct time display
        strftime(buffer, buffer_size, "%H:", &timeinfo);
    } else {
        snprintf(buffer, buffer_size, "--:");
    }
}

// Get minute string in format "MM:"
void get_minute_string(char *buffer, size_t buffer_size)
{
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (time_synced) {
        strftime(buffer, buffer_size, "%M:", &timeinfo);
    } else {
        snprintf(buffer, buffer_size, "--:");
    }
}

// Get seconds string in format "SS"
void get_seconds_string(char *buffer, size_t buffer_size)
{
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (time_synced) {
        strftime(buffer, buffer_size, "%S", &timeinfo);
    } else {
        snprintf(buffer, buffer_size, "--");
    }
}

// Get current date string in "MMM DD" format
void get_date_string(char *buffer, size_t buffer_size)
{
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (time_synced) {
        strftime(buffer, buffer_size, "%b %d", &timeinfo);
    } else {
        snprintf(buffer, buffer_size, "---");
    }
}

// Draw clock with current time and date
void draw_clock(dac_oneshot_handle_t dac_x, dac_oneshot_handle_t dac_y, int iterations) {
    char hour_str[32];
    char minute_str[32];
    char seconds_str[32];
    char date_str[32];
    static char last_hour_str[32] = "";
    static char last_minute_str[32] = "";
    static char last_seconds_str[32] = "";
    static char last_date_str[32] = "";
    static int offset_x = 0;
    static int offset_y = 0;
    static int last_minute = -1;
    static int last_second = -1;
    
    // Get current time and date strings
    get_hour_string(hour_str, sizeof(hour_str));
    get_minute_string(minute_str, sizeof(minute_str));
    get_seconds_string(seconds_str, sizeof(seconds_str));
    get_date_string(date_str, sizeof(date_str));
    
    // Check if time changed - if not, just keep rendering the same data
    bool time_changed = strcmp(hour_str, last_hour_str) != 0 || 
                        strcmp(minute_str, last_minute_str) != 0 ||
                        strcmp(seconds_str, last_seconds_str) != 0 ||
                        strcmp(date_str, last_date_str) != 0;
    
    if (time_changed) {
        // Save the new time strings
        strcpy(last_hour_str, hour_str);
        strcpy(last_minute_str, minute_str);
        strcpy(last_seconds_str, seconds_str);
        strcpy(last_date_str, date_str);
        
        if (ANTI_BURNIN_ENABLED) {
            // Get current time for position shifting
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            
            bool should_shift = false;
            
            if (ANTI_BURNIN_DEBUG_MODE) {
                // In debug mode, shift every second for quick testing
                if (timeinfo.tm_sec != last_second) {
                    last_second = timeinfo.tm_sec;
                    should_shift = true;
                }
            } else {
                // Normal mode - shift every minute
                if (timeinfo.tm_min != last_minute) {
                    last_minute = timeinfo.tm_min;
                    should_shift = true;
                }
            }
            
            if (should_shift) {
                // Change the position slightly to prevent burn-in
                // Use a simple deterministic pattern to create different positions
                // We want positions to change each time, not repeat 0,0
                
                // Use current second or minute (depending on mode) as part of the seed
                int time_value = ANTI_BURNIN_DEBUG_MODE ? last_second : last_minute;
                
                // Generate x offset (positive only to prevent text going off left edge)
                // For X: values from 0 to ANTI_BURNIN_X_RANGE
                offset_x = (time_value * 3 + 7) % ANTI_BURNIN_X_RANGE;
                
                // Generate y offset (centered around 0)
                // For Y: values from -ANTI_BURNIN_Y_RANGE/2 to +ANTI_BURNIN_Y_RANGE/2
                offset_y = ((time_value * 5 + 3) % ANTI_BURNIN_Y_RANGE) - (ANTI_BURNIN_Y_RANGE / 2);
                
                ESP_LOGI(TAG, "Adjusting position to prevent burn-in: offset_x=%d, offset_y=%d", 
                        offset_x, offset_y);
            }
        }
    }
    
    // Calculate final positions with or without offsets
    int pos_offset_x = ANTI_BURNIN_ENABLED ? offset_x : 0;
    int pos_offset_y = ANTI_BURNIN_ENABLED ? offset_y : 0;
    
    // Draw multiple times for brighter display
    for (int i = 0; i < iterations; i++) {
        // Draw time with adjusted font sizes - HH:MM:SS with offsets to prevent burn-in
        // Using slightly smaller fonts (20 instead of 22) to allow more room for movement
        draw_text(dac_x, dac_y, hour_str, 10 + pos_offset_x, 160 + pos_offset_y, 20, true);     // Hours with settling delay, moved right
        draw_text(dac_x, dac_y, minute_str, 95 + pos_offset_x, 160 + pos_offset_y, 20, false);  // Minutes - no delay
        draw_text(dac_x, dac_y, seconds_str, 180 + pos_offset_x, 160 + pos_offset_y, 20, false); // Seconds - no delay
        
        // Draw date in smaller font - with settling delay for month
        draw_text(dac_x, dac_y, date_str, 35 + pos_offset_x, 60 + pos_offset_y, 18, true);      // Date moved right and down, smaller font
        
        // No task yields inside the drawing loop - constant beam motion
    }
}

// Draw clock task
void draw_clock_task(void *pvParameters)
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
    
    printf("Starting clock display on oscilloscope...\n");
    
    // Initialize the Hershey font library
    esp_err_t font_init_res = hershey_fonts_init();
    if (font_init_res != ESP_OK) {
        printf("Failed to initialize Hershey font library: %d\n", font_init_res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } else {
        printf("Hershey font library initialized successfully\n");
    }
    
    // Set global DAC handles so spinner task can access them
    global_dac_handle_x = dac_handle_x;
    global_dac_handle_y = dac_handle_y;
    
    // Spinner task already started in app_main_clock
    printf("Waiting for WiFi connection and NTP time sync...\n");
    
    // Time sync is handled by the app_main_clock function
    // Just wait until it's done
    while (!time_synced) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    // Once time is synced, stop the spinner task
    spinner_running = false;
    
    // Allow spinner to exit cleanly
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // Main drawing loop - runs forever
    printf("Starting main drawing loop...\n");
    
    // Initial time values
    time_t last_check_time = 0;
    time_t current_time;
    
    while (1) {
        // Get current time
        time(&current_time);
        
        // Check time once per second to minimize processing overhead
        if (current_time != last_check_time) {
            last_check_time = current_time;
            
            // Draw the clock continuously for a full second, no pauses
            // This keeps the beam in constant motion for maximum visibility
            for (int i = 0; i < 50; i++) {  // Increased rendering iterations
                draw_clock(dac_handle_x, dac_handle_y, 2);  // Draw twice for better visibility
                
                // Very brief yield every few iterations to prevent watchdog reset
                if (i % 10 == 0) {
                    taskYIELD();
                }
            }
        } else {
            // Just keep drawing the current time values
            draw_clock(dac_handle_x, dac_handle_y, 1);
        }
    }
}

// Function to stop the clock demo
void stop_clock_demo(void) {
    if (clock_task_handle != NULL) {
        vTaskDelete(clock_task_handle);
        clock_task_handle = NULL;
    }
}

// Main entry point for the clock demo
void app_main_clock(void)
{
    printf("Starting oscilloscope clock display...\n");
    
    // Create synchronization semaphore
    sync_semaphore = xSemaphoreCreateBinary();
    
    // Create our drawing task on Core 0 first - this will initialize DAC handles
    xTaskCreatePinnedToCore(
        draw_clock_task,
        "draw_clock_task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,  // Maximum priority
        &clock_task_handle,
        0  // Core 0
    );
    
    // Brief delay for DAC handles to initialize
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // Start the spinner task on Core 1 before WiFi
    // Use lower priority to give WiFi/NTP more CPU time
    xTaskCreatePinnedToCore(
        spinner_task,
        "spinner_task",
        4096,
        NULL,
        tskIDLE_PRIORITY + 5,  // Medium priority
        &spinner_task_handle,
        1  // Core 1
    );
    
    // Create a simple idle task on Core 1 to keep system happy
    TaskHandle_t idle_handle = NULL;
    xTaskCreatePinnedToCore(
        idle_task_clock,
        "idle_task_clock",
        2048,
        NULL,
        1,
        &idle_handle,
        1  // Core 1
    );
    
    // Now that spinner is running, initialize NVS and WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "Starting WiFi in STA mode");
    
    // Set timezone before anything else
#ifdef CONFIG_NTP_TIMEZONE
    printf("Setting timezone to: %s\n", CONFIG_NTP_TIMEZONE);
    setenv("TZ", CONFIG_NTP_TIMEZONE, 1);
#else
    printf("Setting timezone to default\n");
    setenv("TZ", "UTC0", 1);
#endif
    tzset();
    
    // Initialize WiFi after spinner is running
    wifi_init_sta();
    
    // Initialize SNTP for time synchronization
    initialize_sntp();
    
    // Since we're running directly, we don't exit
    printf("\nClock display is running...\n");
    
    // We don't exit this function, let the tasks run
    while(1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
