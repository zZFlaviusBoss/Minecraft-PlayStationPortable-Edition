#pragma once
#include "Chunk.h"
#include "AABB.h"
#include <math.h>
#include <vector>

class Random;

// Ticks per day
static const long long TICKS_PER_DAY = 24000LL;

class Level {
public:
  Level();
  ~Level();

  void generate(Random *rng);
  void computeLighting();
  
  void updateLight(int wx, int wy, int wz);
  void updateBlockLight(int wx, int wy, int wz, uint8_t oldLight, uint8_t newLight);
  void updateSkyLight(int wx, int wy, int wz, uint8_t oldLight, uint8_t newLight);

  Chunk *getChunk(int cx, int cz) const;
  void markDirty(int wx, int wy, int wz, uint8_t priority = 0, bool spreadNeighbors = true);

  uint8_t getBlock(int wx, int wy, int wz) const;
  void setBlock(int wx, int wy, int wz, uint8_t id);

  uint8_t getSkyLight(int wx, int wy, int wz) const;
  uint8_t getBlockLight(int wx, int wy, int wz) const;
  void setSkyLight(int wx, int wy, int wz, uint8_t val);
  void setBlockLight(int wx, int wy, int wz, uint8_t val);

  std::vector<AABB> getCubes(const AABB& box) const;

  // Calculate time of day
  float getTimeOfDay() const {
    long long dayStep = m_time % TICKS_PER_DAY;
    float td = (float)dayStep / (float)TICKS_PER_DAY - 0.25f;
    if (td < 0.0f)
      td += 1.0f;
    if (td > 1.0f)
      td -= 1.0f;
    float tdo = td;
    td = 1.0f - (cosf(tdo * 3.14159265f) + 1.0f) / 2.0f;
    td = tdo + (td - tdo) / 3.0f;
    return td;
  }

  // Get sun brightness
  float getSunBrightness() const {
    float td = getTimeOfDay();
    float br = cosf(td * 3.14159265f * 2.0f) * 2.0f + 0.5f;
    if (br < 0.0f) br = 0.0f;
    if (br > 1.0f) br = 1.0f;
    return br;
  }

  // Get previous brightness cycle
  float getLastSunBrightness() const { return m_lastSunBrightness; }

  int getDay() const { return (int)(m_time / TICKS_PER_DAY); }

  long long getTime() const { return m_time; }

  void tick() {
    // Advance time
    m_time += 1;
  }

private:
  Chunk *m_chunks[WORLD_CHUNKS_X][WORLD_CHUNKS_Z];
  long long m_time = 6000LL;
  float m_lastSunBrightness = 1.0f;
};
