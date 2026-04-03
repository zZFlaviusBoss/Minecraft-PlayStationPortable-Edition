#pragma once
#include <stdint.h>

// Adapted from Minecraft.World/Mth.h (4J Studios, 2014)

class Random;

class Mth {
private:
  static const int BIG_ENOUGH_INT;
  static const float BIG_ENOUGH_FLOAT;
  static float *_sin;
  static const float sinScale;

public:
  static const float PI_VAL;
  static const float DEGRAD; // degrees -> radians
  static const float RADDEG; // radians -> degrees

  static void init();
  static float sin(float i);
  static float cos(float i);
  static float sqrt(float x);
  static float sqrt(double x);
  static int floor(float v);
  static int64_t lfloor(double v);
  static int fastFloor(double x);
  static int floor(double v);
  static int absFloor(double v);
  static float abs(float v);
  static int abs(int v);
  static int ceil(float v);
  static int clamp(int value, int min, int max);
  static float clamp(float value, float min, float max);
  static double asbMax(double a, double b);
  static int intFloorDiv(int a, int b);
  static int nextInt(Random *random, int minInclusive, int maxInclusive);
  static float wrapDegrees(float input);
  static double wrapDegrees(double input);
  static bool almostEquals(double d1, double d2, double precision);
};
