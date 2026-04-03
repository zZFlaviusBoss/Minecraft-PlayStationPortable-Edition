#pragma once
// TreeFeature.h - Placement of classic trees (ported from
// Minecraft.World/TreeFeature.cpp) 4J Studios logic: trunk 4-6, canopy with
// cut corners
#include "chunk_defs.h"
#include <stdint.h>

class Random;
class Level;

class TreeFeature {
public:
  // Attempts to place a tree with its base at absolute coordinates (wx, wy, wz) in the world.
  // Using the global reference (Level) like 4J Studios, we bypass leaf clipping at chunk borders.
  static bool place(Level *level, int wx, int wy, int wz, Random &rng);
};
