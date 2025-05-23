#ifndef _ANIMATION_H_
#define _ANIMATION_H_

/* 
 * Animation header that includes the generated animation data from processed directory
 * The file is generated when you process animations with the scripts in the scripts directory
 */

// Try to include animation data if it exists, otherwise use fallback empty definitions
#if __has_include("../processed/animation.h")
    #include "../processed/animation.h"
#else
    // Fallback definitions when animation data hasn't been generated yet
    #define ANIMATION_FRAMES 1
    #define ANIMATION_WIDTH 10
    #define ANIMATION_HEIGHT 10
    #define ANIMATION_FPS 10
    #define NUM_FRAMES ANIMATION_FRAMES
    #define FRAME_INTENSITY_LEVELS 256
    
    // Empty animation data - just a single dot in the middle
    static const uint8_t animation_data[] = {128, 128, 0};
    
    // Create a compatible animation_frames structure
    static const uint8_t frame0[ANIMATION_HEIGHT][ANIMATION_WIDTH] = {{0}};
    static const uint8_t* animation_frames[ANIMATION_FRAMES][ANIMATION_HEIGHT] = {{
        frame0[0], frame0[1], frame0[2], frame0[3], frame0[4], 
        frame0[5], frame0[6], frame0[7], frame0[8], frame0[9]
    }};
#endif

#endif // _ANIMATION_H_