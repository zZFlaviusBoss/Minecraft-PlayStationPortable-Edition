// Raycast.cpp

#include "Raycast.h"
#include "Blocks.h"
#include "Level.h"
#include <math.h>

RayHit raycast(Level *level, float ox, float oy, float oz,
               float dx, float dy, float dz, float maxDist) {
  RayHit result;
  result.hit = false;
  result.x = result.y = result.z = 0;
  result.face = 0;
  result.nx = result.ny = result.nz = 0;

  // Normalize direction
  float len = sqrtf(dx * dx + dy * dy + dz * dz);
  if (len < 0.0001f) return result;
  dx /= len;
  dy /= len;
  dz /= len;

  // Current block position
  int mapX = (int)floorf(ox);
  int mapY = (int)floorf(oy);
  int mapZ = (int)floorf(oz);

  // Step direction (+1 or -1)
  int stepX = (dx >= 0) ? 1 : -1;
  int stepY = (dy >= 0) ? 1 : -1;
  int stepZ = (dz >= 0) ? 1 : -1;

  // Distance to next grid boundary along each axis
  float tMaxX = (dx != 0.0f) ? ((dx > 0 ? (mapX + 1.0f - ox) : (ox - mapX)) / fabsf(dx)) : 1e30f;
  float tMaxY = (dy != 0.0f) ? ((dy > 0 ? (mapY + 1.0f - oy) : (oy - mapY)) / fabsf(dy)) : 1e30f;
  float tMaxZ = (dz != 0.0f) ? ((dz > 0 ? (mapZ + 1.0f - oz) : (oz - mapZ)) / fabsf(dz)) : 1e30f;

  // How far along the ray we have to travel to cross one full block in each axis
  float tDeltaX = (dx != 0.0f) ? (1.0f / fabsf(dx)) : 1e30f;
  float tDeltaY = (dy != 0.0f) ? (1.0f / fabsf(dy)) : 1e30f;
  float tDeltaZ = (dz != 0.0f) ? (1.0f / fabsf(dz)) : 1e30f;

  float dist = 0.0f;
  int lastFace = 0;

  // Step through grid
  for (int i = 0; i < 200 && dist < maxDist; i++) {
    // Check current block
    uint8_t id = level->getBlock(mapX, mapY, mapZ);
    if (id != BLOCK_AIR && !g_blockProps[id].isLiquid()) {
      result.hit = true;
      result.x = mapX;
      result.y = mapY;
      result.z = mapZ;
      result.id = id;
      result.face = lastFace;

      // Compute adjacent block for placement
      result.nx = mapX;
      result.ny = mapY;
      result.nz = mapZ;
      switch (lastFace) {
        case 0: result.ny--; break; // Y- (bottom)
        case 1: result.ny++; break; // Y+ (top)
        case 2: result.nz--; break; // Z- (north)
        case 3: result.nz++; break; // Z+ (south)
        case 4: result.nx--; break; // X- (west)
        case 5: result.nx++; break; // X+ (east)
      }
      return result;
    }

    // Advance to next block boundary (DDA step)
    if (tMaxX < tMaxY) {
      if (tMaxX < tMaxZ) {
        dist = tMaxX;
        mapX += stepX;
        tMaxX += tDeltaX;
        lastFace = (stepX > 0) ? 4 : 5; // hit west or east face
      } else {
        dist = tMaxZ;
        mapZ += stepZ;
        tMaxZ += tDeltaZ;
        lastFace = (stepZ > 0) ? 2 : 3; // hit north or south face
      }
    } else {
      if (tMaxY < tMaxZ) {
        dist = tMaxY;
        mapY += stepY;
        tMaxY += tDeltaY;
        lastFace = (stepY > 0) ? 0 : 1; // hit bottom or top face
      } else {
        dist = tMaxZ;
        mapZ += stepZ;
        tMaxZ += tDeltaZ;
        lastFace = (stepZ > 0) ? 2 : 3;
      }
    }
  }

  return result;
}
