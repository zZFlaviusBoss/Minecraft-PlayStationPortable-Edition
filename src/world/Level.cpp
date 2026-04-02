#include "Level.h"
#include "Random.h"
#include "WorldGen.h"
#include "TreeFeature.h"
#include <vector>
#include <string.h>

struct LightNode {
  int x, y, z;
};

Level::Level() {
  memset(m_chunks, 0, sizeof(m_chunks));
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
  if (cx >= 0 && cx < WORLD_CHUNKS_X && cz >= 0 && cz < WORLD_CHUNKS_Z && sy >= 0 && sy < 4) {
    if (priority > m_chunks[cx][cz]->priority[sy]) m_chunks[cx][cz]->priority[sy] = priority;
    m_chunks[cx][cz]->dirty[sy] = true;
    
    if (spreadNeighbors) {
      // Dirty neighbor subchunks
      int lx = wx & 0xF;
      int lz = wz & 0xF;
      int ly = wy & 0xF;
      
      auto markNeighbor = [&](int ncx, int ncz, int nsy) {
        if (ncx >= 0 && ncx < WORLD_CHUNKS_X && ncz >= 0 && ncz < WORLD_CHUNKS_Z && nsy >= 0 && nsy < 4) {
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

void Level::setBlock(int wx, int wy, int wz, uint8_t id) {
  int cx = wx >> 4;
  int cz = wz >> 4;
  if (cx < 0 || cx >= WORLD_CHUNKS_X || cz < 0 || cz >= WORLD_CHUNKS_Z || wy < 0 || wy >= CHUNK_SIZE_Y) return;
  uint8_t oldId = m_chunks[cx][cz]->getBlock(wx & 0xF, wy, wz & 0xF);
  if (oldId == id) return;
  
  m_chunks[cx][cz]->setBlock(wx & 0xF, wy, wz & 0xF, id);
  updateLight(wx, wy, wz);
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
          // Create bounding box
          boxes.push_back(AABB((double)x, (double)y, (double)z,
                               (double)(x + 1), (double)(y + 1), (double)(z + 1)));
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

void Level::generate(Random *rng) {
  int64_t seed = rng->nextLong();

  for (int cx = 0; cx < WORLD_CHUNKS_X; cx++) {
    for (int cz = 0; cz < WORLD_CHUNKS_Z; cz++) {
      Chunk *c = m_chunks[cx][cz];
      c->cx = cx;
      c->cz = cz;
      WorldGen::generateChunk(c->blocks, cx, cz, seed);
      for(int i=0; i<4; i++) c->dirty[i] = true;
    }
  }

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
  }

  computeLighting();
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
           if (bp.isOpaque()) curLight = 0;
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
                 if (getSkyLight(nx, ny, nz) < 15 && !g_blockProps[getBlock(nx, ny, nz)].isOpaque()) {
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
      if (g_blockProps[neighborId].isOpaque()) continue;

      int attenuation = 1;
      if (neighborId == BLOCK_LEAVES) attenuation = 2;
      else if (neighborId == BLOCK_WATER_STILL || neighborId == BLOCK_WATER_FLOW || neighborId == BLOCK_LAVA_STILL || neighborId == BLOCK_LAVA_FLOW) attenuation = 3;

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
      if (g_blockProps[neighborId].isOpaque()) continue;

      int attenuation = 1;
      if (neighborId == BLOCK_LEAVES) attenuation = 2;
      else if (neighborId == BLOCK_WATER_STILL || neighborId == BLOCK_WATER_FLOW || neighborId == BLOCK_LAVA_STILL || neighborId == BLOCK_LAVA_FLOW) attenuation = 3;

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
  
  uint8_t blockAtten = g_blockProps[id].isOpaque() ? 15 : ((id == BLOCK_LEAVES) ? 2 : (g_blockProps[id].isLiquid() ? 3 : 1));
  uint8_t expectedBlockLight = newBlockLight;
  if (maxNeighborLight > blockAtten && (maxNeighborLight - blockAtten) > expectedBlockLight) {
      expectedBlockLight = maxNeighborLight - blockAtten;
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
    
    static LightRemovalNode darkQ[65536];
    static LightNode lightQ[65536];
    int darkHead = 0, darkTail = 0;
    int lightHead = 0, lightTail = 0;

    const int dx[] = {-1, 1, 0, 0, 0, 0};
    const int dy[] = {0, 0, -1, 1, 0, 0};
    const int dz[] = {0, 0, 0, 0, -1, 1};

    if (oldLight > newLight) {
        darkQ[darkTail++] = {(short)wx, (short)wy, (short)wz, oldLight};
        setBlockLight(wx, wy, wz, 0);
    } else {
        lightQ[lightTail++] = {(short)wx, (short)wy, (short)wz};
        setBlockLight(wx, wy, wz, newLight);
    }

    while (darkHead < darkTail) {
        LightRemovalNode node = darkQ[darkHead++];
        int x = node.x, y = node.y, z = node.z;
        uint8_t level = node.val;

        for (int i = 0; i < 6; i++) {
            int nx = x + dx[i], ny = y + dy[i], nz = z + dz[i];
            if (ny < 0 || ny >= CHUNK_SIZE_Y || nx < 0 || nz < 0 || nx >= WORLD_CHUNKS_X * CHUNK_SIZE_X || nz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z) continue;
            
            uint8_t neighborLevel = getBlockLight(nx, ny, nz);
            if (neighborLevel != 0 && neighborLevel < level) {
                setBlockLight(nx, ny, nz, 0);
                // Mask array index
            }
        }
    }

    while (lightHead < lightTail) {
        LightNode node = lightQ[lightHead++];
        int x = node.x, y = node.y, z = node.z;
        uint8_t level = getBlockLight(x, y, z);
        
        for (int i = 0; i < 6; i++) {
            int nx = x + dx[i], ny = y + dy[i], nz = z + dz[i];
            if (ny < 0 || ny >= CHUNK_SIZE_Y || nx < 0 || nz < 0 || nx >= WORLD_CHUNKS_X * CHUNK_SIZE_X || nz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z) continue;
            
            uint8_t id = getBlock(nx, ny, nz);
            const BlockProps& bp = g_blockProps[id];
            
            int attenuation = bp.isOpaque() ? 15 : ((id == BLOCK_LEAVES) ? 2 : (bp.isLiquid() ? 3 : 1));
            int neighborLevel = getBlockLight(nx, ny, nz);
            
            if (level - attenuation > neighborLevel) {
                setBlockLight(nx, ny, nz, level - attenuation);
                lightQ[lightTail++ & 0xFFFF] = {(short)nx, (short)ny, (short)nz};
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
            const BlockProps& bp = g_blockProps[id];
            
            int attenuation = bp.isOpaque() ? 15 : ((id == BLOCK_LEAVES) ? 2 : (bp.isLiquid() ? 3 : 1));
            if (dy[i] == -1 && level == 15 && attenuation < 15) attenuation = 0;
            
            int neighborLevel = getSkyLight(nx, ny, nz);
            if (level - attenuation > neighborLevel) {
                setSkyLight(nx, ny, nz, level - attenuation);
                lightQ[lightTail++ & 0xFFFF] = {(short)nx, (short)ny, (short)nz};
            }
        }
    }
}
