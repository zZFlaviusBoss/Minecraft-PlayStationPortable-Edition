#pragma once
#include <stdint.h>

class Vec3;
class HitResult;

// Adapted from Minecraft.World/AABB.h
// Simplified: removed TLS thread pool from Windows

class AABB {
public:
  double x0, y0, z0;
  double x1, y1, z1;

  AABB() : x0(0), y0(0), z0(0), x1(0), y1(0), z1(0) {}
  AABB(double x0, double y0, double z0, double x1, double y1, double z1)
      : x0(x0), y0(y0), z0(z0), x1(x1), y1(y1), z1(z1) {}

  // Same interface as the original, without TLS
  static AABB *newPermanent(double x0, double y0, double z0, double x1,
                            double y1, double z1);
  static AABB *newTemp(double x0, double y0, double z0, double x1, double y1,
                       double z1);

  AABB *set(double x0, double y0, double z0, double x1, double y1, double z1);
  void set(AABB *b);
  AABB *expand(double xa, double ya, double za);
  AABB *grow(double xa, double ya, double za);
  AABB *shrink(double xa, double ya, double za);
  AABB *cloneMove(double xa, double ya, double za);
  AABB *move(double xa, double ya, double za);
  AABB *copy();

  double clipXCollide(AABB *c, double xa);
  double clipYCollide(AABB *c, double ya);
  double clipZCollide(AABB *c, double za);

  bool intersects(AABB *c);
  bool intersectsInner(AABB *c);
  bool intersects(double x02, double y02, double z02, double x12, double y12,
                  double z12);

  bool contains(Vec3 *p);
  bool containsIncludingLowerBound(Vec3 *p);
  bool containsX(Vec3 *v);
  bool containsY(Vec3 *v);
  bool containsZ(Vec3 *v);

  double getSize();

  HitResult *clip(Vec3 *a, Vec3 *b);
};
