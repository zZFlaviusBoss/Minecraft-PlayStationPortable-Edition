#include "TileRenderer.h"
#include "../world/Blocks.h"

// Fake ambient occlusion lighting per face direction
#define LIGHT_TOP 0xFFFFFFFF
#define LIGHT_SIDE 0xFFCCCCCC
#define LIGHT_BOT 0xFF999999

TileRenderer::TileRenderer(Level *level, Tesselator *opaqueTess, Tesselator *transTess,
                           Tesselator *fancyTess, Tesselator *emitTess)
    : m_level(level), m_opaqueTess(opaqueTess), m_transTess(transTess),
      m_fancyTess(fancyTess), m_emitTess(emitTess) {}

TileRenderer::~TileRenderer() {}

// Render plants (cross sprites)
bool TileRenderer::tesselateCrossInWorld(uint8_t id, int lx, int ly, int lz, int cx, int cz) {
  const BlockUV &uv = g_blockUV[id];

  int   wX = cx * CHUNK_SIZE_X + lx;
  int   wY = ly;
  int   wZ = cz * CHUNK_SIZE_Z + lz;

  // Vertex positions in absolute world space (bypass VFPU translation precision issues)
  float xt = (float)(cx * CHUNK_SIZE_X + lx);
  float yt = (float)ly;
  float zt = (float)(cz * CHUNK_SIZE_Z + lz);

  // Random offset constraint
  if (id == BLOCK_TALLGRASS) {
    int64_t seed = ((int64_t)wX * 3129871LL) ^ ((int64_t)wZ * 116129781LL) ^ ((int64_t)wY);
    seed = seed * seed * 42317861LL + seed * 11LL;

    xt += ((((seed >> 16) & 0xf) / 15.0f) - 0.5f) * 0.5f;
    yt += ((((seed >> 20) & 0xf) / 15.0f) - 1.0f) * 0.2f;
    zt += ((((seed >> 24) & 0xf) / 15.0f) - 0.5f) * 0.5f;
  }

  const float ts  = 1.0f / 16.0f;
  const float eps = 0.125f / 256.0f;

  // Sample light from the block position
  float skyL, blkL;
  {
    // 4J brightness ramp: (1-v)/(v*3+1)
    static const float lightTable[16] = {
      0.0f, 0.0625f, 0.125f, 0.1875f, 0.25f, 0.3125f, 0.375f, 0.4375f,
      0.5f, 0.5625f, 0.625f, 0.6875f, 0.75f, 0.8125f, 0.875f, 1.0f
    };
    static bool inited = false;
    if (!inited) {
      for (int i = 0; i <= 15; i++) {
        float v = 1.0f - i / 15.0f;
        const_cast<float*>(lightTable)[i] = (1.0f - v) / (v * 3.0f + 1.0f);
      }
      inited = true;
    }
    uint8_t sl = (wY + 1 < CHUNK_SIZE_Y) ? m_level->getSkyLight(wX, wY + 1, wZ)
                                          : 15;
    uint8_t bl = m_level->getBlockLight(wX, wY, wZ);
    skyL = lightTable[sl];
    blkL = lightTable[bl];
  }
  float brightness = (blkL > skyL + 0.05f) ? blkL : skyL;
  
  uint32_t baseColor = 0xFFFFFFFF;
  // Apply biome green tint for tall grass (vanilla FoliageColor::getDefaultColor)
  if (id == BLOCK_TALLGRASS) {
      baseColor = 0xFF44a065; // PSP ABGR: R=0x44, G=0xa0, B=0x65 (vanilla ~0x65a044)
  }
  uint32_t col = applyLightToFace(baseColor, brightness);

  float u0 = uv.top_x * ts + eps;
  float v0 = uv.top_y * ts + eps;
  float u1 = (uv.top_x + 1) * ts - eps;
  float v1 = (uv.top_y + 1) * ts - eps;

  // Cross width
  const float width = 0.45f;
  float x0 = xt + 0.5f - width;
  float x1 = xt + 0.5f + width;
  float z0 = zt + 0.5f - width;
  float z1 = zt + 0.5f + width;

  Tesselator *t = m_transTess;

  // Diagonal 1: (x0,z0) -> (x1,z1)
  t->addQuad(u0, v0, u1, v1, col, col, col, col,
             x0, yt + 1.0f, z0,
             x1, yt + 1.0f, z1,
             x0, yt,        z0,
             x1, yt,        z1);

  // Diagonal 2: (x0,z1) -> (x1,z0)
  t->addQuad(u0, v0, u1, v1, col, col, col, col,
             x0, yt + 1.0f, z1,
             x1, yt + 1.0f, z0,
             x0, yt,        z1,
             x1, yt,        z0);

  return true;
}

bool TileRenderer::needFace(int lx, int ly, int lz, int cx, int cz, uint8_t id, int dx, int dy, int dz, bool &outIsFancy) {
  int nx = lx + dx, ny = ly + dy, nz = lz + dz;
  int wNx = cx * CHUNK_SIZE_X + nx;
  int wNy = ny;
  int wNz = cz * CHUNK_SIZE_Z + nz;

  // Cull faces at the bottom of the world (Y=0)
  if (wNy < 0) return false;
  if (wNy >= CHUNK_SIZE_Y) return true;

  // Cull faces at the world boundaries (X and Z)
  if (wNx < 0 || wNx >= WORLD_CHUNKS_X * CHUNK_SIZE_X ||
      wNz < 0 || wNz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z) {
    return false;
  }

  uint8_t nb = m_level->getBlock(wNx, wNy, wNz);

  const BlockProps &bp = g_blockProps[id];

  if (g_blockProps[nb].isOpaque())
    return false;

  outIsFancy = false;

  if (nb == id && id == BLOCK_LEAVES) {
    outIsFancy = true;
    return true;
  }

  if (nb == id && (bp.isLiquid() || bp.isTransparent()))
    return false;

  return true;
}

// Returns raw sky light (0-15) at the face-adjacent voxel, no sun multiplier
float TileRenderer::getSkyLightRaw(int lx, int ly, int lz, int cx, int cz, int dx, int dy, int dz) {
  int nx = lx + dx, ny = ly + dy, nz = lz + dz;
  int wNx = cx * CHUNK_SIZE_X + nx;
  int wNy = ny;
  int wNz = cz * CHUNK_SIZE_Z + nz;

  uint8_t skyL;
  if (ny < 0 || ny >= CHUNK_SIZE_Y) {
    skyL = 15;
  } else {
    skyL = m_level->getSkyLight(wNx, wNy, wNz);
  }

  // 4J brightness ramp: (1-v)/(v*3+1)
  static const float lightTable[16] = {
    0.0f, 0.0625f, 0.125f, 0.1875f, 0.25f, 0.3125f, 0.375f, 0.4375f,
    0.5f, 0.5625f, 0.625f, 0.6875f, 0.75f, 0.8125f, 0.875f, 1.0f
  };
  static bool inited = false;
  if (!inited) {
    for (int i = 0; i <= 15; i++) {
      float v = 1.0f - i / 15.0f;
      const_cast<float*>(lightTable)[i] = (1.0f - v) / (v * 3.0f + 1.0f);
    }
    inited = true;
  }
  return lightTable[skyL];
}

// Smooth vertex sky light (4-sample average, no sun multiplier)
float TileRenderer::getVertexSkyLight(int wx, int wy, int wz,
                                      int dx1, int dy1, int dz1,
                                      int dx2, int dy2, int dz2) {
  // 4J brightness ramp: (1-v)/(v*3+1)
  static const float lightTable[16] = {
    0.0f, 0.0625f, 0.125f, 0.1875f, 0.25f, 0.3125f, 0.375f, 0.4375f,
    0.5f, 0.5625f, 0.625f, 0.6875f, 0.75f, 0.8125f, 0.875f, 1.0f
  };
  static bool inited = false;
  if (!inited) {
    for (int i = 0; i <= 15; i++) {
      float v = 1.0f - i / 15.0f;
      const_cast<float*>(lightTable)[i] = (1.0f - v) / (v * 3.0f + 1.0f);
    }
    inited = true;
  }

  auto getS = [&](int x, int y, int z) -> float {
    if (y < 0 || y >= CHUNK_SIZE_Y) return 1.0f;
    uint8_t skyL = m_level->getSkyLight(x, y, z);
    return lightTable[skyL];
  };

  float lCenter = getS(wx, wy, wz);
  float lE1 = getS(wx + dx1, wy + dy1, wz + dz1);
  float lE2 = getS(wx + dx2, wy + dy2, wz + dz2);
  bool oq1 = g_blockProps[m_level->getBlock(wx+dx1, wy+dy1, wz+dz1)].isOpaque();
  bool oq2 = g_blockProps[m_level->getBlock(wx+dx2, wy+dy2, wz+dz2)].isOpaque();
  float lC = getS(wx + dx1 + dx2, wy + dy1 + dy2, wz + dz1 + dz2);
  if (oq1 && oq2) lC = (lE1 + lE2) / 2.0f;
  return (lCenter + lE1 + lE2 + lC) / 4.0f;
}

// Returns block light (torch) brightness at a vertex, NOT multiplied by sun
float TileRenderer::getVertexBlockLight(int wx, int wy, int wz,
                                        int dx1, int dy1, int dz1,
                                        int dx2, int dy2, int dz2) {
  // 4J brightness ramp: (1-v)/(v*3+1)
  static const float lightTable[16] = {
    0.0f, 0.0625f, 0.125f, 0.1875f, 0.25f, 0.3125f, 0.375f, 0.4375f,
    0.5f, 0.5625f, 0.625f, 0.6875f, 0.75f, 0.8125f, 0.875f, 1.0f
  };
  static bool inited = false;
  if (!inited) {
    for (int i = 0; i <= 15; i++) {
      float v = 1.0f - i / 15.0f;
      const_cast<float*>(lightTable)[i] = (1.0f - v) / (v * 3.0f + 1.0f);
    }
    inited = true;
  }

  auto getB = [&](int x, int y, int z) -> float {
    if (y < 0 || y >= CHUNK_SIZE_Y) return 0.0f;
    uint8_t blkL = m_level->getBlockLight(x, y, z);
    return lightTable[blkL];
  };

  float lCenter = getB(wx, wy, wz);
  float lE1 = getB(wx + dx1, wy + dy1, wz + dz1);
  float lE2 = getB(wx + dx2, wy + dy2, wz + dz2);
  float lC = getB(wx + dx1 + dx2, wy + dy1 + dy2, wz + dz1 + dz2);
  return (lCenter + lE1 + lE2 + lC) / 4.0f;
}

uint32_t TileRenderer::applyLightToFace(uint32_t baseColor, float brightness) {
  uint8_t a = (baseColor >> 24) & 0xFF;
  uint8_t b = (baseColor >> 16) & 0xFF;
  uint8_t g = (baseColor >> 8) & 0xFF;
  uint8_t r = baseColor & 0xFF;
  b = (uint8_t)(b * brightness);
  g = (uint8_t)(g * brightness);
  r = (uint8_t)(r * brightness);
  return (a << 24) | (b << 16) | (g << 8) | r;
}

bool TileRenderer::tesselateBlockInWorld(uint8_t id, int lx, int ly, int lz, int cx, int cz) {
  if (id == BLOCK_TALLGRASS || id == BLOCK_FLOWER || id == BLOCK_ROSE) {
    return tesselateCrossInWorld(id, lx, ly, lz, cx, cz);
  }

  const BlockUV &uv = g_blockUV[id];

  // Vertex positions in absolute world space
  float wx = (float)(cx * CHUNK_SIZE_X + lx);
  float wy = (float)ly;
  float wz = (float)(cz * CHUNK_SIZE_Z + lz);

  // World coords for light lookups
  int wX = cx * CHUNK_SIZE_X + lx;
  int wY = ly;
  int wZ = cz * CHUNK_SIZE_Z + lz;

  const float ts = 1.0f / 16.0f;
  const float eps = 0.125f / 256.0f;
  bool drawn = false;
  bool isFancy = false;

  // Water height logic
  float blockHeight = 1.0f;
  if (id == BLOCK_WATER_STILL || id == BLOCK_WATER_FLOW) {
    uint8_t above = m_level->getBlock(wX, wY + 1, wZ);
    if (above != BLOCK_WATER_STILL && above != BLOCK_WATER_FLOW) {
      blockHeight = 14.0f / 16.0f; // 2 pixels lower
    }
  }

  // Select tesselator based on lighting
  auto pickTess = [&](Tesselator *skyTess, Tesselator *fncTess,
                      float skyL, float blkL, bool fancy) -> Tesselator * {
    if (g_blockProps[id].isTransparent()) {
      return fancy ? fncTess : m_transTess;
    }
    // If block light is dominant, route to emitTess
    if (blkL > skyL + 0.05f) return m_emitTess;
    return skyTess;
  };

  // 4J logic to avoid Z-fighting on inner leaves is handled in per-face code

  // TOP (+Y)
  if (needFace(lx, ly, lz, cx, cz, id, 0, 1, 0, isFancy)) {
    float sl00 = getVertexSkyLight(wX, wY+1, wZ, -1,0,0, 0,0,-1);
    float sl10 = getVertexSkyLight(wX, wY+1, wZ,  1,0,0, 0,0,-1);
    float sl01 = getVertexSkyLight(wX, wY+1, wZ, -1,0,0, 0,0, 1);
    float sl11 = getVertexSkyLight(wX, wY+1, wZ,  1,0,0, 0,0, 1);
    float bl00 = getVertexBlockLight(wX, wY+1, wZ, -1,0,0, 0,0,-1);
    float bl10 = getVertexBlockLight(wX, wY+1, wZ,  1,0,0, 0,0,-1);
    float bl01 = getVertexBlockLight(wX, wY+1, wZ, -1,0,0, 0,0, 1);
    float bl11 = getVertexBlockLight(wX, wY+1, wZ,  1,0,0, 0,0, 1);
    float avgBlk = (bl00+bl10+bl01+bl11)*0.25f;
    float avgSky = (sl00+sl10+sl01+sl11)*0.25f;

    Tesselator *t = pickTess(m_opaqueTess, m_fancyTess, avgSky, avgBlk, isFancy);
    bool useBlk = (avgBlk > avgSky + 0.05f);
    uint32_t c00 = applyLightToFace(LIGHT_TOP, useBlk ? bl00 : sl00);
    uint32_t c10 = applyLightToFace(LIGHT_TOP, useBlk ? bl10 : sl10);
    uint32_t c01 = applyLightToFace(LIGHT_TOP, useBlk ? bl01 : sl01);
    uint32_t c11 = applyLightToFace(LIGHT_TOP, useBlk ? bl11 : sl11);

    float u0 = uv.top_x*ts+eps, v0 = uv.top_y*ts+eps;
    float u1 = (uv.top_x+1)*ts-eps, v1 = (uv.top_y+1)*ts-eps;
    float off = isFancy ? 0.005f : 0.0f;
    t->addQuad(u0,v0,u1,v1, c00,c10,c01,c11,
               wx+off,wy+blockHeight-off,wz+off, wx+1-off,wy+blockHeight-off,wz+off, wx+off,wy+blockHeight-off,wz+1-off, wx+1-off,wy+blockHeight-off,wz+1-off);
    drawn = true;
  }

  // BOTTOM (-Y)
  if (needFace(lx, ly, lz, cx, cz, id, 0, -1, 0, isFancy)) {
    float sl00 = getVertexSkyLight(wX, wY-1, wZ, -1,0,0, 0,0,-1);
    float sl10 = getVertexSkyLight(wX, wY-1, wZ,  1,0,0, 0,0,-1);
    float sl01 = getVertexSkyLight(wX, wY-1, wZ, -1,0,0, 0,0, 1);
    float sl11 = getVertexSkyLight(wX, wY-1, wZ,  1,0,0, 0,0, 1);
    float bl00 = getVertexBlockLight(wX, wY-1, wZ, -1,0,0, 0,0,-1);
    float bl10 = getVertexBlockLight(wX, wY-1, wZ,  1,0,0, 0,0,-1);
    float bl01 = getVertexBlockLight(wX, wY-1, wZ, -1,0,0, 0,0, 1);
    float bl11 = getVertexBlockLight(wX, wY-1, wZ,  1,0,0, 0,0, 1);
    float avgBlk=(bl00+bl10+bl01+bl11)*0.25f, avgSky=(sl00+sl10+sl01+sl11)*0.25f;

    Tesselator *t = pickTess(m_opaqueTess, m_fancyTess, avgSky, avgBlk, isFancy);
    bool useBlk = (avgBlk > avgSky + 0.05f);
    uint32_t c00=applyLightToFace(LIGHT_BOT, useBlk?bl00:sl00);
    uint32_t c10=applyLightToFace(LIGHT_BOT, useBlk?bl10:sl10);
    uint32_t c01=applyLightToFace(LIGHT_BOT, useBlk?bl01:sl01);
    uint32_t c11=applyLightToFace(LIGHT_BOT, useBlk?bl11:sl11);

    float u0=uv.bot_x*ts+eps, v0=uv.bot_y*ts+eps;
    float u1=(uv.bot_x+1)*ts-eps, v1=(uv.bot_y+1)*ts-eps;
    float off = isFancy ? 0.005f : 0.0f;
    t->addQuad(u0,v0,u1,v1, c01,c11,c00,c10,
               wx+off,wy+off,wz+1-off, wx+1-off,wy+off,wz+1-off, wx+off,wy+off,wz+off, wx+1-off,wy+off,wz+off);
    drawn = true;
  }

  // NORTH (-Z)
  if (needFace(lx, ly, lz, cx, cz, id, 0, 0, -1, isFancy)) {
    float sl11=getVertexSkyLight(wX,wY,wZ-1, 1,0,0, 0,1,0);
    float sl01=getVertexSkyLight(wX,wY,wZ-1,-1,0,0, 0,1,0);
    float sl10=getVertexSkyLight(wX,wY,wZ-1, 1,0,0, 0,-1,0);
    float sl00=getVertexSkyLight(wX,wY,wZ-1,-1,0,0, 0,-1,0);
    float bl11=getVertexBlockLight(wX,wY,wZ-1, 1,0,0, 0,1,0);
    float bl01=getVertexBlockLight(wX,wY,wZ-1,-1,0,0, 0,1,0);
    float bl10=getVertexBlockLight(wX,wY,wZ-1, 1,0,0, 0,-1,0);
    float bl00=getVertexBlockLight(wX,wY,wZ-1,-1,0,0, 0,-1,0);
    float avgBlk=(bl11+bl01+bl10+bl00)*0.25f, avgSky=(sl11+sl01+sl10+sl00)*0.25f;

    Tesselator *t=pickTess(m_opaqueTess,m_fancyTess,avgSky,avgBlk,isFancy);
    bool useBlk=(avgBlk>avgSky+0.05f);
    uint32_t c11=applyLightToFace(LIGHT_SIDE,useBlk?bl11:sl11);
    uint32_t c01=applyLightToFace(LIGHT_SIDE,useBlk?bl01:sl01);
    uint32_t c10=applyLightToFace(LIGHT_SIDE,useBlk?bl10:sl10);
    uint32_t c00=applyLightToFace(LIGHT_SIDE,useBlk?bl00:sl00);

    float u0=uv.side_x*ts+eps, v0=uv.side_y*ts+eps;
    float u1=(uv.side_x+1)*ts-eps, v1=(uv.side_y+1)*ts-eps;
    float off = isFancy ? 0.005f : 0.0f;
    t->addQuad(u0,v0,u1,v1, c11,c01,c10,c00,
               wx+1-off,wy+blockHeight-off,wz+off, wx+off,wy+blockHeight-off,wz+off, wx+1-off,wy+off,wz+off, wx+off,wy+off,wz+off);
    drawn = true;
  }

  // SOUTH (+Z)
  if (needFace(lx, ly, lz, cx, cz, id, 0, 0, 1, isFancy)) {
    float sl01=getVertexSkyLight(wX,wY,wZ+1,-1,0,0, 0,1,0);
    float sl11=getVertexSkyLight(wX,wY,wZ+1, 1,0,0, 0,1,0);
    float sl00=getVertexSkyLight(wX,wY,wZ+1,-1,0,0, 0,-1,0);
    float sl10=getVertexSkyLight(wX,wY,wZ+1, 1,0,0, 0,-1,0);
    float bl01=getVertexBlockLight(wX,wY,wZ+1,-1,0,0, 0,1,0);
    float bl11=getVertexBlockLight(wX,wY,wZ+1, 1,0,0, 0,1,0);
    float bl00=getVertexBlockLight(wX,wY,wZ+1,-1,0,0, 0,-1,0);
    float bl10=getVertexBlockLight(wX,wY,wZ+1, 1,0,0, 0,-1,0);
    float avgBlk=(bl01+bl11+bl00+bl10)*0.25f, avgSky=(sl01+sl11+sl00+sl10)*0.25f;

    Tesselator *t=pickTess(m_opaqueTess,m_fancyTess,avgSky,avgBlk,isFancy);
    bool useBlk=(avgBlk>avgSky+0.05f);
    uint32_t c01=applyLightToFace(LIGHT_SIDE,useBlk?bl01:sl01);
    uint32_t c11=applyLightToFace(LIGHT_SIDE,useBlk?bl11:sl11);
    uint32_t c00=applyLightToFace(LIGHT_SIDE,useBlk?bl00:sl00);
    uint32_t c10=applyLightToFace(LIGHT_SIDE,useBlk?bl10:sl10);

    float u0=uv.side_x*ts+eps, v0=uv.side_y*ts+eps;
    float u1=(uv.side_x+1)*ts-eps, v1=(uv.side_y+1)*ts-eps;
    float off = isFancy ? 0.005f : 0.0f;
    t->addQuad(u0,v0,u1,v1, c01,c11,c00,c10,
               wx+off,wy+blockHeight-off,wz+1-off, wx+1-off,wy+blockHeight-off,wz+1-off, wx+off,wy+off,wz+1-off, wx+1-off,wy+off,wz+1-off);
    drawn = true;
  }

  // WEST (-X)
  if (needFace(lx, ly, lz, cx, cz, id, -1, 0, 0, isFancy)) {
    float sl01=getVertexSkyLight(wX-1,wY,wZ, 0,1,0, 0,0,-1);
    float sl11=getVertexSkyLight(wX-1,wY,wZ, 0,1,0, 0,0, 1);
    float sl00=getVertexSkyLight(wX-1,wY,wZ, 0,-1,0, 0,0,-1);
    float sl10=getVertexSkyLight(wX-1,wY,wZ, 0,-1,0, 0,0, 1);
    float bl01=getVertexBlockLight(wX-1,wY,wZ, 0,1,0, 0,0,-1);
    float bl11=getVertexBlockLight(wX-1,wY,wZ, 0,1,0, 0,0, 1);
    float bl00=getVertexBlockLight(wX-1,wY,wZ, 0,-1,0, 0,0,-1);
    float bl10=getVertexBlockLight(wX-1,wY,wZ, 0,-1,0, 0,0, 1);
    float avgBlk=(bl01+bl11+bl00+bl10)*0.25f, avgSky=(sl01+sl11+sl00+sl10)*0.25f;

    Tesselator *t=pickTess(m_opaqueTess,m_fancyTess,avgSky,avgBlk,isFancy);
    bool useBlk=(avgBlk>avgSky+0.05f);
    uint32_t c01=applyLightToFace(LIGHT_SIDE,useBlk?bl01:sl01);
    uint32_t c11=applyLightToFace(LIGHT_SIDE,useBlk?bl11:sl11);
    uint32_t c00=applyLightToFace(LIGHT_SIDE,useBlk?bl00:sl00);
    uint32_t c10=applyLightToFace(LIGHT_SIDE,useBlk?bl10:sl10);

    float u0=uv.side_x*ts+eps, v0=uv.side_y*ts+eps;
    float u1=(uv.side_x+1)*ts-eps, v1=(uv.side_y+1)*ts-eps;
    float off = isFancy ? 0.005f : 0.0f;
    t->addQuad(u0,v0,u1,v1, c01,c11,c00,c10,
               wx+off,wy+blockHeight-off,wz+off, wx+off,wy+blockHeight-off,wz+1-off, wx+off,wy+off,wz+off, wx+off,wy+off,wz+1-off);
    drawn = true;
  }

  // EAST (+X)
  if (needFace(lx, ly, lz, cx, cz, id, 1, 0, 0, isFancy)) {
    float sl11=getVertexSkyLight(wX+1,wY,wZ, 0,1,0, 0,0, 1);
    float sl01=getVertexSkyLight(wX+1,wY,wZ, 0,1,0, 0,0,-1);
    float sl10=getVertexSkyLight(wX+1,wY,wZ, 0,-1,0, 0,0, 1);
    float sl00=getVertexSkyLight(wX+1,wY,wZ, 0,-1,0, 0,0,-1);
    float bl11=getVertexBlockLight(wX+1,wY,wZ, 0,1,0, 0,0, 1);
    float bl01=getVertexBlockLight(wX+1,wY,wZ, 0,1,0, 0,0,-1);
    float bl10=getVertexBlockLight(wX+1,wY,wZ, 0,-1,0, 0,0, 1);
    float bl00=getVertexBlockLight(wX+1,wY,wZ, 0,-1,0, 0,0,-1);
    float avgBlk=(bl11+bl01+bl10+bl00)*0.25f, avgSky=(sl11+sl01+sl10+sl00)*0.25f;

    Tesselator *t=pickTess(m_opaqueTess,m_fancyTess,avgSky,avgBlk,isFancy);
    bool useBlk=(avgBlk>avgSky+0.05f);
    uint32_t c11=applyLightToFace(LIGHT_SIDE,useBlk?bl11:sl11);
    uint32_t c01=applyLightToFace(LIGHT_SIDE,useBlk?bl01:sl01);
    uint32_t c10=applyLightToFace(LIGHT_SIDE,useBlk?bl10:sl10);
    uint32_t c00=applyLightToFace(LIGHT_SIDE,useBlk?bl00:sl00);

    float u0=uv.side_x*ts+eps, v0=uv.side_y*ts+eps;
    float u1=(uv.side_x+1)*ts-eps, v1=(uv.side_y+1)*ts-eps;
    float off = isFancy ? 0.005f : 0.0f;
    t->addQuad(u0,v0,u1,v1, c11,c01,c10,c00,
               wx+1-off,wy+blockHeight-off,wz+1-off, wx+1-off,wy+blockHeight-off,wz+off, wx+1-off,wy+off,wz+1-off, wx+1-off,wy+off,wz+off);
    drawn = true;
  }

  return drawn;
}
