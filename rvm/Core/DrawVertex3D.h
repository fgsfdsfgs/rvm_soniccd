//
//  DrawVertex3D.h
//  rvm
//

#ifndef DrawVertex3D_h
#define DrawVertex3D_h

#include "DrawVertex.h"

// VITA: Vector3 already defined in DrawVertex.h

struct DrawVertex3D {
    struct Vector3 position;
    struct Vector3 texCoord;
    struct Color color;
};

#define VTX3D_STRIDE sizeof(struct DrawVertex3D)
#define VTX3D_COUNT 3

#endif /* DrawVertex3D_h */
