// Adapted from Minecraft.World/Vec3.cpp (4J Studios, 2014)
// Major modifications: removed TLS/thread pool (single-threaded on PSP),
//                    removed wstring/wprintf, removed DWORD TLS API

#include "Vec3.h"
#include "AABB.h"
#include <math.h>
#include <stdio.h>

Vec3 *Vec3::newPermanent(double x, double y, double z) {
  return new Vec3(x, y, z);
}

Vec3 *Vec3::newTemp(double x, double y, double z) {
  // On single-threaded PSP: newTemp is identical to newPermanent.
  // WARNING: caller must delete! In adapted code we will avoid
  // newTemp() calls in hot loops - we will use Vec3 on the stack.
  return new Vec3(x, y, z);
}

Vec3 *Vec3::set(double nx, double ny, double nz) {
  x = nx;
  y = ny;
  z = nz;
  return this;
}

Vec3 *Vec3::interpolateTo(Vec3 *t, double p) {
  return Vec3::newTemp(x + (t->x - x) * p, y + (t->y - y) * p,
                       z + (t->z - z) * p);
}

Vec3 *Vec3::vectorTo(Vec3 *p) {
  return Vec3::newTemp(p->x - x, p->y - y, p->z - z);
}

Vec3 *Vec3::normalize() {
  double dist = sqrt(x * x + y * y + z * z);
  if (dist < 0.0001)
    return Vec3::newTemp(0, 0, 0);
  return Vec3::newTemp(x / dist, y / dist, z / dist);
}

double Vec3::dot(Vec3 *p) { return x * p->x + y * p->y + z * p->z; }

Vec3 *Vec3::cross(Vec3 *p) {
  return Vec3::newTemp(y * p->z - z * p->y, z * p->x - x * p->z,
                       x * p->y - y * p->x);
}

Vec3 *Vec3::add(double dx, double dy, double dz) {
  return Vec3::newTemp(x + dx, y + dy, z + dz);
}

double Vec3::distanceTo(Vec3 *p) {
  double xd = p->x - x, yd = p->y - y, zd = p->z - z;
  return sqrt(xd * xd + yd * yd + zd * zd);
}

double Vec3::distanceToSqr(Vec3 *p) {
  double xd = p->x - x, yd = p->y - y, zd = p->z - z;
  return xd * xd + yd * yd + zd * zd;
}

double Vec3::distanceToSqr(double x2, double y2, double z2) {
  double xd = x2 - x, yd = y2 - y, zd = z2 - z;
  return xd * xd + yd * yd + zd * zd;
}

Vec3 *Vec3::scale(double l) { return Vec3::newTemp(x * l, y * l, z * l); }

double Vec3::length() { return sqrt(x * x + y * y + z * z); }

Vec3 *Vec3::lerp(Vec3 *v, double a) {
  return Vec3::newTemp(x + (v->x - x) * a, y + (v->y - y) * a,
                       z + (v->z - z) * a);
}

void Vec3::xRot(float degs) {
  double c = cos(degs), s = sin(degs);
  double yy = y * c + z * s;
  double zz = z * c - y * s;
  y = yy;
  z = zz;
}

void Vec3::yRot(float degs) {
  double c = cos(degs), s = sin(degs);
  double xx = x * c + z * s;
  double zz = z * c - x * s;
  x = xx;
  z = zz;
}

void Vec3::zRot(float degs) {
  double c = cos(degs), s = sin(degs);
  double xx = x * c + y * s;
  double yy = y * c - x * s;
  x = xx;
  y = yy;
}

double Vec3::distanceTo(AABB *box) {
  if (box->contains(this))
    return 0;
  double xd = 0, yd = 0, zd = 0;
  if (x < box->x0)
    xd = box->x0 - x;
  else if (x > box->x1)
    xd = x - box->x1;
  if (y < box->y0)
    yd = box->y0 - y;
  else if (y > box->y1)
    yd = y - box->y1;
  if (z < box->z0)
    zd = box->z0 - z;
  else if (z > box->z1)
    zd = z - box->z1;
  return sqrt(xd * xd + yd * yd + zd * zd);
}
