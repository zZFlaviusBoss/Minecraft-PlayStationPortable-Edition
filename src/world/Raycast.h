#pragma once
#include <stdint.h>

class Level;

// Result of a ray-block intersection test (DDA raycast)
struct RayHit {
  bool hit;       // true if a solid block was found
  int x, y, z;    // block coordinates of the hit
  int face;       // face normal: 0=Y- 1=Y+ 2=Z- 3=Z+ 4=X- 5=X+
  int nx, ny, nz; // adjacent block on the hit face (for placement)
  uint8_t id;     // ID of the hit block
};

// Cast a ray from (ox,oy,oz) in direction (dx,dy,dz) up to maxDist blocks.
// Returns RayHit with the first solid block hit.
RayHit raycast(Level *level, float ox, float oy, float oz,
               float dx, float dy, float dz, float maxDist);
