#pragma once
#include <math.h>
#include <stdint.h>

// Adapted from Minecraft.World/Vec3.h (4J Studios, 2014)
// Major simplification: removed TLS pool system (Thread Local Storage Windows)
// On PSP we are single-threaded - using simple stack/heap allocation

class AABB;

class Vec3 {
public:
  double x, y, z;

  Vec3() : x(0), y(0), z(0) {}
  Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

  // Factory - on PSP we simply allocate on the heap
  static Vec3 *newTemp(double x, double y, double z);
  static Vec3 *newPermanent(double x, double y, double z);

  Vec3 *set(double x, double y, double z);
  Vec3 *interpolateTo(Vec3 *t, double p);
  Vec3 *vectorTo(Vec3 *p);
  Vec3 *normalize();
  double dot(Vec3 *p);
  Vec3 *cross(Vec3 *p);
  Vec3 *add(double x, double y, double z);
  double distanceTo(Vec3 *p);
  double distanceToSqr(Vec3 *p);
  double distanceToSqr(double x2, double y2, double z2);
  Vec3 *scale(double l);
  double length();
  Vec3 *lerp(Vec3 *v, double a);
  void xRot(float degs);
  void yRot(float degs);
  void zRot(float degs);
  double distanceTo(AABB *box);
};
