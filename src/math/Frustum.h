#pragma once

#include <pspgu.h>
#include <pspgum.h>
#include <math.h>

#include "../world/AABB.h"

struct Plane {
    float a, b, c, d;

    void set(float _a, float _b, float _c, float _d) {
        a = _a; b = _b; c = _c; d = _d;
    }

    void normalize() {
        float magSq = a * a + b * b + c * c;
        if (magSq > 0.0f) {
            float invMag = 1.0f / sqrtf(magSq);
            a *= invMag;
            b *= invMag;
            c *= invMag;
            d *= invMag;
        }
    }
};

class Frustum {
public:
    enum Intersection {
        OUTSIDE = 0,
        INSIDE = 1,
        INTERSECTS = 2
    };

    enum {
        FRUSTUM_PLANE_LEFT   = 0,
        FRUSTUM_PLANE_RIGHT  = 1,
        FRUSTUM_PLANE_BOTTOM = 2,
        FRUSTUM_PLANE_TOP    = 3,
        FRUSTUM_PLANE_NEAR   = 4,
        FRUSTUM_PLANE_FAR    = 5
    };

    Frustum();
    ~Frustum();

    Plane planes[6];

    void update(ScePspFMatrix4& clip);
    Intersection testAABB(const AABB& box);
};
