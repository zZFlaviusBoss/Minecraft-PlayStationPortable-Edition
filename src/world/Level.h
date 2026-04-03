#pragma once
#include "Chunk.h"
#include "AABB.h"
#include <math.h>
#include <vector>
#include <queue>
#include <functional>

class Random;

// Ticks per day (MCPE 0.6.1 uses 20 TPS * 60 * 16 = 19200)
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
  uint8_t getWaterDepth(int wx, int wy, int wz) const;
  uint8_t getLavaDepth(int wx, int wy, int wz) const;
  void setWaterDepth(int wx, int wy, int wz, uint8_t depth);

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

  void tick();
  void setSimulationFocus(int wx, int wy, int wz, int radius);
  bool saveToFile(const char *path) const;
  bool loadFromFile(const char *path);

private:
  struct WaterTickNode {
    int dueTick;
    int idx;
    bool operator>(const WaterTickNode &o) const {
      if (dueTick != o.dueTick) return dueTick > o.dueTick;
      return idx > o.idx;
    }
  };

  void tickWater();
  bool isWaterBlock(uint8_t id) const;
  bool isLavaBlock(uint8_t id) const;
  int waterIndex(int wx, int wy, int wz) const;
  void scheduleWaterTick(int wx, int wy, int wz, int delayTicks);
  void wakeWaterNeighborhood(int wx, int wy, int wz, int delayTicks);
  void processWaterCell(int wx, int wy, int wz);
  void tickLava();
  void scheduleLavaTick(int wx, int wy, int wz, int delayTicks);
  void wakeLavaNeighborhood(int wx, int wy, int wz, int delayTicks);
  void processLavaCell(int wx, int wy, int wz);

  Chunk *m_chunks[WORLD_CHUNKS_X][WORLD_CHUNKS_Z];
  std::vector<uint8_t> m_waterDepth;
  std::priority_queue<WaterTickNode, std::vector<WaterTickNode>, std::greater<WaterTickNode>> m_waterTicks;
  std::vector<int> m_waterDue;
  std::vector<uint8_t> m_lavaDepth;
  std::priority_queue<WaterTickNode, std::vector<WaterTickNode>, std::greater<WaterTickNode>> m_lavaTicks;
  std::vector<int> m_lavaDue;
  long long m_time = 6000LL;
  float m_lastSunBrightness = 1.0f;
  int m_simFocusX = -1;
  int m_simFocusY = -1;
  int m_simFocusZ = -1;
  int m_simFocusRadius = 24;
  int m_simFocusYRadius = 24;
  bool m_waterDirty = true;
  int m_waterWakeX = -1;
  int m_waterWakeY = -1;
  int m_waterWakeZ = -1;
  int m_waterWakeRadius = 12;
  int m_waterWakeTicks = 0;
  bool m_lavaDirty = true;
  int m_lavaWakeX = -1;
  int m_lavaWakeY = -1;
  int m_lavaWakeZ = -1;
  int m_lavaWakeRadius = 8;
  int m_lavaWakeTicks = 0;
  bool m_inWaterSimUpdate = false;
};
