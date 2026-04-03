// TreeFeature.cpp - Generating classic trees (Minecraft Oak)
// Faithful port from Minecraft.World/TreeFeature.cpp (4J Studios, Oct 2014)

#include "TreeFeature.h"
#include "Blocks.h"
#include "Random.h"
#include "Level.h"
#include <stdlib.h> // abs

bool TreeFeature::place(Level* level, int wx, int wy, int wz, Random &rng) {
  // Trunk height: 4, 5 or 6 - identical to original 4J
  int treeHeight = rng.nextInt(3) + 4;

  // Quick check: do we have vertical space?
  if (wy + treeHeight + 2 >= CHUNK_SIZE_Y)
    return false;

  // The trunk (log)
  for (int hh = 0; hh < treeHeight; hh++) {
    int y = wy + hh;
    uint8_t b = level->getBlock(wx, y, wz);
    if (b == BLOCK_AIR || b == BLOCK_LEAVES)
      level->setBlock(wx, y, wz, BLOCK_LOG);
  }

  // Leaves: 3 layers from top to bottom (4J generates top-down for heightmap)
  // Top layer (y+treeHeight): radius 1. Layers -1, -2 (y+treeHeight-1, y+treeHeight-2): radius 2
  const int grassHeight = 3;
  for (int yy = wy + treeHeight; yy >= wy + treeHeight - grassHeight; yy--) {
    if (yy < 0 || yy >= CHUNK_SIZE_Y)
      continue;

    int yo = yy - (wy + treeHeight); // 0, -1, -2, -3
    int offs = 1 - yo / 2;           // top=1, rest=2

    for (int xx = wx - offs; xx <= wx + offs; xx++) {
      int xo = xx - wx;
      for (int zz = wz - offs; zz <= wz + offs; zz++) {
        int zo = zz - wz;

        // Classic corner cutting: if it's a corner (|xo|==offs && |zo|==offs)
        // truncate completely at the top layer (yo==0) and 50% on the others
        if (abs(xo) == offs && abs(zo) == offs &&
            (rng.nextInt(2) == 0 || yo == 0))
          continue;

        if (level->getBlock(xx, yy, zz) == BLOCK_AIR)
          level->setBlock(xx, yy, zz, BLOCK_LEAVES);
      }
    }
  }

  return true;
}
