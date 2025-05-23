#ifndef DVD_LOGO_POINTS_H
#define DVD_LOGO_POINTS_H

#include <stdbool.h>
#include <stdint.h>

// Scale factor for all paths
#define DVD_LOGO_SCALE 255.0f



// Total number of paths and points
#define DVD_LOGO_PATH_COUNT 3

#define DVD_LOGO_TOTAL_POINT_COUNT 89


// Individual path data - separate array for each path
// Path segment 1 with 54 points
const float dvd_logo_path_1[] = {
    0.086041f, 0.279921f, 0.073078f, 0.334448f, 0.198529f, 0.334568f, 0.241125f, 0.341079f, 
    0.264412f, 0.352523f, 0.277206f, 0.366138f, 0.283268f, 0.383295f, 0.281922f, 0.403839f, 
    0.264538f, 0.436589f, 0.233538f, 0.458566f, 0.193312f, 0.470464f, 0.136772f, 0.473194f, 
    0.164917f, 0.354658f, 0.068284f, 0.354658f, 0.027162f, 0.527761f, 0.201708f, 0.525386f, 
    0.255263f, 0.513594f, 0.303102f, 0.493224f, 0.349707f, 0.458810f, 0.376699f, 0.420541f, 
    0.384317f, 0.385888f, 0.377394f, 0.333313f, 0.468795f, 0.585169f, 0.690814f, 0.334576f, 
    0.813673f, 0.334575f, 0.856342f, 0.341086f, 0.879720f, 0.352529f, 0.892590f, 0.366144f, 
    0.898711f, 0.383302f, 0.897391f, 0.403846f, 0.879952f, 0.436596f, 0.848836f, 0.458573f, 
    0.808502f, 0.470470f, 0.751918f, 0.473200f, 0.780074f, 0.354646f, 0.683441f, 0.354646f, 
    0.642307f, 0.527758f, 0.804486f, 0.526695f, 0.881803f, 0.509793f, 0.928101f, 0.487742f, 
    0.961591f, 0.462377f, 0.988017f, 0.428669f, 0.999236f, 0.395383f, 0.997161f, 0.363432f, 
    0.980152f, 0.332004f, 0.949674f, 0.306747f, 0.913014f, 0.290643f, 0.850776f, 0.280183f, 
    0.633784f, 0.279915f, 0.542225f, 0.389726f, 0.504411f, 0.443077f, 0.492791f, 0.394468f, 
    0.453829f, 0.279914f, 0.086041f, 0.279921f, 
};
#define DVD_LOGO_PATH_1_POINT_COUNT 54


// Path segment 2 with 22 points
const float dvd_logo_path_2[] = {
    0.472281f, 0.586215f, 0.212862f, 0.597209f, 0.103757f, 0.611287f, 0.035665f, 0.627589f, 
    0.002341f, 0.646445f, 0.000263f, 0.655405f, 0.009214f, 0.666372f, 0.054797f, 0.684472f, 
    0.133139f, 0.699732f, 0.238213f, 0.711299f, 0.363994f, 0.718318f, 0.638641f, 0.715815f, 
    0.755963f, 0.706669f, 0.858632f, 0.691656f, 0.919519f, 0.674712f, 0.938129f, 0.664223f, 
    0.944299f, 0.655405f, 0.938129f, 0.642078f, 0.914427f, 0.629573f, 0.811423f, 0.606569f, 
    0.652645f, 0.591270f, 0.472281f, 0.586215f, 
};
#define DVD_LOGO_PATH_2_POINT_COUNT 22


// Path segment 3 with 13 points
const float dvd_logo_path_3[] = {
    0.455184f, 0.632162f, 0.535810f, 0.639656f, 0.557931f, 0.647678f, 0.562812f, 0.655884f, 
    0.539901f, 0.668280f, 0.488032f, 0.675731f, 0.428633f, 0.676103f, 0.374563f, 0.669291f, 
    0.348185f, 0.657272f, 0.348185f, 0.651675f, 0.357123f, 0.645178f, 0.398927f, 0.635435f, 
    0.455184f, 0.632162f, 
};
#define DVD_LOGO_PATH_3_POINT_COUNT 13



// Array of pointers to each path segment
const float* dvd_logo_paths[] = {
    dvd_logo_path_1,
    dvd_logo_path_2,
    dvd_logo_path_3,
};



// Array of point counts for each path segment
const uint16_t dvd_logo_path_lengths[] = {
    DVD_LOGO_PATH_1_POINT_COUNT,
    DVD_LOGO_PATH_2_POINT_COUNT,
    DVD_LOGO_PATH_3_POINT_COUNT,
};


#endif // DVD_LOGO_POINTS_H
