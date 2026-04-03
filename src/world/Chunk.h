#pragma once
#include "Blocks.h"
#include "chunk_defs.h"
#include <stdint.h>

// Vertex format for sceGu (texture + color)
struct CraftPSPVertex {
  float u, v;
  uint32_t color;
  float x, y, z;
};

struct Chunk {
  uint8_t blocks[CHUNK_SIZE_X][CHUNK_SIZE_Z][CHUNK_SIZE_Y];
  uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Z][CHUNK_SIZE_Y];
  int cx, cz;

  CraftPSPVertex *opaqueVertices[4];    // Sky-lit faces – dimmed via sceGuAmbient(sunBrightness)
  CraftPSPVertex *transVertices[4];     // Transparent (outer leaves, glass)
  CraftPSPVertex *transFancyVertices[4];// Fancy inner leaves
  CraftPSPVertex *emitVertices[4];      // Block-lit (torch) faces – always full brightness
  int opaqueTriCount[4];
  int transTriCount[4];
  int transFancyTriCount[4];
  int emitTriCount[4];
  int opaqueCapacity[4];
  int transCapacity[4];
  int transFancyCapacity[4];
  int emitCapacity[4];
  bool dirty[4]; // Track dirty state per Sub-Chunk
  uint8_t priority[4]; // Determines async queue priority. High = compile first

  Chunk();
  ~Chunk();

  uint8_t getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, uint8_t id);
  uint8_t getSkyLight(int x, int y, int z) const;
  uint8_t getBlockLight(int x, int y, int z) const;
  void setLight(int x, int y, int z, uint8_t sky, uint8_t block);
};
