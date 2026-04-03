#include "Frustum.h"

Frustum::Frustum() {}
Frustum::~Frustum() {}

void Frustum::update(ScePspFMatrix4& clip) {
    Plane *pPlane = nullptr;

    // Near clipping plane
    pPlane = &planes[FRUSTUM_PLANE_NEAR];
    pPlane->set(clip.x.w + clip.x.z, clip.y.w + clip.y.z, clip.z.w + clip.z.z, clip.w.w + clip.w.z);
    pPlane->normalize();

    // Far clipping plane
    pPlane = &planes[FRUSTUM_PLANE_FAR];
    pPlane->set(clip.x.w - clip.x.z, clip.y.w - clip.y.z, clip.z.w - clip.z.z, clip.w.w - clip.w.z);
    pPlane->normalize();

    // Left clipping plane
    pPlane = &planes[FRUSTUM_PLANE_LEFT];
    pPlane->set(clip.x.w + clip.x.x, clip.y.w + clip.y.x, clip.z.w + clip.z.x, clip.w.w + clip.w.x);
    pPlane->normalize();

    // Right clipping plane
    pPlane = &planes[FRUSTUM_PLANE_RIGHT];
    pPlane->set(clip.x.w - clip.x.x, clip.y.w - clip.y.x, clip.z.w - clip.z.x, clip.w.w - clip.w.x);
    pPlane->normalize();

    // Bottom clipping plane
    pPlane = &planes[FRUSTUM_PLANE_BOTTOM];
    pPlane->set(clip.x.w + clip.x.y, clip.y.w + clip.y.y, clip.z.w + clip.z.y, clip.w.w + clip.w.y);
    pPlane->normalize();

    // Top clipping plane
    pPlane = &planes[FRUSTUM_PLANE_TOP];
    pPlane->set(clip.x.w - clip.x.y, clip.y.w - clip.y.y, clip.z.w - clip.z.y, clip.w.w - clip.w.y);
    pPlane->normalize();
}

Frustum::Intersection Frustum::testAABB(const AABB& box) {
    float x[2] = {(float)box.x0, (float)box.x1};
    float y[2] = {(float)box.y0, (float)box.y1};
    float z[2] = {(float)box.z0, (float)box.z1};

    for (int i = 0; i < 6; ++i) {
        int inCount = 0;
        // Test all 8 corners of the AABB
        for (int a = 0; a < 2; ++a) {
            for (int b = 0; b < 2; ++b) {
                for (int c = 0; c < 2; ++c) {
                    // If at least one point is in front (or very close behind)
                    float dist = planes[i].a * x[a] + planes[i].b * y[b] + planes[i].c * z[c] + planes[i].d;
                    if (dist >= 0.0f) { // Standard frustum test: inside if dist >= 0
                        inCount++;
                    }
                }
            }
        }
        
        // Frustum occlusion cull if all 8 points are outside.
        if (inCount == 0) {
            return OUTSIDE;
        }
    }
    return INTERSECTS;
}
