#pragma once
#include <stdint.h>

// Adapted from Minecraft.World/Random.h (4J Studios, 2014)
// Modifications: __int64 -> int64_t, removed stdafx.h include

class Random {
private:
  int64_t seed;
  bool haveNextNextGaussian;
  double nextNextGaussian;

protected:
  int next(int bits);

public:
  Random();
  Random(int64_t seed);
  void setSeed(int64_t s);
  void nextBytes(uint8_t *bytes, unsigned int count);
  double nextDouble();
  double nextGaussian();
  int nextInt();
  int nextInt(int n);
  float nextFloat();
  int64_t nextLong();
  bool nextBoolean();
};
