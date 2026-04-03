// AABB.cpp

#include "AABB.h"
#include "Vec3.h"
#include <math.h>
#include <stdlib.h>

AABB *AABB::newPermanent(double x0, double y0, double z0, double x1, double y1,
                         double z1) {
  return new AABB(x0, y0, z0, x1, y1, z1);
}

AABB *AABB::newTemp(double x0, double y0, double z0, double x1, double y1,
                    double z1) {
  return new AABB(x0, y0, z0, x1, y1, z1);
}

AABB *AABB::set(double nx0, double ny0, double nz0, double nx1, double ny1,
                double nz1) {
  x0 = nx0;
  y0 = ny0;
  z0 = nz0;
  x1 = nx1;
  y1 = ny1;
  z1 = nz1;
  return this;
}

void AABB::set(AABB *b) {
  x0 = b->x0;
  y0 = b->y0;
  z0 = b->z0;
  x1 = b->x1;
  y1 = b->y1;
  z1 = b->z1;
}

AABB *AABB::expand(double xa, double ya, double za) {
  double nx0 = x0, ny0 = y0, nz0 = z0;
  double nx1 = x1, ny1 = y1, nz1 = z1;
  if (xa < 0)
    nx0 += xa;
  else
    nx1 += xa;
  if (ya < 0)
    ny0 += ya;
  else
    ny1 += ya;
  if (za < 0)
    nz0 += za;
  else
    nz1 += za;
  return AABB::newTemp(nx0, ny0, nz0, nx1, ny1, nz1);
}

AABB *AABB::grow(double xa, double ya, double za) {
  return AABB::newTemp(x0 - xa, y0 - ya, z0 - za, x1 + xa, y1 + ya, z1 + za);
}

AABB *AABB::shrink(double xa, double ya, double za) {
  return AABB::newTemp(x0 + xa, y0 + ya, z0 + za, x1 - xa, y1 - ya, z1 - za);
}

AABB *AABB::cloneMove(double xa, double ya, double za) {
  return AABB::newTemp(x0 + xa, y0 + ya, z0 + za, x1 + xa, y1 + ya, z1 + za);
}

AABB *AABB::move(double xa, double ya, double za) {
  x0 += xa;
  y0 += ya;
  z0 += za;
  x1 += xa;
  y1 += ya;
  z1 += za;
  return this;
}

AABB *AABB::copy() { return AABB::newTemp(x0, y0, z0, x1, y1, z1); }

double AABB::clipXCollide(AABB *c, double xa) {
  if (c->y1 <= y0 || c->y0 >= y1)
    return xa;
  if (c->z1 <= z0 || c->z0 >= z1)
    return xa;
  if (xa > 0 && c->x1 <= x0) {
    double max = x0 - c->x1;
    if (max < xa)
      xa = max;
  }
  if (xa < 0 && c->x0 >= x1) {
    double max = x1 - c->x0;
    if (max > xa)
      xa = max;
  }
  return xa;
}

double AABB::clipYCollide(AABB *c, double ya) {
  if (c->x1 <= x0 || c->x0 >= x1)
    return ya;
  if (c->z1 <= z0 || c->z0 >= z1)
    return ya;
  if (ya > 0 && c->y1 <= y0) {
    double max = y0 - c->y1;
    if (max < ya)
      ya = max;
  }
  if (ya < 0 && c->y0 >= y1) {
    double max = y1 - c->y0;
    if (max > ya)
      ya = max;
  }
  return ya;
}

double AABB::clipZCollide(AABB *c, double za) {
  if (c->x1 <= x0 || c->x0 >= x1)
    return za;
  if (c->y1 <= y0 || c->y0 >= y1)
    return za;
  if (za > 0 && c->z1 <= z0) {
    double max = z0 - c->z1;
    if (max < za)
      za = max;
  }
  if (za < 0 && c->z0 >= z1) {
    double max = z1 - c->z0;
    if (max > za)
      za = max;
  }
  return za;
}

bool AABB::intersects(AABB *c) {
  return c->x1 > x0 && c->x0 < x1 && c->y1 > y0 && c->y0 < y1 && c->z1 > z0 &&
         c->z0 < z1;
}

bool AABB::intersectsInner(AABB *c) {
  return c->x1 >= x0 && c->x0 <= x1 && c->y1 >= y0 && c->y0 <= y1 &&
         c->z1 >= z0 && c->z0 <= z1;
}

bool AABB::intersects(double x02, double y02, double z02, double x12,
                      double y12, double z12) {
  return x12 > x0 && x02 < x1 && y12 > y0 && y02 < y1 && z12 > z0 && z02 < z1;
}

bool AABB::contains(Vec3 *p) {
  return p->x > x0 && p->x < x1 && p->y > y0 && p->y < y1 && p->z > z0 &&
         p->z < z1;
}

bool AABB::containsIncludingLowerBound(Vec3 *p) {
  return p->x >= x0 && p->x < x1 && p->y >= y0 && p->y < y1 && p->z >= z0 &&
         p->z < z1;
}

bool AABB::containsX(Vec3 *v) { return v->x > x0 && v->x < x1; }
bool AABB::containsY(Vec3 *v) { return v->y > y0 && v->y < y1; }
bool AABB::containsZ(Vec3 *v) { return v->z > z0 && v->z < z1; }

double AABB::getSize() {
  double dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
  return (dx + dy + dz) / 3.0;
}

// HitResult (stub - to be implemented in the next iteration with raycasting)
HitResult *AABB::clip(Vec3 *a, Vec3 *b) { return nullptr; }
