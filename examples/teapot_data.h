#ifndef TEAPOT_DATA_H
#define TEAPOT_DATA_H

// A simplified version of the Utah teapot
// Reduced number of vertices and edges for better performance on the ESP32

// Number of vertices and edges in the teapot model
#define NUM_VERTICES 128
#define NUM_EDGES 221

// Teapot vertices (simplified version of the Utah teapot)
// Each vertex is represented as [x, y, z] coordinates
const float teapot_vertices[NUM_VERTICES][3] = {
    // Body vertices
    {0.0f, 0.0f, 0.0f},      // 0: Center bottom
    {0.0f, 0.2f, 0.0f},      // 1: Center mid-bottom
    {0.0f, 0.4f, 0.0f},      // 2: Center mid
    {0.0f, 0.6f, 0.0f},      // 3: Center mid-top
    {0.0f, 0.8f, 0.0f},      // 4: Center top
    
    // Bottom ring - 8 vertices
    {0.5f, 0.0f, 0.0f},      // 5
    {0.35f, 0.0f, 0.35f},    // 6
    {0.0f, 0.0f, 0.5f},      // 7
    {-0.35f, 0.0f, 0.35f},   // 8
    {-0.5f, 0.0f, 0.0f},     // 9
    {-0.35f, 0.0f, -0.35f},  // 10
    {0.0f, 0.0f, -0.5f},     // 11
    {0.35f, 0.0f, -0.35f},   // 12
    
    // Lower middle ring - 8 vertices
    {0.65f, 0.2f, 0.0f},     // 13
    {0.46f, 0.2f, 0.46f},    // 14
    {0.0f, 0.2f, 0.65f},     // 15
    {-0.46f, 0.2f, 0.46f},   // 16
    {-0.65f, 0.2f, 0.0f},    // 17
    {-0.46f, 0.2f, -0.46f},  // 18
    {0.0f, 0.2f, -0.65f},    // 19
    {0.46f, 0.2f, -0.46f},   // 20
    
    // Middle ring - 8 vertices
    {0.8f, 0.4f, 0.0f},      // 21
    {0.57f, 0.4f, 0.57f},    // 22
    {0.0f, 0.4f, 0.8f},      // 23
    {-0.57f, 0.4f, 0.57f},   // 24
    {-0.8f, 0.4f, 0.0f},     // 25
    {-0.57f, 0.4f, -0.57f},  // 26
    {0.0f, 0.4f, -0.8f},     // 27
    {0.57f, 0.4f, -0.57f},   // 28
    
    // Upper middle ring - 8 vertices
    {0.65f, 0.6f, 0.0f},     // 29
    {0.46f, 0.6f, 0.46f},    // 30
    {0.0f, 0.6f, 0.65f},     // 31
    {-0.46f, 0.6f, 0.46f},   // 32
    {-0.65f, 0.6f, 0.0f},    // 33
    {-0.46f, 0.6f, -0.46f},  // 34
    {0.0f, 0.6f, -0.65f},    // 35
    {0.46f, 0.6f, -0.46f},   // 36
    
    // Top ring - 8 vertices
    {0.5f, 0.8f, 0.0f},      // 37
    {0.35f, 0.8f, 0.35f},    // 38
    {0.0f, 0.8f, 0.5f},      // 39
    {-0.35f, 0.8f, 0.35f},   // 40
    {-0.5f, 0.8f, 0.0f},     // 41
    {-0.35f, 0.8f, -0.35f},  // 42
    {0.0f, 0.8f, -0.5f},     // 43
    {0.35f, 0.8f, -0.35f},   // 44
    
    // Lid - center and rings
    {0.0f, 0.9f, 0.0f},      // 45: Lid center
    
    // Lid lower ring - 8 vertices
    {0.3f, 0.82f, 0.0f},     // 46
    {0.21f, 0.82f, 0.21f},   // 47
    {0.0f, 0.82f, 0.3f},     // 48
    {-0.21f, 0.82f, 0.21f},  // 49
    {-0.3f, 0.82f, 0.0f},    // 50
    {-0.21f, 0.82f, -0.21f}, // 51
    {0.0f, 0.82f, -0.3f},    // 52
    {0.21f, 0.82f, -0.21f},  // 53
    
    // Lid upper ring - 8 vertices
    {0.2f, 0.9f, 0.0f},      // 54
    {0.14f, 0.9f, 0.14f},    // 55
    {0.0f, 0.9f, 0.2f},      // 56
    {-0.14f, 0.9f, 0.14f},   // 57
    {-0.2f, 0.9f, 0.0f},     // 58
    {-0.14f, 0.9f, -0.14f},  // 59
    {0.0f, 0.9f, -0.2f},     // 60
    {0.14f, 0.9f, -0.14f},   // 61
    
    // Lid top point
    {0.0f, 1.0f, 0.0f},      // 62: Lid top
    
    // Spout base
    {-0.5f, 0.4f, 0.0f},     // 63: Spout base center
    
    // Spout bottom ring - 6 vertices
    {-0.6f, 0.4f, 0.1f},     // 64
    {-0.6f, 0.4f, 0.0f},     // 65
    {-0.6f, 0.4f, -0.1f},    // 66
    {-0.7f, 0.4f, -0.05f},   // 67
    {-0.7f, 0.4f, 0.0f},     // 68
    {-0.7f, 0.4f, 0.05f},    // 69
    
    // Spout middle vertices - 6 vertices
    {-0.8f, 0.5f, 0.05f},    // 70
    {-0.8f, 0.5f, 0.0f},     // 71
    {-0.8f, 0.5f, -0.05f},   // 72
    {-0.9f, 0.6f, -0.025f},  // 73
    {-0.9f, 0.6f, 0.0f},     // 74
    {-0.9f, 0.6f, 0.025f},   // 75
    
    // Spout tip - 1 vertex
    {-1.0f, 0.7f, 0.0f},     // 76: Spout tip
    
    // Handle base points
    {0.4f, 0.4f, 0.0f},      // 77: Right side handle base
    {0.5f, 0.4f, 0.0f},      // 78
    
    // Handle bottom curve - 6 vertices
    {0.6f, 0.42f, 0.0f},     // 79
    {0.7f, 0.45f, 0.0f},     // 80
    {0.8f, 0.5f, 0.0f},      // 81
    {0.85f, 0.55f, 0.0f},    // 82
    {0.9f, 0.6f, 0.0f},      // 83
    {0.92f, 0.65f, 0.0f},    // 84
    
    // Handle top points - 6 vertices
    {0.93f, 0.7f, 0.0f},     // 85
    {0.92f, 0.75f, 0.0f},    // 86
    {0.9f, 0.8f, 0.0f},      // 87
    {0.85f, 0.85f, 0.0f},    // 88
    {0.75f, 0.86f, 0.0f},    // 89
    {0.65f, 0.86f, 0.0f},    // 90
    
    // Handle connection to body
    {0.5f, 0.85f, 0.0f},     // 91
    {0.4f, 0.8f, 0.0f},      // 92
    
    // Body vertical lines - 8 sets of 5 points each to create vertical resolution
    // Vertical 1
    {0.5f, 0.1f, 0.0f},      // 93
    {0.6f, 0.3f, 0.0f},      // 94
    {0.7f, 0.5f, 0.0f},      // 95
    {0.6f, 0.7f, 0.0f},      // 96
    {0.45f, 0.85f, 0.0f},    // 97
    
    // Vertical 2
    {0.35f, 0.1f, 0.35f},    // 98
    {0.42f, 0.3f, 0.42f},    // 99
    {0.5f, 0.5f, 0.5f},      // 100
    {0.42f, 0.7f, 0.42f},    // 101
    {0.3f, 0.85f, 0.3f},     // 102
    
    // Vertical 3
    {0.0f, 0.1f, 0.5f},      // 103
    {0.0f, 0.3f, 0.6f},      // 104
    {0.0f, 0.5f, 0.7f},      // 105
    {0.0f, 0.7f, 0.6f},      // 106
    {0.0f, 0.85f, 0.45f},    // 107
    
    // Vertical 4
    {-0.35f, 0.1f, 0.35f},   // 108
    {-0.42f, 0.3f, 0.42f},   // 109
    {-0.5f, 0.5f, 0.5f},     // 110
    {-0.42f, 0.7f, 0.42f},   // 111
    {-0.3f, 0.85f, 0.3f},    // 112
    
    // Vertical 5
    {-0.5f, 0.1f, 0.0f},     // 113
    {-0.6f, 0.3f, 0.0f},     // 114
    {-0.7f, 0.5f, 0.0f},     // 115
    {-0.6f, 0.7f, 0.0f},     // 116
    {-0.45f, 0.85f, 0.0f},   // 117
    
    // Vertical 6
    {-0.35f, 0.1f, -0.35f},  // 118
    {-0.42f, 0.3f, -0.42f},  // 119
    {-0.5f, 0.5f, -0.5f},    // 120
    {-0.42f, 0.7f, -0.42f},  // 121
    {-0.3f, 0.85f, -0.3f},   // 122
    
    // Vertical 7
    {0.0f, 0.1f, -0.5f},     // 123
    {0.0f, 0.3f, -0.6f},     // 124
    {0.0f, 0.5f, -0.7f},     // 125
    {0.0f, 0.7f, -0.6f},     // 126
    {0.0f, 0.85f, -0.45f},   // 127
};

// Teapot edges (connections between vertices)
// Each edge is defined by the indices of its two vertices
const int teapot_edges[NUM_EDGES][2] = {
    // Bottom ring connections
    {5, 6}, {6, 7}, {7, 8}, {8, 9}, {9, 10}, {10, 11}, {11, 12}, {12, 5},
    
    // Lower middle ring connections
    {13, 14}, {14, 15}, {15, 16}, {16, 17}, {17, 18}, {18, 19}, {19, 20}, {20, 13},
    
    // Middle ring connections
    {21, 22}, {22, 23}, {23, 24}, {24, 25}, {25, 26}, {26, 27}, {27, 28}, {28, 21},
    
    // Upper middle ring connections
    {29, 30}, {30, 31}, {31, 32}, {32, 33}, {33, 34}, {34, 35}, {35, 36}, {36, 29},
    
    // Top ring connections
    {37, 38}, {38, 39}, {39, 40}, {40, 41}, {41, 42}, {42, 43}, {43, 44}, {44, 37},
    
    // Lid lower ring connections
    {46, 47}, {47, 48}, {48, 49}, {49, 50}, {50, 51}, {51, 52}, {52, 53}, {53, 46},
    
    // Lid upper ring connections
    {54, 55}, {55, 56}, {56, 57}, {57, 58}, {58, 59}, {59, 60}, {60, 61}, {61, 54},
    
    // Connect bottom ring to lower middle ring
    {5, 13}, {6, 14}, {7, 15}, {8, 16}, {9, 17}, {10, 18}, {11, 19}, {12, 20},
    
    // Connect lower middle ring to middle ring
    {13, 21}, {14, 22}, {15, 23}, {16, 24}, {17, 25}, {18, 26}, {19, 27}, {20, 28},
    
    // Connect middle ring to upper middle ring
    {21, 29}, {22, 30}, {23, 31}, {24, 32}, {25, 33}, {26, 34}, {27, 35}, {28, 36},
    
    // Connect upper middle ring to top ring
    {29, 37}, {30, 38}, {31, 39}, {32, 40}, {33, 41}, {34, 42}, {35, 43}, {36, 44},
    
    // Connect top ring to lid lower ring
    {37, 46}, {38, 47}, {39, 48}, {40, 49}, {41, 50}, {42, 51}, {43, 52}, {44, 53},
    
    // Connect lid lower ring to lid upper ring
    {46, 54}, {47, 55}, {48, 56}, {49, 57}, {50, 58}, {51, 59}, {52, 60}, {53, 61},
    
    // Connect lid upper ring to lid top
    {54, 62}, {55, 62}, {56, 62}, {57, 62}, {58, 62}, {59, 62}, {60, 62}, {61, 62},
    
    // Spout connections
    {25, 63}, {63, 65}, {65, 68}, {68, 71}, {71, 74}, {74, 76},  // Center line
    {64, 65}, {65, 66}, {67, 68}, {68, 69}, {70, 71}, {71, 72}, {73, 74}, {74, 75},  // Rings
    {64, 69}, {66, 67}, {70, 75}, {72, 73},  // Additional connections
    {64, 70}, {66, 72}, {69, 75}, {67, 73},  // Vertical connections
    
    // Handle connections
    {21, 77}, {77, 78}, {78, 79}, {79, 80}, {80, 81}, {81, 82}, {82, 83}, {83, 84},
    {84, 85}, {85, 86}, {86, 87}, {87, 88}, {88, 89}, {89, 90}, {90, 91}, {91, 92}, {92, 37},
    
    // Vertical lines along body (for better shape definition)
    {5, 93}, {93, 94}, {94, 95}, {95, 96}, {96, 97}, {97, 37},   // Vertical 1
    {6, 98}, {98, 99}, {99, 100}, {100, 101}, {101, 102}, {102, 38},  // Vertical 2
    {7, 103}, {103, 104}, {104, 105}, {105, 106}, {106, 107}, {107, 39},  // Vertical 3
    {8, 108}, {108, 109}, {109, 110}, {110, 111}, {111, 112}, {112, 40},  // Vertical 4
    {9, 113}, {113, 114}, {114, 115}, {115, 116}, {116, 117}, {117, 41},  // Vertical 5
    {10, 118}, {118, 119}, {119, 120}, {120, 121}, {121, 122}, {122, 42},  // Vertical 6
    {11, 123}, {123, 124}, {124, 125}, {125, 126}, {126, 127}, {127, 43},  // Vertical 7
    
    // Center vertical lines
    {0, 1}, {1, 2}, {2, 3}, {3, 4},  // Center vertical line
    
    // Connect center points to rings for stability
    {0, 5}, {0, 7}, {0, 9}, {0, 11},  // Connect center bottom to bottom ring
    {1, 13}, {1, 15}, {1, 17}, {1, 19},  // Connect center mid-bottom to lower middle ring
    {2, 21}, {2, 23}, {2, 25}, {2, 27},  // Connect center mid to middle ring
    {3, 29}, {3, 31}, {3, 33}, {3, 35},  // Connect center mid-top to upper middle ring
    {4, 37}, {4, 39}, {4, 41}, {4, 43},  // Connect center top to top ring
    {45, 54}, {45, 56}, {45, 58}, {45, 60},  // Connect lid center to lid ring
};

#endif // TEAPOT_DATA_H