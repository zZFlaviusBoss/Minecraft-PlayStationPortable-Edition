// NoiseGen.cpp - 2D noise implementation with octaves
// Ported/adapted from Minecraft.World/ImprovedNoise.cpp (4J Studios)

#include "NoiseGen.h"
#include <math.h>

static float hash2d(int ix, int iz, int64_t seed) {
  uint32_t h = (uint32_t)(seed ^ (ix * 374761393LL) ^ (iz * 668265263LL));
  h = (h ^ (h >> 13)) * 1274126177;
  h = h ^ (h >> 16);
  return (float)(h & 0xFFFF) / 65535.0f;
}

static float smoothNoise2d(float x, float z, int64_t seed) {
  int ix = (int)floorf(x);
  int iz = (int)floorf(z);
  float fx = x - ix;
  float fz = z - iz;

  // Smoothstep interpolation
  float ux = fx * fx * (3.0f - 2.0f * fx);
  float uz = fz * fz * (3.0f - 2.0f * fz);

  float a = hash2d(ix, iz, seed);
  float b = hash2d(ix + 1, iz, seed);
  float c = hash2d(ix, iz + 1, seed);
  float d = hash2d(ix + 1, iz + 1, seed);

  return a + (b - a) * ux + (c - a) * uz + (a - b - c + d) * ux * uz;
}

float NoiseGen::noise2d(float x, float z, int64_t seed) {
  return smoothNoise2d(x, z, seed);
}

float NoiseGen::octaveNoise(float x, float z, int64_t seed, int octaves,
                            float persistence) {
  float total = 0.0f;
  float freq = 1.0f;
  float amp = 1.0f;
  float maxVal = 0.0f;

  for (int i = 0; i < octaves; i++) {
    total += smoothNoise2d(x * freq, z * freq, seed + i * 73856093LL) * amp;
    maxVal += amp;
    amp *= persistence;
    freq *= 2.0f;
  }

  return total / maxVal;
}
