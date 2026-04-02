#include "Chunk.h"
#include <string.h>
#include <malloc.h>
#include <stdlib.h>

Chunk::Chunk() : cx(0), cz(0) {
  for(int i=0; i<4; i++) {
    dirty[i] = true;
    opaqueVertices[i] = nullptr;
    transVertices[i] = nullptr;
    transFancyVertices[i] = nullptr;
    emitVertices[i] = nullptr;
    opaqueTriCount[i] = 0;
    transTriCount[i] = 0;
    transFancyTriCount[i] = 0;
    emitTriCount[i] = 0;
    opaqueCapacity[i] = 0;
    transCapacity[i] = 0;
    transFancyCapacity[i] = 0;
    emitCapacity[i] = 0;
    priority[i] = 0;
  }
  memset(blocks, BLOCK_AIR, sizeof(blocks));
  memset(light, 0, sizeof(light));
}

Chunk::~Chunk() {
  for(int i=0; i<4; i++) {
    if (opaqueVertices[i]) { free(opaqueVertices[i]); opaqueVertices[i] = nullptr; }
    if (transVertices[i]) { free(transVertices[i]); transVertices[i] = nullptr; }
    if (transFancyVertices[i]) { free(transFancyVertices[i]); transFancyVertices[i] = nullptr; }
    if (emitVertices[i]) { free(emitVertices[i]); emitVertices[i] = nullptr; }
  }
}

uint8_t Chunk::getBlock(int x, int y, int z) const {
  if (x < 0 || x >= CHUNK_SIZE_X)
    return BLOCK_AIR;
  if (y < 0 || y >= CHUNK_SIZE_Y)
    return BLOCK_AIR;
  if (z < 0 || z >= CHUNK_SIZE_Z)
    return BLOCK_AIR;
  return blocks[x][z][y];
}

void Chunk::setBlock(int x, int y, int z, uint8_t id) {
  if (x < 0 || x >= CHUNK_SIZE_X)
    return;
  if (y < 0 || y >= CHUNK_SIZE_Y)
    return;
  if (z < 0 || z >= CHUNK_SIZE_Z)
    return;
  blocks[x][z][y] = id;
  int sy = y / 16;
  dirty[sy] = true;
  if (y % 16 == 0 && sy > 0) dirty[sy - 1] = true;
  if (y % 16 == 15 && sy < 3) dirty[sy + 1] = true;
}

uint8_t Chunk::getSkyLight(int x, int y, int z) const {
  if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z)
    return 15;
  return (light[x][z][y] >> 4) & 0x0F;
}

uint8_t Chunk::getBlockLight(int x, int y, int z) const {
  if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z)
    return 0;
  return light[x][z][y] & 0x0F;
}

void Chunk::setLight(int x, int y, int z, uint8_t sky, uint8_t block) {
  if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z)
    return;
  light[x][z][y] = ((sky & 0x0F) << 4) | (block & 0x0F);
  int sy = y / 16;
  dirty[sy] = true;
  if (y % 16 == 0 && sy > 0) dirty[sy - 1] = true;
  if (y % 16 == 15 && sy < 3) dirty[sy + 1] = true;
}
