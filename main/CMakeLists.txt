idf_component_register(SRCS "main.c" "../examples/daisy.c" "../examples/grayscale.c" "../examples/timebase_image.c" "../examples/animation.c" "../examples/clock.c" "../examples/fractal_maze.c"
                       INCLUDE_DIRS "." "../examples" "../processed"
                       REQUIRES hershey_fonts esp_rom esp_wifi nvs_flash esp_event)