// Mth.cpp

#include "Mth.h"
#include "Random.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const int Mth::BIG_ENOUGH_INT = 1024;
const float Mth::BIG_ENOUGH_FLOAT = 1024.0f;
const float Mth::PI_VAL = (float)M_PI;
const float Mth::DEGRAD = (float)(M_PI / 180.0);
const float Mth::RADDEG = (float)(180.0 / M_PI);

float *Mth::_sin = 0;
const float Mth::sinScale = 65536.0f / (float)(M_PI * 2.0f);

void Mth::init() {
  if (_sin)
    return;
  _sin = new float[65536];
  for (int i = 0; i < 65536; i++)
    _sin[i] = (float)::sin(i * M_PI * 2.0 / 65536.0);
}

float Mth::sin(float i) {
  if (!_sin)
    init();
  return _sin[(int)(i * sinScale) & 65535];
}

float Mth::cos(float i) {
  if (!_sin)
    init();
  return _sin[(int)(i * sinScale + 16384) & 65535];
}

float Mth::sqrt(float x) { return (float)::sqrt(x); }
float Mth::sqrt(double x) { return (float)::sqrt(x); }

int Mth::floor(float v) {
  int i = (int)v;
  return v < i ? i - 1 : i;
}

int64_t Mth::lfloor(double v) {
  int64_t i = (int64_t)v;
  return v < i ? i - 1 : i;
}

int Mth::fastFloor(double x) {
  return (int)(x + BIG_ENOUGH_FLOAT) - BIG_ENOUGH_INT;
}

int Mth::floor(double v) {
  int i = (int)v;
  return v < i ? i - 1 : i;
}

int Mth::absFloor(double v) { return (int)(v >= 0 ? v : -v + 1); }

float Mth::abs(float v) { return v >= 0 ? v : -v; }
int Mth::abs(int v) { return v >= 0 ? v : -v; }

int Mth::ceil(float v) {
  int i = (int)v;
  return v > i ? i + 1 : i;
}

int Mth::clamp(int value, int min, int max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

float Mth::clamp(float value, float min, float max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

double Mth::asbMax(double a, double b) {
  if (a < 0)
    a = -a;
  if (b < 0)
    b = -b;
  return a > b ? a : b;
}

int Mth::intFloorDiv(int a, int b) {
  if (a < 0)
    return -((-a - 1) / b) - 1;
  return a / b;
}

int Mth::nextInt(Random *random, int minInclusive, int maxInclusive) {
  if (minInclusive >= maxInclusive)
    return minInclusive;
  return random->nextInt(maxInclusive - minInclusive + 1) + minInclusive;
}

float Mth::wrapDegrees(float input) {
  while (input >= 180)
    input -= 360;
  while (input < -180)
    input += 360;
  return input;
}

double Mth::wrapDegrees(double input) {
  while (input >= 180)
    input -= 360;
  while (input < -180)
    input += 360;
  return input;
}

bool Mth::almostEquals(double d1, double d2, double precision) {
  double diff = d1 - d2;
  if (diff < 0)
    diff = -diff;
  return diff <= precision;
}
