// BlockHighlight.cpp - wireframe cube around the targeted block
// Uses PSP GU_LINES to draw 12 edges of the block's AABB

#include "BlockHighlight.h"
#include "../world/Blocks.h"
#include <pspgu.h>
#include <pspgum.h>

// Simple vertex: just position (no texture, no color per-vertex)
struct HLVertex {
  float x, y, z;
};

void BlockHighlight_Draw(int bx, int by, int bz, uint8_t blockId) {
  // Slight expansion to prevent z-fighting with block faces
  const float e = 0.002f;
  const BlockProps& props = g_blockProps[blockId];
  

  float x0 = props.minX - e;
  float y0 = props.minY - e;
  float z0 = props.minZ - e;
  float x1 = props.maxX + e;
  float y1 = props.maxY + e;
  float z1 = props.maxZ + e;

  // 12 edges of a cube = 24 vertices (2 per line)
  static HLVertex lines[24];

  // Bottom face edges (y0)
  lines[0]  = {x0, y0, z0}; lines[1]  = {x1, y0, z0};
  lines[2]  = {x1, y0, z0}; lines[3]  = {x1, y0, z1};
  lines[4]  = {x1, y0, z1}; lines[5]  = {x0, y0, z1};
  lines[6]  = {x0, y0, z1}; lines[7]  = {x0, y0, z0};

  // Top face edges (y1)
  lines[8]  = {x0, y1, z0}; lines[9]  = {x1, y1, z0};
  lines[10] = {x1, y1, z0}; lines[11] = {x1, y1, z1};
  lines[12] = {x1, y1, z1}; lines[13] = {x0, y1, z1};
  lines[14] = {x0, y1, z1}; lines[15] = {x0, y1, z0};

  // Vertical edges
  lines[16] = {x0, y0, z0}; lines[17] = {x0, y1, z0};
  lines[18] = {x1, y0, z0}; lines[19] = {x1, y1, z0};
  lines[20] = {x1, y0, z1}; lines[21] = {x1, y1, z1};
  lines[22] = {x0, y0, z1}; lines[23] = {x0, y1, z1};

  // Disable texturing for wireframe
  sceGuDisable(GU_TEXTURE_2D);

  // Semi-transparent black lines
  sceGuColor(0x88000000);
  sceGuDepthMask(GU_TRUE); // no depth writes

  sceGumMatrixMode(GU_MODEL);
  sceGumLoadIdentity();
  
  // Translate to block position to keep coordinate values small
  ScePspFVector3 blockPos = { (float)bx, (float)by, (float)bz };
  sceGumTranslate(&blockPos);

  sceGumDrawArray(GU_LINES,
                  GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                  24, NULL, lines);

  sceGuDepthMask(GU_FALSE); // restore depth writes
  sceGuEnable(GU_TEXTURE_2D);
}
