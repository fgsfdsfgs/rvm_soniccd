//
//  DrawVertex.h
//  rvm
//

#ifndef DrawVertex_h
#define DrawVertex_h
#include "SDL.h"

// VITA: vgl*Pointer functions can't take float2, so we go with float3

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

struct DrawVertex {
    struct Vector3 position;
    struct Vector3 texCoord;
    struct Color color;
};

#define VTX2D_STRIDE sizeof(struct DrawVertex)
#define VTX2D_COUNT 3

#endif /* DrawVertex_h */
