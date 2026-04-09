#include "Level.h"
#include "Random.h"
#include "WorldGen.h"
#include "TreeFeature.h"
#include <vector>
#include <algorithm>
#include <functional>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

struct LightNode {
  int x, y, z;
};

static inline bool blocksLightFully(uint8_t id) {
  const BlockProps &bp = g_blockProps[id];
  if (!bp.isOpaque()) return false;
  const float eps = 0.0001f;
  return bp.minX <= eps && bp.minY <= eps && bp.minZ <= eps &&
         bp.maxX >= 1.0f - eps && bp.maxY >= 1.0f - eps && bp.maxZ >= 1.0f - eps;
}

static inline int lightAttenuationFor(uint8_t id) {
  if (blocksLightFully(id)) return 15;
  if (id == BLOCK_LEAVES) return 2;
  if (id == BLOCK_WATER_STILL || id == BLOCK_WATER_FLOW || id == BLOCK_LAVA_STILL || id == BLOCK_LAVA_FLOW) return 3;
  return 1;
}

bool Level::isWaterBlock(uint8_t id) const {
  return id == BLOCK_WATER_STILL || id == BLOCK_WATER_FLOW;
}

bool Level::isLavaBlock(uint8_t id) const {
  return id == BLOCK_LAVA_STILL || id == BLOCK_LAVA_FLOW;
}

bool Level::isFallingBlock(uint8_t id) const {
  return id == BLOCK_SAND || id == BLOCK_GRAVEL;
}

bool Level::canFallThrough(uint8_t id) const {
  return id == BLOCK_AIR || id == BLOCK_FIRE || isWaterBlock(id) || isLavaBlock(id);
}

void Level::setSimulationFocus(int wx, int wy, int wz, int radius) {
  m_simFocusX = wx;
  m_simFocusY = wy;
  m_simFocusZ = wz;
  m_simFocusRadius = radius;
}

void Level::tick() {
  m_time += 1;
  if (m_waterDirty) tickWater();
  if (m_lavaDirty) tickLava();
  tickFallingBlocks();
  if (m_waterWakeTicks > 0) m_waterWakeTicks--;
  if (m_lavaWakeTicks > 0) m_lavaWakeTicks--;
}

void Level::tickWater() {
  // Budget per world tick (world ticks run at fixed 20 TPS from main loop).
  const int maxTicksPerWorldTick = 128;
  int processed = 0;
  while (processed < maxTicksPerWorldTick && !m_waterTicks.empty()) {
    WaterTickNode n = m_waterTicks.top();
    if (n.dueTick > (int)m_time) break;
    m_waterTicks.pop();
    if (n.idx < 0 || n.idx >= (int)m_waterDue.size()) continue;
    if (m_waterDue[n.idx] != n.dueTick) continue;
    m_waterDue[n.idx] = -1;

    int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
    int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
    int x = n.idx % maxX;
    int tmp = n.idx / maxX;
    int z = tmp % maxZ;
    int y = tmp / maxZ;
    processWaterCell(x, y, z);
    processed++;
  }
  m_waterDirty = !m_waterTicks.empty();
}

void Level::tickLava() {
  // Lava moves slower than water and has a lower per-tick processing budget.
  const int maxTicksPerWorldTick = 64;
  int processed = 0;
  while (processed < maxTicksPerWorldTick && !m_lavaTicks.empty()) {
    WaterTickNode n = m_lavaTicks.top();
    if (n.dueTick > (int)m_time) break;
    m_lavaTicks.pop();
    if (n.idx < 0 || n.idx >= (int)m_lavaDue.size()) continue;
    if (m_lavaDue[n.idx] != n.dueTick) continue;
    m_lavaDue[n.idx] = -1;

    int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
    int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
    int x = n.idx % maxX;
    int tmp = n.idx / maxX;
    int z = tmp % maxZ;
    int y = tmp / maxZ;
    processLavaCell(x, y, z);
    processed++;
  }
  m_lavaDirty = !m_lavaTicks.empty();
}

void Level::processWaterCell(int x, int y, int z) {
  int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
  int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
  auto inBounds = [&](int wx, int wy, int wz) {
    return wx >= 0 && wx < maxX && wz >= 0 && wz < maxZ && wy >= 0 && wy < CHUNK_SIZE_Y;
  };
  if (!inBounds(x, y, z)) return;
  uint8_t id = getBlock(x, y, z);
  if (!isWaterBlock(id)) return;

  auto getDepth = [&](int wx, int wy, int wz) -> int {
    if (!inBounds(wx, wy, wz)) return -1;
    uint8_t bid = getBlock(wx, wy, wz);
    if (!isWaterBlock(bid)) return -1;
    uint8_t d = getWaterDepth(wx, wy, wz);
    if (d == 0xFF) d = (bid == BLOCK_WATER_STILL) ? 0 : 1;
    return (int)d;
  };
  auto isWaterBlocking = [&](int wx, int wy, int wz) -> bool {
    if (!inBounds(wx, wy, wz)) return true;
    uint8_t bid = getBlock(wx, wy, wz);
    if (bid == BLOCK_AIR) return false;
    return g_blockProps[bid].isSolid();
  };
  auto canSpreadTo = [&](int wx, int wy, int wz) -> bool {
    if (!inBounds(wx, wy, wz)) return false;
    uint8_t bid = getBlock(wx, wy, wz);
    if (isWaterBlock(bid)) return false;
    return !isWaterBlocking(wx, wy, wz);
  };
  auto setWaterCell = [&](int wx, int wy, int wz, uint8_t bid, uint8_t d) {
    m_inWaterSimUpdate = true;
    setBlock(wx, wy, wz, bid);
    m_inWaterSimUpdate = false;
    setWaterDepth(wx, wy, wz, d);
  };

  int depth = getDepth(x, y, z);
  if (depth < 0) return;
  int dropOff = 1;
  bool becomeStatic = true;
  int maxCount = 0;

  if (depth > 0) {
    int highest = -100;
    static const int dx[4] = {-1, 1, 0, 0};
    static const int dz[4] = {0, 0, -1, 1};
    for (int i = 0; i < 4; ++i) {
      int nd = getDepth(x + dx[i], y, z + dz[i]);
      if (nd < 0) continue;
      if (nd == 0) maxCount++;
      if (nd >= 8) nd = 0;
      if (highest < 0 || nd < highest) highest = nd;
    }

    int above = getDepth(x, y + 1, z);
    int newDepth = highest + dropOff;
    if (newDepth >= 8 || highest < 0) newDepth = -1;
    if (above >= 0) newDepth = (above >= 8) ? above : (above + 8);

    if (maxCount >= 2) {
      if (isWaterBlocking(x, y - 1, z)) newDepth = 0;
      else if (isWaterBlock(getBlock(x, y - 1, z)) && getDepth(x, y - 1, z) == 0) newDepth = 0;
    }

    if (newDepth != depth) {
      if (newDepth < 0) {
        setWaterCell(x, y, z, BLOCK_AIR, 0xFF);
        wakeWaterNeighborhood(x, y, z, 5);
        return;
      }

      depth = newDepth;
      setWaterDepth(x, y, z, (uint8_t)depth);
      scheduleWaterTick(x, y, z, 5);
      wakeWaterNeighborhood(x, y, z, 5);
    } else {
      if (becomeStatic) setWaterCell(x, y, z, BLOCK_WATER_STILL, (uint8_t)depth);
    }
  } else {
    setWaterCell(x, y, z, BLOCK_WATER_STILL, 0);
  }

  if (canSpreadTo(x, y - 1, z)) {
    int d = (depth >= 8) ? depth : (depth + 8);
    if (d > 7) d = 7;
    setWaterCell(x, y - 1, z, BLOCK_WATER_FLOW, (uint8_t)d);
    scheduleWaterTick(x, y - 1, z, 5);
  } else if (depth >= 0 && (depth == 0 || isWaterBlocking(x, y - 1, z))) {
    int neighbor = depth + dropOff;
    if (depth >= 8) neighbor = 1;
    if (neighbor >= 8) return;

    static const int dx[4] = {-1, 1, 0, 0};
    static const int dz[4] = {0, 0, -1, 1};
    int dist[4] = {1000, 1000, 1000, 1000};
    bool spread[4] = {false, false, false, false};
    static const int opposite[4] = {1, 0, 3, 2};
    const int maxSlopePass = 6; // expand pit detection by one more block
    std::function<int(int, int, int, int, int)> slopeDistance =
        [&](int wx, int wy, int wz, int pass, int from) -> int {
      int lowest = 1000;
      for (int d = 0; d < 4; ++d) {
        if (from >= 0 && d == opposite[from]) continue;
        int nx = wx + dx[d], nz = wz + dz[d];
        if (isWaterBlocking(nx, wy, nz)) continue;
        int nd = getDepth(nx, wy, nz);
        if (nd == 0) continue;
        if (!isWaterBlocking(nx, wy - 1, nz)) return pass;
        if (pass < maxSlopePass) {
          int v = slopeDistance(nx, wy, nz, pass + 1, d);
          if (v < lowest) lowest = v;
        }
      }
      return lowest;
    };

    for (int d = 0; d < 4; ++d) {
      int nx = x + dx[d], nz = z + dz[d];
      if (isWaterBlocking(nx, y, nz)) continue;
      int nd = getDepth(nx, y, nz);
      if (nd == 0) continue;
      if (!isWaterBlocking(nx, y - 1, nz)) dist[d] = 0;
      else dist[d] = slopeDistance(nx, y, nz, 1, d);
    }
    int lowest = dist[0];
    for (int d = 1; d < 4; ++d) if (dist[d] < lowest) lowest = dist[d];
    for (int d = 0; d < 4; ++d) spread[d] = (dist[d] == lowest);
    for (int d = 0; d < 4; ++d) {
      if (!spread[d]) continue;
      int nx = x + dx[d], nz = z + dz[d];
      if (!canSpreadTo(nx, y, nz)) continue;
      setWaterCell(nx, y, nz, BLOCK_WATER_FLOW, (uint8_t)neighbor);
      scheduleWaterTick(nx, y, nz, 5);
    }
  }
}

void Level::processLavaCell(int x, int y, int z) {
  int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
  int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
  auto inBounds = [&](int wx, int wy, int wz) {
    return wx >= 0 && wx < maxX && wz >= 0 && wz < maxZ && wy >= 0 && wy < CHUNK_SIZE_Y;
  };
  if (!inBounds(x, y, z)) return;
  uint8_t id = getBlock(x, y, z);
  if (!isLavaBlock(id)) return;

  auto getDepth = [&](int wx, int wy, int wz) -> int {
    if (!inBounds(wx, wy, wz)) return -1;
    uint8_t bid = getBlock(wx, wy, wz);
    if (!isLavaBlock(bid)) return -1;
    uint8_t d = m_lavaDepth[waterIndex(wx, wy, wz)];
    if (d == 0xFF) d = (bid == BLOCK_LAVA_STILL) ? 0 : 1;
    return (int)d;
  };
  auto isLavaBlocking = [&](int wx, int wy, int wz) -> bool {
    if (!inBounds(wx, wy, wz)) return true;
    uint8_t bid = getBlock(wx, wy, wz);
    if (bid == BLOCK_AIR) return false;
    return g_blockProps[bid].isSolid();
  };
  auto canSpreadTo = [&](int wx, int wy, int wz) -> bool {
    if (!inBounds(wx, wy, wz)) return false;
    uint8_t bid = getBlock(wx, wy, wz);
    if (isLavaBlock(bid)) return false;
    return !isLavaBlocking(wx, wy, wz);
  };
  auto setLavaCell = [&](int wx, int wy, int wz, uint8_t bid, uint8_t d) {
    m_inWaterSimUpdate = true;
    setBlock(wx, wy, wz, bid);
    m_inWaterSimUpdate = false;
    m_lavaDepth[waterIndex(wx, wy, wz)] = d;
  };

  int depth = getDepth(x, y, z);
  if (depth < 0) return;
  const int dropOff = 2;
  bool becomeStatic = true;
  int maxCount = 0;

  if (depth > 0) {
    int highest = -100;
    static const int dx[4] = {-1, 1, 0, 0};
    static const int dz[4] = {0, 0, -1, 1};
    for (int i = 0; i < 4; ++i) {
      int nd = getDepth(x + dx[i], y, z + dz[i]);
      if (nd < 0) continue;
      if (nd == 0) maxCount++;
      if (nd >= 8) nd = 0;
      if (highest < 0 || nd < highest) highest = nd;
    }

    int above = getDepth(x, y + 1, z);
    int newDepth = highest + dropOff;
    if (newDepth >= 8 || highest < 0) newDepth = -1;
    if (above >= 0) newDepth = (above >= 8) ? above : (above + 8);

    if (maxCount >= 2) {
      if (isLavaBlocking(x, y - 1, z)) newDepth = 0;
      else if (isLavaBlock(getBlock(x, y - 1, z)) && getDepth(x, y - 1, z) == 0) newDepth = 0;
    }

    if (newDepth != depth) {
      if (newDepth < 0) {
        setLavaCell(x, y, z, BLOCK_AIR, 0xFF);
        wakeLavaNeighborhood(x, y, z, 12);
        return;
      }

      depth = newDepth;
      m_lavaDepth[waterIndex(x, y, z)] = (uint8_t)depth;
      scheduleLavaTick(x, y, z, 12);
      wakeLavaNeighborhood(x, y, z, 12);
    } else {
      if (becomeStatic) setLavaCell(x, y, z, BLOCK_LAVA_STILL, (uint8_t)depth);
    }
  } else {
    setLavaCell(x, y, z, BLOCK_LAVA_STILL, 0);
  }

  if (canSpreadTo(x, y - 1, z)) {
    int d = (depth >= 8) ? depth : (depth + 8);
    if (d > 7) d = 7;
    setLavaCell(x, y - 1, z, BLOCK_LAVA_FLOW, (uint8_t)d);
    scheduleLavaTick(x, y - 1, z, 12);
  } else if (depth >= 0 && (depth == 0 || isLavaBlocking(x, y - 1, z))) {
    int neighbor = depth + dropOff;
    if (depth >= 8) neighbor = 2;
    if (neighbor >= 8) return;

    static const int dx[4] = {-1, 1, 0, 0};
    static const int dz[4] = {0, 0, -1, 1};
    int dist[4] = {1000, 1000, 1000, 1000};
    bool spread[4] = {false, false, false, false};
    static const int opposite[4] = {1, 0, 3, 2};
    const int maxSlopePass = 2;
    std::function<int(int, int, int, int, int)> slopeDistance =
        [&](int wx, int wy, int wz, int pass, int from) -> int {
      int lowest = 1000;
      for (int d = 0; d < 4; ++d) {
        if (from >= 0 && d == opposite[from]) continue;
        int nx = wx + dx[d], nz = wz + dz[d];
        if (isLavaBlocking(nx, wy, nz)) continue;
        int nd = getDepth(nx, wy, nz);
        if (nd == 0) continue;
        if (!isLavaBlocking(nx, wy - 1, nz)) return pass;
        if (pass < maxSlopePass) {
          int v = slopeDistance(nx, wy, nz, pass + 1, d);
          if (v < lowest) lowest = v;
        }
      }
      return lowest;
    };

    for (int d = 0; d < 4; ++d) {
      int nx = x + dx[d], nz = z + dz[d];
      if (isLavaBlocking(nx, y, nz)) continue;
      int nd = getDepth(nx, y, nz);
      if (nd == 0) continue;
      if (!isLavaBlocking(nx, y - 1, nz)) dist[d] = 0;
      else dist[d] = slopeDistance(nx, y, nz, 1, d);
    }
    int lowest = dist[0];
    for (int d = 1; d < 4; ++d) if (dist[d] < lowest) lowest = dist[d];
    for (int d = 0; d < 4; ++d) spread[d] = (dist[d] == lowest);
    for (int d = 0; d < 4; ++d) {
      if (!spread[d]) continue;
      int nx = x + dx[d], nz = z + dz[d];
      if (!canSpreadTo(nx, y, nz)) continue;
      setLavaCell(nx, y, nz, BLOCK_LAVA_FLOW, (uint8_t)neighbor);
      scheduleLavaTick(nx, y, nz, 12);
    }
  }
}

Level::Level() {
  memset(m_chunks, 0, sizeof(m_chunks));
  m_waterDepth.resize(WORLD_CHUNKS_X * CHUNK_SIZE_X * CHUNK_SIZE_Y * WORLD_CHUNKS_Z * CHUNK_SIZE_Z, 0xFF);
  m_waterDue.resize(m_waterDepth.size(), -1);
  m_lavaDepth.resize(m_waterDepth.size(), 0xFF);
  m_lavaDue.resize(m_waterDepth.size(), -1);
  m_fallDue.resize(m_waterDepth.size(), -1);
  for (int cx = 0; cx < WORLD_CHUNKS_X; cx++)
    for (int cz = 0; cz < WORLD_CHUNKS_Z; cz++)
      m_chunks[cx][cz] = new Chunk();
}

Level::~Level() {
  for (int cx = 0; cx < WORLD_CHUNKS_X; cx++)
    for (int cz = 0; cz < WORLD_CHUNKS_Z; cz++)
      delete m_chunks[cx][cz];
}

Chunk* Level::getChunk(int cx, int cz) const {
  if (cx < 0 || cx >= WORLD_CHUNKS_X || cz < 0 || cz >= WORLD_CHUNKS_Z) return nullptr;
  return m_chunks[cx][cz];
}

void Level::markDirty(int wx, int wy, int wz, uint8_t priority, bool spreadNeighbors) {
  int cx = wx >> 4;
  int cz = wz >> 4;
  int sy = wy >> 4;
  const int subchunkCount = CHUNK_SIZE_Y / 16;
  if (cx >= 0 && cx < WORLD_CHUNKS_X && cz >= 0 && cz < WORLD_CHUNKS_Z && sy >= 0 && sy < subchunkCount) {
    if (priority > m_chunks[cx][cz]->priority[sy]) m_chunks[cx][cz]->priority[sy] = priority;
    m_chunks[cx][cz]->dirty[sy] = true;

    if (spreadNeighbors) {
      int lx = wx & 0xF;
      int lz = wz & 0xF;
      int ly = wy & 0xF;
      auto markNeighbor = [&](int ncx, int ncz, int nsy) {
        if (ncx >= 0 && ncx < WORLD_CHUNKS_X && ncz >= 0 && ncz < WORLD_CHUNKS_Z && nsy >= 0 && nsy < subchunkCount) {
          if (priority > m_chunks[ncx][ncz]->priority[nsy]) m_chunks[ncx][ncz]->priority[nsy] = priority;
          m_chunks[ncx][ncz]->dirty[nsy] = true;
        }
      };
      if (lx == 0) markNeighbor(cx - 1, cz, sy);
      if (lx == 15) markNeighbor(cx + 1, cz, sy);
      if (lz == 0) markNeighbor(cx, cz - 1, sy);
      if (lz == 15) markNeighbor(cx, cz + 1, sy);
      if (ly == 0) markNeighbor(cx, cz, sy - 1);
      if (ly == 15) markNeighbor(cx, cz, sy + 1);
    }
  }
}

uint8_t Level::getBlock(int wx, int wy, int wz) const {
  int cx = wx >> 4;
  int cz = wz >> 4;
  if (cx < 0 || cx >= WORLD_CHUNKS_X || cz < 0 || cz >= WORLD_CHUNKS_Z || wy < 0 || wy >= CHUNK_SIZE_Y) return BLOCK_AIR;
  return m_chunks[cx][cz]->getBlock(wx & 0xF, wy, wz & 0xF);
}

int Level::waterIndex(int wx, int wy, int wz) const {
  return ((wy * (WORLD_CHUNKS_Z * CHUNK_SIZE_Z) + wz) * (WORLD_CHUNKS_X * CHUNK_SIZE_X) + wx);
}

uint8_t Level::getWaterDepth(int wx, int wy, int wz) const {
  int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
  int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
  if (wx < 0 || wx >= maxX || wz < 0 || wz >= maxZ || wy < 0 || wy >= CHUNK_SIZE_Y) return 0xFF;
  return m_waterDepth[waterIndex(wx, wy, wz)];
}

uint8_t Level::getLavaDepth(int wx, int wy, int wz) const {
  int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
  int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
  if (wx < 0 || wx >= maxX || wz < 0 || wz >= maxZ || wy < 0 || wy >= CHUNK_SIZE_Y) return 0xFF;
  return m_lavaDepth[waterIndex(wx, wy, wz)];
}

bool Level::applyWaterCurrent(const AABB& box, float& velX, float& velY, float& velZ) const {
  int x0 = (int)floor(box.x0);
  int x1 = (int)floor(box.x1 + 1.0);
  int y0 = (int)floor(box.y0);
  int y1 = (int)floor(box.y1 + 1.0);
  int z0 = (int)floor(box.z0);
  int z1 = (int)floor(box.z1 + 1.0);

  int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
  int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
  if (x0 < 0 || z0 < 0 || y0 < 0 || x1 > maxX || z1 > maxZ || y1 > CHUNK_SIZE_Y) return false;

  auto getRenderedDepth = [&](int wx, int wy, int wz) -> int {
    uint8_t bid = getBlock(wx, wy, wz);
    if (!isWaterBlock(bid)) return -1;
    uint8_t d = getWaterDepth(wx, wy, wz);
    if (d == 0xFF) d = (bid == BLOCK_WATER_STILL) ? 0 : 1;
    if (d >= 8) d = 0;
    return (int)d;
  };

  auto shouldRenderFace = [&](int wx, int wy, int wz) -> bool {
    uint8_t bid = getBlock(wx, wy, wz);
    return !isWaterBlock(bid) && bid != BLOCK_ICE;
  };

  auto addFlowForCell = [&](int wx, int wy, int wz, double& fx, double& fy, double& fz) {
    const int mid = getRenderedDepth(wx, wy, wz);
    if (mid < 0) return;

    double cellX = 0.0, cellY = 0.0, cellZ = 0.0;
    static const int dx[4] = {-1, 0, 1, 0};
    static const int dz[4] = {0, -1, 0, 1};
    for (int d = 0; d < 4; ++d) {
      const int nx = wx + dx[d];
      const int nz = wz + dz[d];
      int nd = getRenderedDepth(nx, wy, nz);
      if (nd < 0) {
        if (!g_blockProps[getBlock(nx, wy, nz)].isSolid()) {
          nd = getRenderedDepth(nx, wy - 1, nz);
          if (nd >= 0) {
            const int dir = nd - (mid - 8);
            cellX += (double)(nx - wx) * (double)dir;
            cellY += 0.0;
            cellZ += (double)(nz - wz) * (double)dir;
          }
        }
      } else {
        const int dir = nd - mid;
        cellX += (double)(nx - wx) * (double)dir;
        cellY += 0.0;
        cellZ += (double)(nz - wz) * (double)dir;
      }
    }

    uint8_t d = getWaterDepth(wx, wy, wz);
    if (d != 0xFF && d >= 8) {
      bool falling = false;
      if (shouldRenderFace(wx, wy, wz - 1)) falling = true;
      if (shouldRenderFace(wx, wy, wz + 1)) falling = true;
      if (shouldRenderFace(wx - 1, wy, wz)) falling = true;
      if (shouldRenderFace(wx + 1, wy, wz)) falling = true;
      if (shouldRenderFace(wx, wy + 1, wz - 1)) falling = true;
      if (shouldRenderFace(wx, wy + 1, wz + 1)) falling = true;
      if (shouldRenderFace(wx - 1, wy + 1, wz)) falling = true;
      if (shouldRenderFace(wx + 1, wy + 1, wz)) falling = true;
      if (falling) {
        double len = sqrt(cellX * cellX + cellY * cellY + cellZ * cellZ);
        if (len > 0.00001) {
          cellX /= len;
          cellY /= len;
          cellZ /= len;
        }
        cellY += -6.0;
      }
    }

    double len = sqrt(cellX * cellX + cellY * cellY + cellZ * cellZ);
    if (len > 0.00001) {
      cellX /= len;
      cellY /= len;
      cellZ /= len;
    }

    fx += cellX * 0.5;
    fy += cellY * 0.5;
    fz += cellZ * 0.5;
  };

  bool inWater = false;
  double flowX = 0.0, flowY = 0.0, flowZ = 0.0;
  for (int wx = x0; wx < x1; ++wx) {
    for (int wy = y0; wy < y1; ++wy) {
      for (int wz = z0; wz < z1; ++wz) {
        uint8_t bid = getBlock(wx, wy, wz);
        if (!isWaterBlock(bid)) continue;
        uint8_t d = getWaterDepth(wx, wy, wz);
        if (d == 0xFF) d = (bid == BLOCK_WATER_STILL) ? 0 : 1;
        if (d >= 8) d = 0;
        const float liquidHeight = (float)(wy + 1) - (float)(d + 1) / 9.0f;
        if ((float)y1 >= liquidHeight) {
          inWater = true;
          addFlowForCell(wx, wy, wz, flowX, flowY, flowZ);
        }
      }
    }
  }

  const double len = sqrt(flowX * flowX + flowY * flowY + flowZ * flowZ);
  if (len > 0.0) {
    const float p = (float)(0.004 / len);
    velX += (float)flowX * p;
    velY += (float)flowY * p;
    velZ += (float)flowZ * p;
  }
  return inWater;
}

void Level::setWaterDepth(int wx, int wy, int wz, uint8_t depth) {
  int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
  int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
  if (wx < 0 || wx >= maxX || wz < 0 || wz >= maxZ || wy < 0 || wy >= CHUNK_SIZE_Y) return;
  m_waterDepth[waterIndex(wx, wy, wz)] = depth;
}

void Level::scheduleWaterTick(int wx, int wy, int wz, int delayTicks) {
  int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
  int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
  if (wx < 0 || wx >= maxX || wz < 0 || wz >= maxZ || wy < 0 || wy >= CHUNK_SIZE_Y) return;
  int idx = waterIndex(wx, wy, wz);
  int due = (int)m_time + delayTicks;
  if (idx < 0 || idx >= (int)m_waterDue.size()) return;
  if (m_waterDue[idx] != -1 && m_waterDue[idx] <= due) return;
  m_waterDue[idx] = due;
  m_waterTicks.push({due, idx});
  m_waterDirty = true;
}

void Level::scheduleLavaTick(int wx, int wy, int wz, int delayTicks) {
  int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
  int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
  if (wx < 0 || wx >= maxX || wz < 0 || wz >= maxZ || wy < 0 || wy >= CHUNK_SIZE_Y) return;
  int idx = waterIndex(wx, wy, wz);
  int due = (int)m_time + delayTicks;
  if (idx < 0 || idx >= (int)m_lavaDue.size()) return;
  if (m_lavaDue[idx] != -1 && m_lavaDue[idx] <= due) return;
  m_lavaDue[idx] = due;
  m_lavaTicks.push({due, idx});
  m_lavaDirty = true;
}

void Level::wakeWaterNeighborhood(int wx, int wy, int wz, int delayTicks) {
  static const int dx[7] = {0, -1, 1, 0, 0, 0, 0};
  static const int dy[7] = {0, 0, 0, -1, 1, 0, 0};
  static const int dz[7] = {0, 0, 0, 0, 0, -1, 1};
  for (int i = 0; i < 7; ++i) {
    int nx = wx + dx[i], ny = wy + dy[i], nz = wz + dz[i];
    if (isWaterBlock(getBlock(nx, ny, nz))) scheduleWaterTick(nx, ny, nz, delayTicks);
  }
}

void Level::wakeLavaNeighborhood(int wx, int wy, int wz, int delayTicks) {
  static const int dx[7] = {0, -1, 1, 0, 0, 0, 0};
  static const int dy[7] = {0, 0, 0, -1, 1, 0, 0};
  static const int dz[7] = {0, 0, 0, 0, 0, -1, 1};
  for (int i = 0; i < 7; ++i) {
    int nx = wx + dx[i], ny = wy + dy[i], nz = wz + dz[i];
    if (isLavaBlock(getBlock(nx, ny, nz))) scheduleLavaTick(nx, ny, nz, delayTicks);
  }
}

int Level::fallIndex(int wx, int wy, int wz) const {
  return waterIndex(wx, wy, wz);
}

void Level::scheduleFallCheck(int wx, int wy, int wz, int delayTicks) {
  int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
  int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
  if (wx < 0 || wx >= maxX || wz < 0 || wz >= maxZ || wy < 0 || wy >= CHUNK_SIZE_Y) return;
  int idx = fallIndex(wx, wy, wz);
  int due = (int)m_time + delayTicks;
  if (idx < 0 || idx >= (int)m_fallDue.size()) return;
  if (m_fallDue[idx] != -1 && m_fallDue[idx] <= due) return;
  m_fallDue[idx] = due;
  m_fallTicks.push({due, idx});
}

void Level::scheduleFallNeighborhood(int wx, int wy, int wz, int delayTicks) {
  static const int dx[7] = {0, -1, 1, 0, 0, 0, 0};
  static const int dy[7] = {0, 0, 0, -1, 1, 0, 0};
  static const int dz[7] = {0, 0, 0, 0, 0, -1, 1};
  for (int i = 0; i < 7; ++i) {
    scheduleFallCheck(wx + dx[i], wy + dy[i], wz + dz[i], delayTicks);
  }
}

void Level::processFallCheck(int wx, int wy, int wz) {
  uint8_t id = getBlock(wx, wy, wz);
  if (!isFallingBlock(id)) return;
  if (wy <= 0) return;
  if (!canFallThrough(getBlock(wx, wy - 1, wz))) return;

  setBlock(wx, wy, wz, BLOCK_AIR);
  FallingBlockEntity e;
  e.x = (float)wx + 0.5f;
  e.y = (float)wy + 0.5f;
  e.z = (float)wz + 0.5f;
  e.xd = 0.0f;
  e.yd = 0.0f;
  e.zd = 0.0f;
  e.id = id;
  e.age = 0;
  e.removed = false;
  m_fallingBlocks.push_back(e);
}

void Level::tickFallingBlocks() {
  const int maxTicksPerWorldTick = 128;
  int processed = 0;
  while (processed < maxTicksPerWorldTick && !m_fallTicks.empty()) {
    WaterTickNode n = m_fallTicks.top();
    if (n.dueTick > (int)m_time) break;
    m_fallTicks.pop();
    if (n.idx < 0 || n.idx >= (int)m_fallDue.size()) continue;
    if (m_fallDue[n.idx] != n.dueTick) continue;
    m_fallDue[n.idx] = -1;

    int maxX = WORLD_CHUNKS_X * CHUNK_SIZE_X;
    int maxZ = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;
    int x = n.idx % maxX;
    int tmp = n.idx / maxX;
    int z = tmp % maxZ;
    int y = tmp / maxZ;
    processFallCheck(x, y, z);
    processed++;
  }

  for (size_t i = 0; i < m_fallingBlocks.size(); ++i) {
    FallingBlockEntity &e = m_fallingBlocks[i];
    if (e.removed) continue;
    e.age++;
    e.yd -= 0.04f;
    e.x += e.xd;
    e.y += e.yd;
    e.z += e.zd;
    e.xd *= 0.98f;
    e.yd *= 0.98f;
    e.zd *= 0.98f;

    int xt = (int)floorf(e.x);
    int yt = (int)floorf(e.y);
    int zt = (int)floorf(e.z);
    if (xt < 0 || zt < 0 || xt >= WORLD_CHUNKS_X * CHUNK_SIZE_X || zt >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z || yt < -1) {
      e.removed = true;
      continue;
    }

    int belowY = (int)floorf(e.y - 0.51f);
    if (belowY < 0) {
      int placeY = 0;
      if (canFallThrough(getBlock(xt, placeY, zt))) {
        setBlock(xt, placeY, zt, e.id);
      }
      e.removed = true;
      continue;
    }

    if (!canFallThrough(getBlock(xt, belowY, zt))) {
      int placeY = belowY + 1;
      if (placeY >= 0 && placeY < CHUNK_SIZE_Y && canFallThrough(getBlock(xt, placeY, zt))) {
        setBlock(xt, placeY, zt, e.id);
      } else if (yt >= 0 && yt < CHUNK_SIZE_Y && canFallThrough(getBlock(xt, yt, zt))) {
        setBlock(xt, yt, zt, e.id);
      }
      e.removed = true;
      continue;
    }

    if (e.age > 100) {
      int py = (int)floorf(e.y);
      if (py >= 0 && py < CHUNK_SIZE_Y && canFallThrough(getBlock(xt, py, zt))) {
        setBlock(xt, py, zt, e.id);
      }
      e.removed = true;
    }
  }

  m_fallingBlocks.erase(
      std::remove_if(m_fallingBlocks.begin(), m_fallingBlocks.end(),
                     [](const FallingBlockEntity &e) { return e.removed; }),
      m_fallingBlocks.end());
}

void Level::setBlock(int wx, int wy, int wz, uint8_t id) {
  static bool s_inCactusStabilize = false;
  int cx = wx >> 4;
  int cz = wz >> 4;
  if (cx < 0 || cx >= WORLD_CHUNKS_X || cz < 0 || cz >= WORLD_CHUNKS_Z || wy < 0 || wy >= CHUNK_SIZE_Y) return;
  uint8_t oldId = m_chunks[cx][cz]->getBlock(wx & 0xF, wy, wz & 0xF);
  if (oldId == id) return;
  
  m_chunks[cx][cz]->setBlock(wx & 0xF, wy, wz & 0xF, id);
  markDirty(wx, wy, wz);
  bool affectsFalling = isFallingBlock(oldId) || isFallingBlock(id);
  if (!affectsFalling) {
    static const int fdx[6] = {-1, 1, 0, 0, 0, 0};
    static const int fdy[6] = {0, 0, -1, 1, 0, 0};
    static const int fdz[6] = {0, 0, 0, 0, -1, 1};
    for (int i = 0; i < 6; ++i) {
      if (isFallingBlock(getBlock(wx + fdx[i], wy + fdy[i], wz + fdz[i]))) {
        affectsFalling = true;
        break;
      }
    }
  }
  if (affectsFalling) {
    scheduleFallNeighborhood(wx, wy, wz, 2);
  }
  if (isWaterBlock(id)) {
    setWaterDepth(wx, wy, wz, (id == BLOCK_WATER_STILL) ? 0 : 1);
    m_lavaDepth[waterIndex(wx, wy, wz)] = 0xFF;
  } else if (isLavaBlock(id)) {
    m_lavaDepth[waterIndex(wx, wy, wz)] = (id == BLOCK_LAVA_STILL) ? 0 : 1;
    setWaterDepth(wx, wy, wz, 0xFF);
  } else {
    setWaterDepth(wx, wy, wz, 0xFF);
    m_lavaDepth[waterIndex(wx, wy, wz)] = 0xFF;
  }

  bool touchesFluid = isWaterBlock(oldId) || isWaterBlock(id) || isLavaBlock(oldId) || isLavaBlock(id);
  if (!touchesFluid) {
    static const int dx[6] = {-1, 1, 0, 0, 0, 0};
    static const int dy[6] = {0, 0, -1, 1, 0, 0};
    static const int dz[6] = {0, 0, 0, 0, -1, 1};
    for (int i = 0; i < 6; ++i) {
      int nx = wx + dx[i], ny = wy + dy[i], nz = wz + dz[i];
      if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;
      uint8_t n = getBlock(nx, ny, nz);
      if (isWaterBlock(n) || isLavaBlock(n)) {
        touchesFluid = true;
        break;
      }
    }
  }
  if (touchesFluid) {
    if (!m_inWaterSimUpdate) {
      m_waterDirty = true;
      m_lavaDirty = true;
      m_waterWakeX = wx;
      m_waterWakeY = wy;
      m_waterWakeZ = wz;
      m_waterWakeTicks = 16;
      m_lavaWakeX = wx;
      m_lavaWakeY = wy;
      m_lavaWakeZ = wz;
      m_lavaWakeTicks = 20;
      wakeWaterNeighborhood(wx, wy, wz, 5);
      wakeLavaNeighborhood(wx, wy, wz, 12);
    }
  }

  updateLight(wx, wy, wz);

  // MCPE-like cactus survival:
  // - must stand on sand or cactus
  // - must have no solid horizontal neighbors
  // If support/neighbor changes, collapse invalid cactus blocks in the affected
  // column and adjacent columns immediately (matching classic MCPE behavior).
  if (!s_inCactusStabilize) {
    s_inCactusStabilize = true;
    auto canStayCactus = [&](int x, int y, int z) -> bool {
      if (getBlock(x, y, z) != BLOCK_CACTUS) return true;
      uint8_t below = getBlock(x, y - 1, z);
      if (below != BLOCK_SAND && below != BLOCK_CACTUS) return false;
      if (g_blockProps[getBlock(x - 1, y, z)].isSolid()) return false;
      if (g_blockProps[getBlock(x + 1, y, z)].isSolid()) return false;
      if (g_blockProps[getBlock(x, y, z - 1)].isSolid()) return false;
      if (g_blockProps[getBlock(x, y, z + 1)].isSolid()) return false;
      return true;
    };
    auto stabilizeColumn = [&](int x, int z) {
      for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
        if (getBlock(x, y, z) == BLOCK_CACTUS && !canStayCactus(x, y, z)) {
          for (int cy = y; cy < CHUNK_SIZE_Y && getBlock(x, cy, z) == BLOCK_CACTUS; ++cy) {
            setBlock(x, cy, z, BLOCK_AIR);
          }
          break;
        }
      }
    };
    stabilizeColumn(wx, wz);
    stabilizeColumn(wx - 1, wz);
    stabilizeColumn(wx + 1, wz);
    stabilizeColumn(wx, wz - 1);
    stabilizeColumn(wx, wz + 1);
    s_inCactusStabilize = false;
  }
}

std::vector<AABB> Level::getCubes(const AABB& box) const {
  std::vector<AABB> boxes;
  int x0 = (int)floorf(box.x0);
  int x1 = (int)floorf(box.x1 + 1.0f);
  int y0 = (int)floorf(box.y0);
  int y1 = (int)floorf(box.y1 + 1.0f);
  int z0 = (int)floorf(box.z0);
  int z1 = (int)floorf(box.z1 + 1.0f);

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (z0 < 0) z0 = 0;
  if (x1 > WORLD_CHUNKS_X * CHUNK_SIZE_X) x1 = WORLD_CHUNKS_X * CHUNK_SIZE_X;
  if (y1 > CHUNK_SIZE_Y) y1 = CHUNK_SIZE_Y;
  if (z1 > WORLD_CHUNKS_Z * CHUNK_SIZE_Z) z1 = WORLD_CHUNKS_Z * CHUNK_SIZE_Z;

  for (int x = x0; x < x1; x++) {
    for (int y = y0; y < y1; y++) {
      for (int z = z0; z < z1; z++) {
        uint8_t id = getBlock(x, y, z);
        if (id > 0 && g_blockProps[id].isSolid()) {
          if (isStairId(id)) {
            // Stair collision: lower full step + upper rear half, rotated by facing.
            bool upsideDown = isUpsideDownStair(id);
            if (!upsideDown) {
              boxes.push_back(AABB((double)x, (double)y, (double)z,
                                   (double)x + 1.0, (double)y + 0.5, (double)z + 1.0));
            } else {
              boxes.push_back(AABB((double)x, (double)y + 0.5, (double)z,
                                   (double)x + 1.0, (double)y + 1.0, (double)z + 1.0));
            }
            int facing = stairFacing(id);
            if (facing == 0) {
              if (!upsideDown) boxes.push_back(AABB((double)x, (double)y + 0.5, (double)z + 0.5,
                                                    (double)x + 1.0, (double)y + 1.0, (double)z + 1.0));
              else boxes.push_back(AABB((double)x, (double)y, (double)z + 0.5,
                                        (double)x + 1.0, (double)y + 0.5, (double)z + 1.0));
            } else if (facing == 1) {
              if (!upsideDown) boxes.push_back(AABB((double)x, (double)y + 0.5, (double)z,
                                                    (double)x + 0.5, (double)y + 1.0, (double)z + 1.0));
              else boxes.push_back(AABB((double)x, (double)y, (double)z,
                                        (double)x + 0.5, (double)y + 0.5, (double)z + 1.0));
            } else if (facing == 2) {
              if (!upsideDown) boxes.push_back(AABB((double)x, (double)y + 0.5, (double)z,
                                                    (double)x + 1.0, (double)y + 1.0, (double)z + 0.5));
              else boxes.push_back(AABB((double)x, (double)y, (double)z,
                                        (double)x + 1.0, (double)y + 0.5, (double)z + 0.5));
            } else {
              if (!upsideDown) boxes.push_back(AABB((double)x + 0.5, (double)y + 0.5, (double)z,
                                                    (double)x + 1.0, (double)y + 1.0, (double)z + 1.0));
              else boxes.push_back(AABB((double)x + 0.5, (double)y, (double)z,
                                        (double)x + 1.0, (double)y + 0.5, (double)z + 1.0));
            }
            continue;
          }
          const BlockProps &bp = g_blockProps[id];
          boxes.push_back(AABB((double)x + bp.minX, (double)y + bp.minY, (double)z + bp.minZ,
                               (double)x + bp.maxX, (double)y + bp.maxY, (double)z + bp.maxZ));
        }
      }
    }
  }
  return boxes;
}

uint8_t Level::getSkyLight(int wx, int wy, int wz) const {
  int cx = wx >> 4;
  int cz = wz >> 4;
  if (cx < 0 || cx >= WORLD_CHUNKS_X || cz < 0 || cz >= WORLD_CHUNKS_Z || wy < 0 || wy >= CHUNK_SIZE_Y) return 15;
  return m_chunks[cx][cz]->getSkyLight(wx & 0xF, wy, wz & 0xF);
}

uint8_t Level::getBlockLight(int wx, int wy, int wz) const {
  int cx = wx >> 4;
  int cz = wz >> 4;
  if (cx < 0 || cx >= WORLD_CHUNKS_X || cz < 0 || cz >= WORLD_CHUNKS_Z || wy < 0 || wy >= CHUNK_SIZE_Y) return 0;
  return m_chunks[cx][cz]->getBlockLight(wx & 0xF, wy, wz & 0xF);
}

void Level::setSkyLight(int wx, int wy, int wz, uint8_t val) {
  int cx = wx >> 4;
  int cz = wz >> 4;
  if (cx < 0 || cx >= WORLD_CHUNKS_X || cz < 0 || cz >= WORLD_CHUNKS_Z || wy < 0 || wy >= CHUNK_SIZE_Y) return;
  uint8_t curBlock = m_chunks[cx][cz]->getBlockLight(wx & 0xF, wy, wz & 0xF);
  m_chunks[cx][cz]->setLight(wx & 0xF, wy, wz & 0xF, val, curBlock);
}

void Level::setBlockLight(int wx, int wy, int wz, uint8_t val) {
  int cx = wx >> 4;
  int cz = wz >> 4;
  if (cx < 0 || cx >= WORLD_CHUNKS_X || cz < 0 || cz >= WORLD_CHUNKS_Z || wy < 0 || wy >= CHUNK_SIZE_Y) return;
  uint8_t curSky = m_chunks[cx][cz]->getSkyLight(wx & 0xF, wy, wz & 0xF);
  m_chunks[cx][cz]->setLight(wx & 0xF, wy, wz & 0xF, curSky, val);
}

bool Level::saveToFile(const char *path) const {
  if (!path) return false;
  FILE *f = fopen(path, "wb");
  if (!f) return false;

  struct SaveHeader {
    char magic[8];
    uint32_t version;
    uint32_t chunksX;
    uint32_t chunksZ;
    uint32_t chunkY;
    int64_t time;
    uint32_t waterSize;
  } hdr;

  memcpy(hdr.magic, "MCPSPWLD", 8);
  hdr.version = 3;
  hdr.chunksX = WORLD_CHUNKS_X;
  hdr.chunksZ = WORLD_CHUNKS_Z;
  hdr.chunkY = CHUNK_SIZE_Y;
  hdr.time = m_time;
  hdr.waterSize = (uint32_t)m_waterDepth.size();
  if (fwrite(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return false; }

  for (int cx = 0; cx < WORLD_CHUNKS_X; ++cx) {
    for (int cz = 0; cz < WORLD_CHUNKS_Z; ++cz) {
      const Chunk *ch = m_chunks[cx][cz];
      if (fwrite(ch->blocks, sizeof(ch->blocks), 1, f) != 1) { fclose(f); return false; }
      if (fwrite(ch->light, sizeof(ch->light), 1, f) != 1) { fclose(f); return false; }
    }
  }

  if (!m_waterDepth.empty()) {
    if (fwrite(m_waterDepth.data(), 1, m_waterDepth.size(), f) != m_waterDepth.size()) {
      fclose(f);
      return false;
    }
  }
  if (!m_lavaDepth.empty()) {
    if (fwrite(m_lavaDepth.data(), 1, m_lavaDepth.size(), f) != m_lavaDepth.size()) {
      fclose(f);
      return false;
    }
  }

  fflush(f);
  fclose(f);
  return true;
}

bool Level::loadFromFile(const char *path) {
  if (!path) return false;
  FILE *f = fopen(path, "rb");
  if (!f) return false;

  struct SaveHeader {
    char magic[8];
    uint32_t version;
    uint32_t chunksX;
    uint32_t chunksZ;
    uint32_t chunkY;
    int64_t time;
    uint32_t waterSize;
  } hdr;

  if (fread(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return false; }
  if (memcmp(hdr.magic, "MCPSPWLD", 8) != 0 || (hdr.version != 1 && hdr.version != 2 && hdr.version != 3)) { fclose(f); return false; }
  if (hdr.chunksX != WORLD_CHUNKS_X || hdr.chunksZ != WORLD_CHUNKS_Z || hdr.chunkY != CHUNK_SIZE_Y) { fclose(f); return false; }
  if (hdr.waterSize != m_waterDepth.size()) { fclose(f); return false; }

  for (int cx = 0; cx < WORLD_CHUNKS_X; ++cx) {
    for (int cz = 0; cz < WORLD_CHUNKS_Z; ++cz) {
      Chunk *ch = m_chunks[cx][cz];
      ch->cx = cx;
      ch->cz = cz;
      if (fread(ch->blocks, sizeof(ch->blocks), 1, f) != 1) { fclose(f); return false; }
      if (fread(ch->light, sizeof(ch->light), 1, f) != 1) { fclose(f); return false; }
      for (int sy = 0; sy < (CHUNK_SIZE_Y / 16); ++sy) ch->dirty[sy] = true;
    }
  }

  if (!m_waterDepth.empty()) {
    if (fread(m_waterDepth.data(), 1, m_waterDepth.size(), f) != m_waterDepth.size()) {
      fclose(f);
      return false;
    }
  }
  if (hdr.version >= 3 && !m_lavaDepth.empty()) {
    if (fread(m_lavaDepth.data(), 1, m_lavaDepth.size(), f) != m_lavaDepth.size()) {
      fclose(f);
      return false;
    }
  }

  fclose(f);
  m_time = hdr.time;
  while (!m_waterTicks.empty()) m_waterTicks.pop();
  while (!m_lavaTicks.empty()) m_lavaTicks.pop();
  std::fill(m_waterDue.begin(), m_waterDue.end(), -1);
  std::fill(m_lavaDue.begin(), m_lavaDue.end(), -1);
  for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
    for (int z = 0; z < WORLD_CHUNKS_Z * CHUNK_SIZE_Z; ++z) {
      for (int x = 0; x < WORLD_CHUNKS_X * CHUNK_SIZE_X; ++x) {
        uint8_t id = getBlock(x, y, z);
        if (isWaterBlock(id)) scheduleWaterTick(x, y, z, 1);
        else if (isLavaBlock(id)) {
          if (hdr.version < 3) m_lavaDepth[waterIndex(x, y, z)] = (id == BLOCK_LAVA_STILL) ? 0 : 1;
          scheduleLavaTick(x, y, z, 4);
        } else if (hdr.version < 3) {
          m_lavaDepth[waterIndex(x, y, z)] = 0xFF;
        }
      }
    }
  }
  m_waterDirty = !m_waterTicks.empty();
  m_lavaDirty = !m_lavaTicks.empty();
  return true;
}

void Level::generate(Random *rng, ProgressCallback callback, void* userdata) {
  int64_t seed = rng->nextLong();

  // Phase 1: Terrain generation (0-50%)
  if (callback) callback(0.1f, "Generating terrain", userdata);
  for (int cx = 0; cx < WORLD_CHUNKS_X; cx++) {
    for (int cz = 0; cz < WORLD_CHUNKS_Z; cz++) {
      Chunk *c = m_chunks[cx][cz];
      c->cx = cx;
      c->cz = cz;
      WorldGen::generateChunk(c->blocks, cx, cz, seed);
      for(int i=0; i<(CHUNK_SIZE_Y / 16); i++) c->dirty[i] = true;
    }
    if (callback) callback(0.1f + 0.4f * ((float)(cx+1) / WORLD_CHUNKS_X), "Generating terrain", userdata);
  }

  // Phase 2: Tree placement (50-70%)
  if (callback) callback(0.51f, "Placing trees", userdata);
  for (int cx = 0; cx < WORLD_CHUNKS_X; cx++) {
    for (int cz = 0; cz < WORLD_CHUNKS_Z; cz++) {
      Random chunkRng(seed ^ ((int64_t)cx * 341873128712LL) ^ ((int64_t)cz * 132897987541LL));
      for (int i = 0; i < 3; i++) {
        int lx = chunkRng.nextInt(CHUNK_SIZE_X);
        int lz = chunkRng.nextInt(CHUNK_SIZE_Z);
        int wx = cx * CHUNK_SIZE_X + lx;
        int wz = cz * CHUNK_SIZE_Z + lz;

        int wy = CHUNK_SIZE_Y - 1;
        while (wy > 0 && getBlock(wx, wy, wz) == BLOCK_AIR) wy--;

        if (wy > 50 && getBlock(wx, wy, wz) == BLOCK_GRASS) {
          setBlock(wx, wy, wz, BLOCK_DIRT);
          TreeFeature::place(this, wx, wy + 1, wz, chunkRng);
        }
      }
    }
    if (callback) callback(0.51f + 0.19f * ((float)(cx+1) / WORLD_CHUNKS_X), "Placing trees", userdata);
  }

  for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
    for (int z = 0; z < WORLD_CHUNKS_Z * CHUNK_SIZE_Z; ++z) {
      for (int x = 0; x < WORLD_CHUNKS_X * CHUNK_SIZE_X; ++x) {
        uint8_t id = getBlock(x, y, z);
        if (id == BLOCK_WATER_STILL) setWaterDepth(x, y, z, 0);
        else if (id == BLOCK_WATER_FLOW) setWaterDepth(x, y, z, 1);
        else setWaterDepth(x, y, z, 0xFF);
        if (id == BLOCK_LAVA_STILL) m_lavaDepth[waterIndex(x, y, z)] = 0;
        else if (id == BLOCK_LAVA_FLOW) m_lavaDepth[waterIndex(x, y, z)] = 1;
        else m_lavaDepth[waterIndex(x, y, z)] = 0xFF;
      }
    }
  }
  while (!m_waterTicks.empty()) m_waterTicks.pop();
  while (!m_lavaTicks.empty()) m_lavaTicks.pop();
  std::fill(m_waterDue.begin(), m_waterDue.end(), -1);
  std::fill(m_lavaDue.begin(), m_lavaDue.end(), -1);

  computeLighting();
  if (callback) callback(1.0f, "Simulating world", userdata);
}

void Level::computeLighting() {
  std::vector<LightNode> lightQ;
  lightQ.reserve(65536);

  // 1. Sunlight
  for (int x = 0; x < WORLD_CHUNKS_X * CHUNK_SIZE_X; x++) {
    for (int z = 0; z < WORLD_CHUNKS_Z * CHUNK_SIZE_Z; z++) {
      int curLight = 15;
      for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
        uint8_t id = getBlock(x, y, z);
        if (id != BLOCK_AIR) {
           const BlockProps &bp = g_blockProps[id];
           if (blocksLightFully(id)) curLight = 0;
           else if (id == BLOCK_LEAVES) curLight = (curLight >= 2) ? curLight - 2 : 0;
           else if (bp.isLiquid()) curLight = (curLight >= 3) ? curLight - 3 : 0;
        }
        setSkyLight(x, y, z, curLight);
      }
    }
  }

  // 2. Queue borders
  for (int x = 0; x < WORLD_CHUNKS_X * CHUNK_SIZE_X; x++) {
    for (int z = 0; z < WORLD_CHUNKS_Z * CHUNK_SIZE_Z; z++) {
      for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
        if (getSkyLight(x, y, z) == 15) {
           bool needsSpread = false;
           const int dx[] = {-1, 1, 0, 0, 0, 0};
           const int dy[] = {0, 0, -1, 1, 0, 0};
           const int dz[] = {0, 0, 0, 0, -1, 1};
           for(int i = 0; i < 6; i++) {
             int nx = x + dx[i], ny = y + dy[i], nz = z + dz[i];
             if (ny >= 0 && ny < CHUNK_SIZE_Y && nx >= 0 && nx < WORLD_CHUNKS_X * CHUNK_SIZE_X && nz >= 0 && nz < WORLD_CHUNKS_Z * CHUNK_SIZE_Z) {
                 if (getSkyLight(nx, ny, nz) < 15 && !blocksLightFully(getBlock(nx, ny, nz))) {
                     needsSpread = true;
                     break;
                 }
             }
           }
           if (needsSpread) lightQ.push_back({x, y, z});
        }
      }
    }
  }

  // 3. Sky light flood fill
  int head = 0;
  while (head < (int)lightQ.size()) {
    LightNode node = lightQ[head++];
    uint8_t level = getSkyLight(node.x, node.y, node.z);
    if (level <= 1) continue;

    const int dx[] = {-1, 1, 0, 0, 0, 0};
    const int dy[] = {0, 0, -1, 1, 0, 0};
    const int dz[] = {0, 0, 0, 0, -1, 1};

    for (int i = 0; i < 6; i++) {
      int nx = node.x + dx[i];
      int ny = node.y + dy[i];
      int nz = node.z + dz[i];

      if (ny < 0 || ny >= CHUNK_SIZE_Y || nx < 0 || nz < 0 || nx >= WORLD_CHUNKS_X * CHUNK_SIZE_X || nz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z) continue;
      uint8_t neighborId = getBlock(nx, ny, nz);
      if (blocksLightFully(neighborId)) continue;
      int attenuation = lightAttenuationFor(neighborId);

      int neighborLevel = getSkyLight(nx, ny, nz);
      if (level - attenuation > neighborLevel) {
        setSkyLight(nx, ny, nz, level - attenuation);
        lightQ.push_back({nx, ny, nz});
      }
    }
  }

  lightQ.clear();

  // 4. Block light sources
  for (int cx = 0; cx < WORLD_CHUNKS_X; cx++) {
    for (int cz = 0; cz < WORLD_CHUNKS_Z; cz++) {
      for (int lx = 0; lx < CHUNK_SIZE_X; lx++) {
        for (int lz = 0; lz < CHUNK_SIZE_Z; lz++) {
          for (int ly = 0; ly < CHUNK_SIZE_Y; ly++) {
            int wx = cx * CHUNK_SIZE_X + lx;
            int wz = cz * CHUNK_SIZE_Z + lz;
            uint8_t id = m_chunks[cx][cz]->blocks[lx][lz][ly];
            if (id == BLOCK_LAVA_STILL || id == BLOCK_LAVA_FLOW || id == BLOCK_GLOWSTONE) {
              setBlockLight(wx, ly, wz, 15);
              lightQ.push_back({wx, ly, wz});
            } else {
              setBlockLight(wx, ly, wz, 0);
            }
          }
        }
      }
    }
  }

  head = 0;
  while (head < (int)lightQ.size()) {
    LightNode node = lightQ[head++];
    uint8_t level = getBlockLight(node.x, node.y, node.z);
    if (level <= 1) continue;

    const int dx[] = {-1, 1, 0, 0, 0, 0};
    const int dy[] = {0, 0, -1, 1, 0, 0};
    const int dz[] = {0, 0, 0, 0, -1, 1};

    for (int i = 0; i < 6; i++) {
      int nx = node.x + dx[i];
      int ny = node.y + dy[i];
      int nz = node.z + dz[i];

      if (ny < 0 || ny >= CHUNK_SIZE_Y || nx < 0 || nz < 0 || nx >= WORLD_CHUNKS_X * CHUNK_SIZE_X || nz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z) continue;
      uint8_t neighborId = getBlock(nx, ny, nz);
      if (blocksLightFully(neighborId)) continue;
      int attenuation = lightAttenuationFor(neighborId);

      int neighborLevel = getBlockLight(nx, ny, nz);
      if (level - attenuation > neighborLevel) {
        setBlockLight(nx, ny, nz, level - attenuation);
        lightQ.push_back({nx, ny, nz});
      }
    }
  }
}

struct LightRemovalNode {
    short x, y, z;
    uint8_t val;
};

void Level::updateLight(int wx, int wy, int wz) {
  uint8_t id = getBlock(wx, wy, wz);
  uint8_t oldBlockLight = getBlockLight(wx, wy, wz);
  uint8_t newBlockLight = g_blockProps[id].light_emit;
  
  const int dx[] = {-1, 1, 0, 0, 0, 0};
  const int dy[] = {0, 0, -1, 1, 0, 0};
  const int dz[] = {0, 0, 0, 0, -1, 1};
  
  uint8_t maxNeighborLight = 0;
  for(int i=0; i<6; i++) {
    int nx = wx + dx[i];
    int ny = wy + dy[i];
    int nz = wz + dz[i];
    if (ny < 0 || ny >= CHUNK_SIZE_Y || nx < 0 || nz < 0 || nx >= WORLD_CHUNKS_X * CHUNK_SIZE_X || nz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z) continue;
    uint8_t nl = getBlockLight(nx, ny, nz);
    if(nl > maxNeighborLight) maxNeighborLight = nl;
  }

  uint8_t blockAtten = (uint8_t)lightAttenuationFor(id);
  uint8_t expectedBlockLight = newBlockLight;
  // If light is being removed at this voxel (e.g. breaking an emitter),
  // start from intrinsic emission only to avoid keeping stale light.
  // Otherwise (e.g. opening a wall), allow relight from neighboring light.
  if (oldBlockLight <= newBlockLight) {
    if (maxNeighborLight > blockAtten && (maxNeighborLight - blockAtten) > expectedBlockLight) {
      expectedBlockLight = maxNeighborLight - blockAtten;
    }
  }
  updateBlockLight(wx, wy, wz, oldBlockLight, expectedBlockLight);

  uint8_t oldSkyLight = getSkyLight(wx, wy, wz);
  uint8_t expectedSkyLight = 0;
  if (wy == CHUNK_SIZE_Y - 1) {
      expectedSkyLight = blockAtten < 15 ? 15 : 0;
  } else if (getSkyLight(wx, wy + 1, wz) == 15 && blockAtten < 15) {
      expectedSkyLight = 15;
  } else {
      uint8_t maxNeighborSkyLight = 0;
      for(int i=0; i<6; i++) {
        int nx = wx + dx[i];
        int ny = wy + dy[i];
        int nz = wz + dz[i];
        if (ny < 0 || ny >= CHUNK_SIZE_Y || nx < 0 || nz < 0 || nx >= WORLD_CHUNKS_X * CHUNK_SIZE_X || nz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z) continue;
        uint8_t nl = getSkyLight(nx, ny, nz);
        if(nl > maxNeighborSkyLight) maxNeighborSkyLight = nl;
      }
      if (maxNeighborSkyLight > blockAtten) expectedSkyLight = maxNeighborSkyLight - blockAtten;
  }
  updateSkyLight(wx, wy, wz, oldSkyLight, expectedSkyLight);
}

void Level::updateBlockLight(int wx, int wy, int wz, uint8_t oldLight, uint8_t newLight) {
    if (oldLight == newLight) return;

    std::vector<LightRemovalNode> darkQ;
    std::vector<LightNode> lightQ;
    darkQ.reserve(65536);
    lightQ.reserve(65536);
    int darkHead = 0;
    int lightHead = 0;

    const int dx[] = {-1, 1, 0, 0, 0, 0};
    const int dy[] = {0, 0, -1, 1, 0, 0};
    const int dz[] = {0, 0, 0, 0, -1, 1};

    if (oldLight > newLight) {
        darkQ.push_back({(short)wx, (short)wy, (short)wz, oldLight});
        setBlockLight(wx, wy, wz, 0);
    } else {
        setBlockLight(wx, wy, wz, newLight);
        lightQ.push_back({(short)wx, (short)wy, (short)wz});
    }

    while (darkHead < (int)darkQ.size()) {
        LightRemovalNode node = darkQ[darkHead++];
        int x = node.x, y = node.y, z = node.z;
        uint8_t level = node.val;

        for (int i = 0; i < 6; i++) {
            int nx = x + dx[i], ny = y + dy[i], nz = z + dz[i];
            if (ny < 0 || ny >= CHUNK_SIZE_Y || nx < 0 || nz < 0 || nx >= WORLD_CHUNKS_X * CHUNK_SIZE_X || nz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z) continue;

            uint8_t neighborLevel = getBlockLight(nx, ny, nz);
            if (neighborLevel == 0) continue;

            uint8_t nid = getBlock(nx, ny, nz);
            uint8_t neighborEmit = g_blockProps[nid].light_emit;

            // Dark-pass rule:
            // - remove lower propagated light levels (likely fed by the removed path)
            // - keep intrinsic emitters at their own emission level
            if (neighborLevel < level && neighborLevel != neighborEmit) {
                setBlockLight(nx, ny, nz, 0);
                darkQ.push_back({(short)nx, (short)ny, (short)nz, neighborLevel});
            } else {
                lightQ.push_back({(short)nx, (short)ny, (short)nz});
            }
        }
    }

    if (newLight > 0 && oldLight > newLight) {
        setBlockLight(wx, wy, wz, newLight);
        lightQ.push_back({(short)wx, (short)wy, (short)wz});
    }

    while (lightHead < (int)lightQ.size()) {
        LightNode node = lightQ[lightHead++];
        int x = node.x, y = node.y, z = node.z;
        uint8_t level = getBlockLight(x, y, z);
        if (level <= 1) continue;
        
        for (int i = 0; i < 6; i++) {
            int nx = x + dx[i], ny = y + dy[i], nz = z + dz[i];
            if (ny < 0 || ny >= CHUNK_SIZE_Y || nx < 0 || nz < 0 || nx >= WORLD_CHUNKS_X * CHUNK_SIZE_X || nz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z) continue;
            
            uint8_t id = getBlock(nx, ny, nz);
            int attenuation = lightAttenuationFor(id);
            int neighborLevel = getBlockLight(nx, ny, nz);
            
            if (level - attenuation > neighborLevel) {
                setBlockLight(nx, ny, nz, level - attenuation);
                lightQ.push_back({(short)nx, (short)ny, (short)nz});
            }
        }
    }
}

void Level::updateSkyLight(int wx, int wy, int wz, uint8_t oldLight, uint8_t newLight) {
    if (oldLight == newLight) return;
    
    static LightRemovalNode darkQ[65536];
    static LightNode lightQ[65536];
    int darkHead = 0, darkTail = 0;
    int lightHead = 0, lightTail = 0;

    const int dx[] = {-1, 1, 0, 0, 0, 0};
    const int dy[] = {0, 0, -1, 1, 0, 0};
    const int dz[] = {0, 0, 0, 0, -1, 1};

    if (oldLight > newLight) {
        darkQ[darkTail++] = {(short)wx, (short)wy, (short)wz, oldLight};
        setSkyLight(wx, wy, wz, 0);
    } else {
        lightQ[lightTail++] = {(short)wx, (short)wy, (short)wz};
        setSkyLight(wx, wy, wz, newLight);
    }

    while (darkHead < darkTail) {
        LightRemovalNode node = darkQ[darkHead++];
        int x = node.x, y = node.y, z = node.z;
        uint8_t level = node.val;

        for (int i = 0; i < 6; i++) {
            int nx = x + dx[i], ny = y + dy[i], nz = z + dz[i];
            if (ny < 0 || ny >= CHUNK_SIZE_Y || nx < 0 || nz < 0 || nx >= WORLD_CHUNKS_X * CHUNK_SIZE_X || nz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z) continue;
            
            uint8_t neighborLevel = getSkyLight(nx, ny, nz);
            
            if (neighborLevel != 0 && ((dy[i] == -1 && level == 15 && neighborLevel == 15) || neighborLevel < level)) {
                setSkyLight(nx, ny, nz, 0);
                darkQ[darkTail++ & 0xFFFF] = {(short)nx, (short)ny, (short)nz, neighborLevel};
            } else if (neighborLevel >= level) {
                lightQ[lightTail++ & 0xFFFF] = {(short)nx, (short)ny, (short)nz};
            }
        }
    }

    while (lightHead < lightTail) {
        LightNode node = lightQ[lightHead++];
        int x = node.x, y = node.y, z = node.z;
        uint8_t level = getSkyLight(x, y, z);
        
        for (int i = 0; i < 6; i++) {
            int nx = x + dx[i], ny = y + dy[i], nz = z + dz[i];
            if (ny < 0 || ny >= CHUNK_SIZE_Y || nx < 0 || nz < 0 || nx >= WORLD_CHUNKS_X * CHUNK_SIZE_X || nz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z) continue;
            
            uint8_t id = getBlock(nx, ny, nz);
            int attenuation = lightAttenuationFor(id);
            if (dy[i] == -1 && level == 15 && attenuation < 15) attenuation = 0;
            
            int neighborLevel = getSkyLight(nx, ny, nz);
            if (level - attenuation > neighborLevel) {
                setSkyLight(nx, ny, nz, level - attenuation);
                lightQ[lightTail++ & 0xFFFF] = {(short)nx, (short)ny, (short)nz};
            }
        }
    }
}
