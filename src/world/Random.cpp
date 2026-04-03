// Adapted from Minecraft.World/Random.cpp (4J Studios, 2014)
// Modifications: __int64 -> int64_t, QueryPerformanceCounter ->
// sceKernelGetSystemTimeWide

#include "Random.h"
#include <assert.h>
#include <math.h>
#include <pspkernel.h>
#include <stdint.h>

Random::Random() {
  // On PSP we use sceKernelGetSystemTimeWide() instead of
  // QueryPerformanceCounter
  int64_t seed = (int64_t)sceKernelGetSystemTimeWide();
  seed += 8682522807148012LL;
  setSeed(seed);
}

Random::Random(int64_t seed) { setSeed(seed); }

void Random::setSeed(int64_t s) {
  this->seed = (s ^ 0x5DEECE66DLL) & ((1LL << 48) - 1);
  haveNextNextGaussian = false;
}

int Random::next(int bits) {
  seed = (seed * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
  return (int)(seed >> (48 - bits));
}

void Random::nextBytes(uint8_t *bytes, unsigned int count) {
  for (unsigned int i = 0; i < count; i++)
    bytes[i] = (uint8_t)next(8);
}

double Random::nextDouble() {
  return (((int64_t)next(26) << 27) + next(27)) / (double)(1LL << 53);
}

double Random::nextGaussian() {
  if (haveNextNextGaussian) {
    haveNextNextGaussian = false;
    return nextNextGaussian;
  } else {
    double v1, v2, s;
    do {
      v1 = 2 * nextDouble() - 1;
      v2 = 2 * nextDouble() - 1;
      s = v1 * v1 + v2 * v2;
    } while (s >= 1 || s == 0);
    double multiplier = sqrt(-2 * log(s) / s);
    nextNextGaussian = v2 * multiplier;
    haveNextNextGaussian = true;
    return v1 * multiplier;
  }
}

int Random::nextInt() { return next(32); }

int Random::nextInt(int n) {
  assert(n > 0);
  if ((n & -n) == n)
    return (int)(((int64_t)next(31) * n) >> 31);

  int bits, val;
  do {
    bits = next(31);
    val = bits % n;
  } while (bits - val + (n - 1) < 0);
  return val;
}

float Random::nextFloat() { return next(24) / ((float)(1 << 24)); }

int64_t Random::nextLong() { return ((int64_t)next(32) << 32) + next(32); }

bool Random::nextBoolean() { return next(1) != 0; }
