#include <stdio.h>
#include "esp_system.h"
#include "sdkconfig.h"

// Function declarations for demos
extern void app_main_daisy(void);
extern void app_main_grayscale(void);
extern void app_main_timebase(void);
extern void app_main_animation(void);
extern void app_main_clock(void);
extern void app_main_fractal_maze(void);
extern void app_main_dvd_bounce(void);

void app_main(void)
{
    printf("Starting ESP32 Oscilloscope Demo\n");
    
#if defined(CONFIG_DEMO_DAISY)
    printf("Running Daisy Flower demo\n");
    app_main_daisy();
#elif defined(CONFIG_DEMO_GRAYSCALE)
    printf("Running Grayscale Image demo\n");
    app_main_grayscale();
#elif defined(CONFIG_DEMO_TIMEBASE)
    printf("Running Timebase Image demo\n");
    app_main_timebase();
#elif defined(CONFIG_DEMO_ANIMATION)
    printf("Running Animation demo\n");
    app_main_animation();
#elif defined(CONFIG_DEMO_CLOCK)
    printf("Running Clock demo\n");
    app_main_clock();
#elif defined(CONFIG_DEMO_FRACTAL_MAZE)
    printf("Running Fractal Maze demo\n");
    app_main_fractal_maze();
#elif defined(CONFIG_DEMO_DVD_BOUNCE)
    printf("Running DVD Logo Bounce demo\n");
    app_main_dvd_bounce();
#else
    // Default to timebase demo
    printf("No demo selected. Running Timebase Image demo by default\n");
    app_main_timebase();
#endif
}