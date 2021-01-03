//
//  DrawVertex.h
//  rvm
//

#ifndef DrawVertex_h
#define DrawVertex_h
#include "SDL.h"

// VITA: vgl*Pointer functions can't take float2, so we go with float3 for position

struct Vector2 {
    float X;
    float Y;
};

struct Vector3 {
    float X;
    float Y;
    float Z;
};

struct Color {
    uint8_t R;
    uint8_t G;
    uint8_t B;
    uint8_t A;
};

// VITA: use separate gpu-mapped buffers for each vertex attrib, eliminating stride
struct DrawBuffer {
    struct Vector3 *position;
    struct Vector2 *texCoord;
    struct Color *color;
};

#endif /* DrawVertex_h */
