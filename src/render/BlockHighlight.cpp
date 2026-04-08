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

  // Stair outline needs up to 18 edges (36 verts), keep headroom.
  static HLVertex lines[64];
  int vcount = 0;
  auto addLine = [&](float ax, float ay, float az, float bx, float by, float bz) {
    lines[vcount++] = {ax, ay, az};
    lines[vcount++] = {bx, by, bz};
  };
  auto appendBoxEdges = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    // Bottom
    lines[vcount++] = {x0, y0, z0}; lines[vcount++] = {x1, y0, z0};
    lines[vcount++] = {x1, y0, z0}; lines[vcount++] = {x1, y0, z1};
    lines[vcount++] = {x1, y0, z1}; lines[vcount++] = {x0, y0, z1};
    lines[vcount++] = {x0, y0, z1}; lines[vcount++] = {x0, y0, z0};
    // Top
    lines[vcount++] = {x0, y1, z0}; lines[vcount++] = {x1, y1, z0};
    lines[vcount++] = {x1, y1, z0}; lines[vcount++] = {x1, y1, z1};
    lines[vcount++] = {x1, y1, z1}; lines[vcount++] = {x0, y1, z1};
    lines[vcount++] = {x0, y1, z1}; lines[vcount++] = {x0, y1, z0};
    // Vertical
    lines[vcount++] = {x0, y0, z0}; lines[vcount++] = {x0, y1, z0};
    lines[vcount++] = {x1, y0, z0}; lines[vcount++] = {x1, y1, z0};
    lines[vcount++] = {x1, y0, z1}; lines[vcount++] = {x1, y1, z1};
    lines[vcount++] = {x0, y0, z1}; lines[vcount++] = {x0, y1, z1};
  };

  if (isStairId(blockId)) {
    const float x0 = 0.0f - e, x1 = 1.0f + e;
    const float y0 = 0.0f - e, yM = 0.5f, y1 = 1.0f + e;
    const float z0 = 0.0f - e, zM = 0.5f, z1 = 1.0f + e;
    int facing = stairFacing(blockId);
    bool upsideDown = isUpsideDownStair(blockId);
    auto rotateLocal = [&](float &x, float &z) {
      float rx = x, rz = z;
      if (facing == 1) { rx = 1.0f - z; rz = x; }
      else if (facing == 2) { rx = 1.0f - x; rz = 1.0f - z; }
      else if (facing == 3) { rx = z; rz = 1.0f - x; }
      x = rx;
      z = rz;
    };
    auto addLineRot = [&](float ax, float ay, float az, float bx, float by, float bz) {
      if (upsideDown) {
        ay = 1.0f - ay;
        by = 1.0f - by;
      }
      rotateLocal(ax, az);
      rotateLocal(bx, bz);
      addLine(ax, ay, az, bx, by, bz);
    };

    // Bottom rectangle
    addLineRot(x0, y0, z0, x1, y0, z0);
    addLineRot(x1, y0, z0, x1, y0, z1);
    addLineRot(x1, y0, z1, x0, y0, z1);
    addLineRot(x0, y0, z1, x0, y0, z0);

    // Upper-top rectangle (rear half)
    addLineRot(x0, y1, zM, x1, y1, zM);
    addLineRot(x1, y1, zM, x1, y1, z1);
    addLineRot(x1, y1, z1, x0, y1, z1);
    addLineRot(x0, y1, z1, x0, y1, zM);

    // Step top rectangle (front half at y=0.5)
    addLineRot(x0, yM, z0, x1, yM, z0);
    addLineRot(x1, yM, z0, x1, yM, zM);
    addLineRot(x1, yM, zM, x0, yM, zM);
    addLineRot(x0, yM, zM, x0, yM, z0);

    // Outer verticals / riser sides (no duplicated coplanar lines)
    addLineRot(x0, y0, z0, x0, yM, z0);
    addLineRot(x1, y0, z0, x1, yM, z0);
    addLineRot(x0, y0, z1, x0, y1, z1);
    addLineRot(x1, y0, z1, x1, y1, z1);
    addLineRot(x0, yM, zM, x0, y1, zM);
    addLineRot(x1, yM, zM, x1, y1, zM);
  } else {
    const BlockProps& props = g_blockProps[blockId];
    appendBoxEdges(props.minX - e, props.minY - e, props.minZ - e,
                   props.maxX + e, props.maxY + e, props.maxZ + e);
  }

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
                  vcount, NULL, lines);

  sceGuDepthMask(GU_FALSE); // restore depth writes
  sceGuEnable(GU_TEXTURE_2D);
}
