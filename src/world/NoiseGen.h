#pragma once
// NoiseGen.h — 2D octave noise generator (ImprovedNoise port/adaptation)
// Separated from WorldGen for clarity, similar to 4J Studios' structure
#include <stdint.h>

class NoiseGen {
public:
  // Returns noise value at (x, z) with a given seed
  static float noise2d(float x, float z, int64_t seed);

  // Multi-octave (fractal) noise — sum of octaves at increasing frequencies
  static float octaveNoise(float x, float z, int64_t seed, int octaves = 4,
                           float persistence = 0.5f);
};
