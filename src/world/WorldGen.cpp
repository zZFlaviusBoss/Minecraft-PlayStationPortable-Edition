// WorldGen.cpp

#include "WorldGen.h"
#include "Blocks.h"
#include "NoiseGen.h"
#include "Random.h"
#include "TreeFeature.h"
#include "chunk_defs.h"
#include <string.h>

// Get terrain height
int WorldGen::getTerrainHeight(int wx, int wz, int64_t seed) {
  // 4 noise octaves, 64 block scale
  float n = NoiseGen::octaveNoise(wx / 64.0f, wz / 64.0f, seed);
  // Range: 40..60 (base level 50 = sea level)
  return 40 + (int)(n * 20.0f);
}

// Generate chunk
void WorldGen::generateChunk(
    uint8_t out[CHUNK_SIZE_X][CHUNK_SIZE_Z][CHUNK_SIZE_Y], int cx, int cz,
    int64_t worldSeed) {

  memset(out, BLOCK_AIR, CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y);

  Random rng(worldSeed ^ ((int64_t)cx * 341873128712LL) ^
             ((int64_t)cz * 132897987541LL));

  // === Base Terrain ===
  for (int lx = 0; lx < CHUNK_SIZE_X; lx++) {
    for (int lz = 0; lz < CHUNK_SIZE_Z; lz++) {
      int wx = cx * CHUNK_SIZE_X + lx;
      int wz = cz * CHUNK_SIZE_Z + lz;

      int surfaceY = getTerrainHeight(wx, wz, worldSeed);
      if (surfaceY >= CHUNK_SIZE_Y)
        surfaceY = CHUNK_SIZE_Y - 1;

      for (int y = 0; y <= surfaceY; y++) {
        uint8_t block;

        if (y == 0) {
          block = BLOCK_BEDROCK;
        } else if (y < surfaceY - 4) {
          block = BLOCK_STONE;
        } else if (y < surfaceY) {
          block = BLOCK_DIRT;
        } else {
          block = BLOCK_GRASS;
        }
        out[lx][lz][y] = block;
      }

      // Water at sea level (50) if surface is below it
      if (surfaceY < 50) {
        for (int y = surfaceY + 1; y <= 50; y++) {
          if (y < CHUNK_SIZE_Y)
            out[lx][lz][y] = BLOCK_WATER_STILL;
        }
      }
    }
  }

  // === Vegetation ===
  int xo = cx * CHUNK_SIZE_X;
  int zo = cz * CHUNK_SIZE_Z;

  // 1-2 grass clusters per chunk
  int grassClusters = 1 + rng.nextInt(2);
  for (int i = 0; i < grassClusters; i++) {
    int x = xo + rng.nextInt(16);
    int z = zo + rng.nextInt(16);
    int y = rng.nextInt(CHUNK_SIZE_Y);
    
    // TallGrassFeature spreads 128 times around the center
    for (int j = 0; j < 128; j++) {
      int x2 = x + rng.nextInt(8) - rng.nextInt(8);
      int y2 = y + rng.nextInt(4) - rng.nextInt(4);
      int z2 = z + rng.nextInt(8) - rng.nextInt(8);
      
      int lx = x2 - xo;
      int ly = y2;
      int lz = z2 - zo;
      
      if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z && ly > 0 && ly < CHUNK_SIZE_Y) {
        if (out[lx][lz][ly] == BLOCK_AIR && out[lx][lz][ly - 1] == BLOCK_GRASS) {
          out[lx][lz][ly] = BLOCK_TALLGRASS;
        }
      }
    }
  }

  // Flowers (2 clusters)
  for (int i = 0; i < 2; i++) {
    int x = xo + rng.nextInt(16);
    int z = zo + rng.nextInt(16);
    int y = rng.nextInt(CHUNK_SIZE_Y);
    
    // FlowerFeature spreads 64 times
    for (int j = 0; j < 64; j++) {
      int x2 = x + rng.nextInt(8) - rng.nextInt(8);
      int y2 = y + rng.nextInt(4) - rng.nextInt(4);
      int z2 = z + rng.nextInt(8) - rng.nextInt(8);
      
      int lx = x2 - xo;
      int ly = y2;
      int lz = z2 - zo;
      
      if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z && ly > 0 && ly < CHUNK_SIZE_Y) {
        if (out[lx][lz][ly] == BLOCK_AIR && out[lx][lz][ly - 1] == BLOCK_GRASS) {
          out[lx][lz][ly] = BLOCK_FLOWER;
        }
      }
    }
    
    // Rose (25% chance of second flower patch being red)
    if (rng.nextInt(4) == 0) {
      x = xo + rng.nextInt(16);
      z = zo + rng.nextInt(16);
      y = rng.nextInt(CHUNK_SIZE_Y);
      for (int j = 0; j < 64; j++) {
        int x2 = x + rng.nextInt(8) - rng.nextInt(8);
        int y2 = y + rng.nextInt(4) - rng.nextInt(4);
        int z2 = z + rng.nextInt(8) - rng.nextInt(8);
        
        int lx = x2 - xo;
        int ly = y2;
        int lz = z2 - zo;
        
        if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z && ly > 0 && ly < CHUNK_SIZE_Y) {
          if (out[lx][lz][ly] == BLOCK_AIR && out[lx][lz][ly - 1] == BLOCK_GRASS) {
            out[lx][lz][ly] = BLOCK_ROSE;
          }
        }
      }
    }
  }
}

