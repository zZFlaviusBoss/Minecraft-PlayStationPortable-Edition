#pragma once
#include <stdint.h>
#include "../world/Level.h"
#include "Tesselator.h"

class TileRenderer {
public:
  // emitTess: receives block-lit (torch) faces drawn at full brightness at night
  // opaqueTess: receives sky-lit faces dimmed at night via sceGuAmbient(sunBrightness)
  TileRenderer(Level *level, Tesselator *opaqueTess, Tesselator *transTess,
               Tesselator *fancyTess, Tesselator *emitTess);
  ~TileRenderer();

  bool tesselateBlockInWorld(uint8_t id, int lx, int ly, int lz, int cx, int cz);

private:
  Level *m_level;
  Tesselator *m_opaqueTess;
  Tesselator *m_transTess;
  Tesselator *m_fancyTess;
  Tesselator *m_emitTess;   // Block-lit (torch) faces

  float getSkyLightRaw(int lx, int ly, int lz, int cx, int cz, int dx, int dy, int dz);
  float getVertexSkyLight(int wx, int wy, int wz, int dx1, int dy1, int dz1, int dx2, int dy2, int dz2);
  float getVertexBlockLight(int wx, int wy, int wz, int dx1, int dy1, int dz1, int dx2, int dy2, int dz2);
  uint32_t applyLightToFace(uint32_t baseColor, float brightness);
  bool needFace(int lx, int ly, int lz, int cx, int cz, uint8_t id, int dx, int dy, int dz, bool &outIsFancy);
  bool tesselateCrossInWorld(uint8_t id, int lx, int ly, int lz, int cx, int cz);
};
