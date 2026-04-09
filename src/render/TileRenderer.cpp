#include "TileRenderer.h"
#include "../world/Blocks.h"

// Fake ambient occlusion lighting per face direction
#define LIGHT_TOP 0xFFFFFFFF
#define LIGHT_SIDE 0xFFCCCCCC
#define LIGHT_BOT 0xFF999999

static inline uint8_t stretchBlockLightRange(uint8_t rawLevel) {
  // Keep source brightness unchanged, but make falloff slower so the
  // perceived travel distance of block light is roughly 3x.
  int stretched = (int)rawLevel * 3;
  if (stretched > 15) stretched = 15;
  return (uint8_t)stretched;
}

static inline bool isSlabBlock(uint8_t id) {
  return id == BLOCK_STONE_SLAB || id == BLOCK_WOOD_SLAB || id == BLOCK_COBBLE_SLAB ||
         id == BLOCK_SANDSTONE_SLAB || id == BLOCK_BRICK_SLAB || id == BLOCK_STONE_BRICK_SLAB ||
         id == BLOCK_STONE_SLAB_TOP || id == BLOCK_WOOD_SLAB_TOP || id == BLOCK_COBBLE_SLAB_TOP ||
         id == BLOCK_SANDSTONE_SLAB_TOP || id == BLOCK_BRICK_SLAB_TOP || id == BLOCK_STONE_BRICK_SLAB_TOP;
}

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
    uint8_t bl = stretchBlockLightRange(m_level->getBlockLight(wX, wY, wZ));
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
  float emitL = blkL * 1.35f;
  if (emitL > 1.0f) emitL = 1.0f;
  uint32_t emitCol = applyLightToFace(baseColor, emitL);

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

  if (blkL > 0.001f) {
    m_emitTess->addQuad(u0, v0, u1, v1, emitCol, emitCol, emitCol, emitCol,
                        x0, yt + 1.0f, z0,
                        x1, yt + 1.0f, z1,
                        x0, yt,        z0,
                        x1, yt,        z1);
    m_emitTess->addQuad(u0, v0, u1, v1, emitCol, emitCol, emitCol, emitCol,
                        x0, yt + 1.0f, z1,
                        x1, yt + 1.0f, z0,
                        x0, yt,        z1,
                        x1, yt,        z0);
  }

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

  if (g_blockProps[nb].isOpaque()) {
    // Stairs are partial geometry and should never fully occlude a neighboring
    // block face like a full cube does.
    if (isStairId(id) || isStairId(nb)) {
      return true;
    }
    if (dy == 1 || dy == -1) {
      const BlockProps &nbp = g_blockProps[nb];
      const float epsY = 0.0001f;
      if (dy == 1) {
        // Neighbor is one cell above: keep face only if there is a vertical gap.
        float currTop = bp.maxY;
        float nbBottomShifted = 1.0f + nbp.minY;
        if (currTop + epsY < nbBottomShifted) return true;
      } else {
        // Neighbor is one cell below: keep face only if there is a vertical gap.
        float currBottom = bp.minY;
        float nbTopShifted = nbp.maxY - 1.0f;
        if (currBottom > nbTopShifted + epsY) return true;
      }
    }
    // For half-height slabs against full cubes (or vice versa), don't fully cull
    // horizontal faces; otherwise we lose visible upper/lower parts and get holes.
    if (dy == 0 && (isSlabBlock(id) || isSlabBlock(nb))) {
      const BlockProps &nbp = g_blockProps[nb];
      if (bp.minY != nbp.minY || bp.maxY != nbp.maxY) {
        return true;
      }
    }
    return false;
  }

  outIsFancy = false;

  if (nb == id && id == BLOCK_LEAVES) {
    outIsFancy = true;
    return true;
  }

  if (bp.isLiquid() && g_blockProps[nb].isLiquid())
    return false;

  if (nb == id && bp.isTransparent() && !isStairId(id))
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
    uint8_t blkL = stretchBlockLightRange(m_level->getBlockLight(x, y, z));
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
  if (id == BLOCK_TALLGRASS || id == BLOCK_FLOWER || id == BLOCK_ROSE || id == BLOCK_SAPLING || id == BLOCK_REEDS) {
    return tesselateCrossInWorld(id, lx, ly, lz, cx, cz);
  }

  // Fluids use a dedicated path so they behave visually like liquid:
  // lower top surface, smooth corner heights, and hidden inner faces.
  if (id == BLOCK_WATER_STILL || id == BLOCK_WATER_FLOW ||
      id == BLOCK_LAVA_STILL || id == BLOCK_LAVA_FLOW) {
    const BlockUV &uv = g_blockUV[id];
    const bool isWater = (id == BLOCK_WATER_STILL || id == BLOCK_WATER_FLOW);
    float wx = (float)(cx * CHUNK_SIZE_X + lx);
    float wy = (float)ly;
    float wz = (float)(cz * CHUNK_SIZE_Z + lz);
    int wX = cx * CHUNK_SIZE_X + lx;
    int wY = ly;
    int wZ = cz * CHUNK_SIZE_Z + lz;

    const float ts = 1.0f / 16.0f;
    const float eps = 0.125f / 256.0f;

    auto isFluidId = [&](uint8_t b) {
      if (isWater) return b == BLOCK_WATER_STILL || b == BLOCK_WATER_FLOW;
      return b == BLOCK_LAVA_STILL || b == BLOCK_LAVA_FLOW;
    };
    const bool selfLitFluid = (g_blockProps[id].light_emit > 0);
    // Self-lit fluids (lava) go to opaque pass to avoid semi-transparent look.
    // Their night brightness is then reinforced by emissive overlay quads.
    Tesselator *fluidTess = selfLitFluid ? m_opaqueTess : m_transTess;
    // Water tint/alpha tuned toward MCPE visuals (blue tint + visible transparency).
    uint32_t topColor = isWater ? 0xA0E07040 : 0xFFFFFFFF;
    uint32_t bottomColor = isWater ? 0xB4B85C33 : 0xFFFFFFFF;
    uint32_t sideColor = isWater ? 0xAFC8683A : 0xFFFFFFFF;
    const bool addSelfLitOverlay = selfLitFluid && (fluidTess != m_emitTess);

    // Smooth corner heights (MCPE 0.6.1-like):
    // - top surface is ~14px (8/9 of a block) for calm/full fluid
    // - if fluid exists above a sampled corner, that corner becomes full-height (1.0)
    //   so waterfalls don't form horizontal slits between stacked blocks.
    auto cornerHeight = [&](int cx0, int cz0) -> float {
      float sum = 0.0f;
      float wsum = 0.0f;
      for (int ox = -1; ox <= 0; ++ox) {
        for (int oz = -1; oz <= 0; ++oz) {
          int sx = cx0 + ox;
          int sz = cz0 + oz;
          if (isFluidId(m_level->getBlock(sx, wY + 1, sz))) return 1.0f;

          uint8_t idHere = m_level->getBlock(sx, wY, sz);
          if (isFluidId(idHere)) {
            uint8_t d = isWater ? m_level->getWaterDepth(sx, wY, sz)
                                : m_level->getLavaDepth(sx, wY, sz);
            if (d == 0xFF || d > 7) d = (idHere == BLOCK_WATER_STILL || idHere == BLOCK_LAVA_STILL) ? 0 : 1;

            // MCPE 0.6.1 uses LiquidTile::getHeight(d)=(d+1)/9 and renderer returns
            // 1 - averagedHeight, which becomes (8-d)/9 at a single sample.
            float h = ((float)d + 1.0f) / 9.0f;
            float w = (d == 0) ? 10.0f : 1.0f;
            sum += h * w;
            wsum += w;
          } else if (!g_blockProps[idHere].isSolid()) {
            // Non-solid neighbors bias the corner down (same as old MCPE logic).
            sum += 1.0f;
            wsum += 1.0f;
          }
        }
      }
      if (wsum <= 0.0f) return 0.0f;
      return 1.0f - (sum / wsum);
    };

    float h00 = cornerHeight(wX, wZ);
    float h01 = cornerHeight(wX, wZ + 1);
    float h11 = cornerHeight(wX + 1, wZ + 1);
    float h10 = cornerHeight(wX + 1, wZ);
    bool drawn = false;

    bool isFancy = false;
    // Top
    if (needFace(lx, ly, lz, cx, cz, id, 0, 1, 0, isFancy)) {
      float sl = getSkyLightRaw(lx, ly, lz, cx, cz, 0, 1, 0);
      float bl = getVertexBlockLight(wX, wY + 1, wZ, 0, 0, 0, 0, 0, 0);
      float br = (bl > sl + 0.05f) ? bl : sl;
      if (selfLitFluid) br = 1.0f;
      uint32_t c = applyLightToFace(topColor, br);
      float u0 = uv.top_x * ts + eps, v0 = uv.top_y * ts + eps;
      float u1 = (uv.top_x + 1) * ts - eps, v1 = (uv.top_y + 1) * ts - eps;
      fluidTess->addQuad(u0, v0, u1, v1, c, c, c, c,
                           wx, wy + h00, wz,
                           wx + 1, wy + h10, wz,
                           wx, wy + h01, wz + 1,
                           wx + 1, wy + h11, wz + 1);
      if (addSelfLitOverlay) {
        uint32_t ec = applyLightToFace(topColor, 1.0f);
        m_emitTess->addQuad(u0, v0, u1, v1, ec, ec, ec, ec,
                            wx, wy + h00, wz,
                            wx + 1, wy + h10, wz,
                            wx, wy + h01, wz + 1,
                            wx + 1, wy + h11, wz + 1);
      }
      drawn = true;
    }

    // Bottom
    if (needFace(lx, ly, lz, cx, cz, id, 0, -1, 0, isFancy)) {
      float sl = getSkyLightRaw(lx, ly, lz, cx, cz, 0, -1, 0);
      float bl = getVertexBlockLight(wX, wY - 1, wZ, 0, 0, 0, 0, 0, 0);
      float br = (bl > sl + 0.05f) ? bl : sl;
      if (selfLitFluid) br = 1.0f;
      uint32_t c = applyLightToFace(bottomColor, br);
      float u0 = uv.bot_x * ts + eps, v0 = uv.bot_y * ts + eps;
      float u1 = (uv.bot_x + 1) * ts - eps, v1 = (uv.bot_y + 1) * ts - eps;
      fluidTess->addQuad(u0, v0, u1, v1, c, c, c, c,
                           wx, wy, wz + 1,
                           wx + 1, wy, wz + 1,
                           wx, wy, wz,
                           wx + 1, wy, wz);
      if (addSelfLitOverlay) {
        uint32_t ec = applyLightToFace(bottomColor, 1.0f);
        m_emitTess->addQuad(u0, v0, u1, v1, ec, ec, ec, ec,
                            wx, wy, wz + 1,
                            wx + 1, wy, wz + 1,
                            wx, wy, wz,
                            wx + 1, wy, wz);
      }
      drawn = true;
    }

    auto addSide = [&](int dx, int dz) {
      bool localFancy = false;
      if (!needFace(lx, ly, lz, cx, cz, id, dx, 0, dz, localFancy)) return;
      float sl = getSkyLightRaw(lx, ly, lz, cx, cz, dx, 0, dz);
      float bl = getVertexBlockLight(wX + dx, wY, wZ + dz, 0, 0, 0, 0, 0, 0);
      float br = (bl > sl + 0.05f) ? bl : sl;
      if (selfLitFluid) br = 1.0f;
      float sideBr = selfLitFluid ? br : (br * 0.85f);
      uint32_t c = applyLightToFace(sideColor, sideBr);

      float u0 = uv.side_x * ts + eps, v0 = uv.side_y * ts + eps;
      float u1 = (uv.side_x + 1) * ts - eps;
      float faceH0 = h00, faceH1 = h10;
      if (dz == 1) { faceH0 = h11; faceH1 = h01; }
      if (dx == -1) { faceH0 = h01; faceH1 = h00; }
      if (dx == 1) { faceH0 = h10; faceH1 = h11; }
      float v1 = (uv.side_y + ((faceH0 + faceH1) * 0.5f)) * ts - eps;
      if (dz == -1) {
        fluidTess->addQuad(u0, v0, u1, v1, c, c, c, c,
                             wx + 1, wy + h10, wz,
                             wx, wy + h00, wz,
                             wx + 1, wy, wz,
                             wx, wy, wz);
        if (addSelfLitOverlay) {
          uint32_t ec = applyLightToFace(sideColor, 1.0f);
          m_emitTess->addQuad(u0, v0, u1, v1, ec, ec, ec, ec,
                               wx + 1, wy + h10, wz,
                               wx, wy + h00, wz,
                               wx + 1, wy, wz,
                               wx, wy, wz);
        }
      } else if (dz == 1) {
        fluidTess->addQuad(u0, v0, u1, v1, c, c, c, c,
                             wx, wy + h01, wz + 1,
                             wx + 1, wy + h11, wz + 1,
                             wx, wy, wz + 1,
                             wx + 1, wy, wz + 1);
        if (addSelfLitOverlay) {
          uint32_t ec = applyLightToFace(sideColor, 1.0f);
          m_emitTess->addQuad(u0, v0, u1, v1, ec, ec, ec, ec,
                               wx, wy + h01, wz + 1,
                               wx + 1, wy + h11, wz + 1,
                               wx, wy, wz + 1,
                               wx + 1, wy, wz + 1);
        }
      } else if (dx == -1) {
        fluidTess->addQuad(u0, v0, u1, v1, c, c, c, c,
                             wx, wy + h00, wz,
                             wx, wy + h01, wz + 1,
                             wx, wy, wz,
                             wx, wy, wz + 1);
        if (addSelfLitOverlay) {
          uint32_t ec = applyLightToFace(sideColor, 1.0f);
          m_emitTess->addQuad(u0, v0, u1, v1, ec, ec, ec, ec,
                               wx, wy + h00, wz,
                               wx, wy + h01, wz + 1,
                               wx, wy, wz,
                               wx, wy, wz + 1);
        }
      } else {
        fluidTess->addQuad(u0, v0, u1, v1, c, c, c, c,
                             wx + 1, wy + h11, wz + 1,
                             wx + 1, wy + h10, wz,
                             wx + 1, wy, wz + 1,
                             wx + 1, wy, wz);
        if (addSelfLitOverlay) {
          uint32_t ec = applyLightToFace(sideColor, 1.0f);
          m_emitTess->addQuad(u0, v0, u1, v1, ec, ec, ec, ec,
                               wx + 1, wy + h11, wz + 1,
                               wx + 1, wy + h10, wz,
                               wx + 1, wy, wz + 1,
                               wx + 1, wy, wz);
        }
      }
      drawn = true;
    };

    addSide(0, -1);
    addSide(0, 1);
    addSide(-1, 0);
    addSide(1, 0);
    return drawn;
  }

  if (isStairId(id)) {
    int facing = stairFacing(id);
    bool upsideDown = isUpsideDownStair(id);
    uint8_t baseId = stairBaseId(id);
    const BlockUV &uv = g_blockUV[baseId];
    float wx = (float)(cx * CHUNK_SIZE_X + lx);
    float wy = (float)ly;
    float wz = (float)(cz * CHUNK_SIZE_Z + lz);
    int wX = cx * CHUNK_SIZE_X + lx;
    int wY = ly;
    int wZ = cz * CHUNK_SIZE_Z + lz;
    const float ts = 1.0f / 16.0f;
    const float eps = 0.125f / 256.0f;
    bool drawn = false;
    bool isFancy = false;

    const float uTop0 = uv.top_x * ts + eps;
    const float vTop0 = uv.top_y * ts + eps;
    const float uTop1 = (uv.top_x + 1) * ts - eps;
    const float vTop1 = (uv.top_y + 1) * ts - eps;
    const float vTopHalf = vTop0 + (vTop1 - vTop0) * 0.5f;
    const float uSide0 = uv.side_x * ts + eps;
    const float vSide0 = uv.side_y * ts + eps;
    const float uSide1 = (uv.side_x + 1) * ts - eps;
    const float vSide1 = (uv.side_y + 1) * ts - eps;
    const float uSideHalf = uSide0 + (uSide1 - uSide0) * 0.5f;
    const float vSideHalf = vSide0 + (vSide1 - vSide0) * 0.5f;
    const float uBot0 = uv.bot_x * ts + eps;
    const float vBot0 = uv.bot_y * ts + eps;
    const float uBot1 = (uv.bot_x + 1) * ts - eps;
    const float vBot1 = (uv.bot_y + 1) * ts - eps;

    Tesselator *t = m_opaqueTess;
    auto rotateLocal = [&](float &x, float &z) {
      float lx0 = x - wx;
      float lz0 = z - wz;
      float rx = lx0, rz = lz0;
      if (facing == 1) { rx = 1.0f - lz0; rz = lx0; }
      else if (facing == 2) { rx = 1.0f - lx0; rz = 1.0f - lz0; }
      else if (facing == 3) { rx = lz0; rz = 1.0f - lx0; }
      x = wx + rx;
      z = wz + rz;
    };
    auto lpack = [&](uint32_t baseCol, int lx, int ly, int lz, int dx1, int dy1, int dz1, int dx2, int dy2, int dz2) -> uint64_t {
      float sl = getVertexSkyLight(lx, ly, lz, dx1, dy1, dz1, dx2, dy2, dz2);
      float bl = getVertexBlockLight(lx, ly, lz, dx1, dy1, dz1, dx2, dy2, dz2);
      uint32_t c = applyLightToFace(baseCol, sl);
      float em = bl * (1.0f - 0.75f * sl);
      if (em < 0.0f) em = 0.0f;
      uint32_t e = applyLightToFace(baseCol, em);
      return ((uint64_t)e << 32) | c;
    };
    auto addQuadRot = [&](float u0, float v0, float u1, float v1, uint64_t c00_p, uint64_t c10_p, uint64_t c01_p, uint64_t c11_p,
                          float x00, float y00, float z00, float x10, float y10, float z10,
                          float x01, float y01, float z01, float x11, float y11, float z11) {
      if (upsideDown) {
        y00 = wy + 1.0f - (y00 - wy);
        y10 = wy + 1.0f - (y10 - wy);
        y01 = wy + 1.0f - (y01 - wy);
        y11 = wy + 1.0f - (y11 - wy);
      }
      rotateLocal(x00, z00); rotateLocal(x10, z10); rotateLocal(x01, z01); rotateLocal(x11, z11);
      
      uint32_t c00 = (uint32_t)c00_p, c10 = (uint32_t)c10_p, c01 = (uint32_t)c01_p, c11 = (uint32_t)c11_p;
      if (upsideDown) {
        t->addQuadReversed(u0, v0, u1, v1, c00, c10, c01, c11, x00, y00, z00, x10, y10, z10, x01, y01, z01, x11, y11, z11);
      } else {
        t->addQuad(u0, v0, u1, v1, c00, c10, c01, c11, x00, y00, z00, x10, y10, z10, x01, y01, z01, x11, y11, z11);
      }
      uint32_t e00 = (uint32_t)(c00_p >> 32), e10 = (uint32_t)(c10_p >> 32), e01 = (uint32_t)(c01_p >> 32), e11 = (uint32_t)(c11_p >> 32);
      if ((e00 & 0xFFFFFF) || (e10 & 0xFFFFFF) || (e01 & 0xFFFFFF) || (e11 & 0xFFFFFF)) {
        if (upsideDown) {
          m_emitTess->addQuadReversed(u0, v0, u1, v1, e00, e10, e01, e11, x00, y00, z00, x10, y10, z10, x01, y01, z01, x11, y11, z11);
        } else {
          m_emitTess->addQuad(u0, v0, u1, v1, e00, e10, e01, e11, x00, y00, z00, x10, y10, z10, x01, y01, z01, x11, y11, z11);
        }
      }
    };
    auto stairNeedFace = [&](int dx, int dy, int dz) -> bool {
      int ndx = dx, ndz = dz;
      int ndy = upsideDown ? -dy : dy;
      if (facing == 1) { ndx = -dz; ndz = dx; }
      else if (facing == 2) { ndx = -dx; ndz = -dz; }
      else if (facing == 3) { ndx = dz; ndz = -dx; }
      return needFace(lx, ly, lz, cx, cz, id, ndx, ndy, ndz, isFancy);
    };

    // Per-vertex face colors (same sampling style as full blocks) to avoid
    // flat-looking stair lighting.
    uint64_t topC00 = lpack(LIGHT_TOP, wX, wY + 1, wZ, -1, 0, 0, 0, 0, -1);
    uint64_t topC10 = lpack(LIGHT_TOP, wX, wY + 1, wZ,  1, 0, 0, 0, 0, -1);
    uint64_t topC01 = lpack(LIGHT_TOP, wX, wY + 1, wZ, -1, 0, 0, 0, 0,  1);
    uint64_t topC11 = lpack(LIGHT_TOP, wX, wY + 1, wZ,  1, 0, 0, 0, 0,  1);
    uint64_t botC00 = lpack(LIGHT_BOT, wX, wY - 1, wZ, -1, 0, 0, 0, 0, -1);
    uint64_t botC10 = lpack(LIGHT_BOT, wX, wY - 1, wZ,  1, 0, 0, 0, 0, -1);
    uint64_t botC01 = lpack(LIGHT_BOT, wX, wY - 1, wZ, -1, 0, 0, 0, 0,  1);
    uint64_t botC11 = lpack(LIGHT_BOT, wX, wY - 1, wZ,  1, 0, 0, 0, 0,  1);
    uint64_t midTopC00 = lpack(LIGHT_TOP, wX, wY, wZ, -1, 0, 0, 0, 0, -1);
    uint64_t midTopC10 = lpack(LIGHT_TOP, wX, wY, wZ,  1, 0, 0, 0, 0, -1);
    uint64_t midTopC01 = lpack(LIGHT_TOP, wX, wY, wZ, -1, 0, 0, 0, 0,  1);
    uint64_t midTopC11 = lpack(LIGHT_TOP, wX, wY, wZ,  1, 0, 0, 0, 0,  1);
    uint64_t midBotC00 = lpack(LIGHT_BOT, wX, wY, wZ, -1, 0, 0, 0, 0, -1);
    uint64_t midBotC10 = lpack(LIGHT_BOT, wX, wY, wZ,  1, 0, 0, 0, 0, -1);
    uint64_t midBotC01 = lpack(LIGHT_BOT, wX, wY, wZ, -1, 0, 0, 0, 0,  1);
    uint64_t midBotC11 = lpack(LIGHT_BOT, wX, wY, wZ,  1, 0, 0, 0, 0,  1);

    const int sideTopY = upsideDown ? -1 : 1;
    const int sideBottomY = upsideDown ? 1 : -1;
    uint64_t northC10 = lpack(LIGHT_SIDE, wX, wY, wZ - 1,  1, 0, 0, 0, sideBottomY, 0);
    uint64_t northC00 = lpack(LIGHT_SIDE, wX, wY, wZ - 1, -1, 0, 0, 0, sideBottomY, 0);
    uint64_t midNorthC11 = lpack(LIGHT_SIDE, wX, wY, wZ,  1, 0, 0, 0, sideTopY, 0);
    uint64_t midNorthC01 = lpack(LIGHT_SIDE, wX, wY, wZ, -1, 0, 0, 0, sideTopY, 0);
    uint64_t midNorthC10 = lpack(LIGHT_SIDE, wX, wY, wZ,  1, 0, 0, 0, sideBottomY, 0);
    uint64_t midNorthC00 = lpack(LIGHT_SIDE, wX, wY, wZ, -1, 0, 0, 0, sideBottomY, 0);
    uint64_t southC01 = lpack(LIGHT_SIDE, wX, wY, wZ + 1, -1, 0, 0, 0, sideTopY, 0);
    uint64_t southC11 = lpack(LIGHT_SIDE, wX, wY, wZ + 1,  1, 0, 0, 0, sideTopY, 0);
    uint64_t southC00 = lpack(LIGHT_SIDE, wX, wY, wZ + 1, -1, 0, 0, 0, sideBottomY, 0);
    uint64_t southC10 = lpack(LIGHT_SIDE, wX, wY, wZ + 1,  1, 0, 0, 0, sideBottomY, 0);
    uint64_t westC01 = lpack(LIGHT_SIDE, wX - 1, wY, wZ, 0, sideTopY, 0, 0, 0,-1);
    uint64_t westC11 = lpack(LIGHT_SIDE, wX - 1, wY, wZ, 0, sideTopY, 0, 0, 0, 1);
    uint64_t westC00 = lpack(LIGHT_SIDE, wX - 1, wY, wZ, 0, sideBottomY, 0, 0, 0,-1);
    uint64_t westC10 = lpack(LIGHT_SIDE, wX - 1, wY, wZ, 0, sideBottomY, 0, 0, 0, 1);
    uint64_t eastC11 = lpack(LIGHT_SIDE, wX + 1, wY, wZ, 0, sideTopY, 0, 0, 0, 1);
    uint64_t eastC01 = lpack(LIGHT_SIDE, wX + 1, wY, wZ, 0, sideTopY, 0, 0, 0,-1);
    uint64_t eastC10 = lpack(LIGHT_SIDE, wX + 1, wY, wZ, 0, sideBottomY, 0, 0, 0, 1);
    uint64_t eastC00 = lpack(LIGHT_SIDE, wX + 1, wY, wZ, 0, sideBottomY, 0, 0, 0,-1);
    
    auto lerpPack = [&](uint64_t a, uint64_t b, float t) -> uint64_t {
      auto lerpHalf = [&](uint32_t c1, uint32_t c2) -> uint32_t {
        uint8_t aa=(c1>>24)&0xFF, ba=(c2>>24)&0xFF;
        uint8_t ab=(c1>>16)&0xFF, bb=(c2>>16)&0xFF;
        uint8_t ag=(c1>>8)&0xFF, bg=(c2>>8)&0xFF;
        uint8_t ar=c1&0xFF, br=c2&0xFF;
        return ((uint32_t)(aa+(ba-aa)*t)<<24)|((uint32_t)(ab+(bb-ab)*t)<<16)|((uint32_t)(ag+(bg-ag)*t)<<8)|(uint32_t)(ar+(br-ar)*t);
      };
      return ((uint64_t)lerpHalf((uint32_t)(a>>32), (uint32_t)(b>>32)) << 32) | lerpHalf((uint32_t)a, (uint32_t)b);
    };

    uint64_t westMidZ0 = lerpPack(westC00, westC01, 0.5f);
    uint64_t westMidZ1 = lerpPack(westC10, westC11, 0.5f);
    uint64_t eastMidZ1 = lerpPack(eastC10, eastC11, 0.5f);
    uint64_t eastMidZ0 = lerpPack(eastC00, eastC01, 0.5f);
    uint64_t northMidR = lerpPack(midNorthC10, midNorthC11, 0.5f);
    uint64_t northMidL = lerpPack(midNorthC00, midNorthC01, 0.5f);

    // For upside-down stairs Y-mirroring swaps which horizontal planes are
    // exposed to the player: use top-light for the exposed top and bottom-light
    // for the hidden underside.
    uint64_t stepTopC00 = upsideDown ? midBotC00 : midTopC00;
    uint64_t stepTopC10 = upsideDown ? midBotC10 : midTopC10;
    uint64_t stepTopC01 = upsideDown ? midBotC01 : midTopC01;
    uint64_t stepTopC11 = upsideDown ? midBotC11 : midTopC11;
    uint64_t stepBottomC01 = upsideDown ? topC01 : botC01;
    uint64_t stepBottomC11 = upsideDown ? topC11 : botC11;
    uint64_t stepBottomC00 = upsideDown ? topC00 : botC00;
    uint64_t stepBottomC10 = upsideDown ? topC10 : botC10;

    // Top of lower step (front half, y = 0.5, z:0..0.5)
    if (stairNeedFace(0, 1, 0)) {
      addQuadRot(uTop0, vTop0, uTop1, vTopHalf, stepTopC00, stepTopC10, stepTopC01, stepTopC11,
                 wx, wy + 0.5f, wz, wx + 1.0f, wy + 0.5f, wz,
                 wx, wy + 0.5f, wz + 0.5f, wx + 1.0f, wy + 0.5f, wz + 0.5f);
      drawn = true;
    }

    // Top of upper step (rear half, z:0.5..1.0, y = 1.0)
    if (stairNeedFace(0, 1, 0)) {
      addQuadRot(uTop0, vTopHalf, uTop1, vTop1, stepTopC00, stepTopC10, stepTopC01, stepTopC11,
                 wx, wy + 1.0f, wz + 0.5f, wx + 1.0f, wy + 1.0f, wz + 0.5f,
                 wx, wy + 1.0f, wz + 1.0f, wx + 1.0f, wy + 1.0f, wz + 1.0f);
      drawn = true;
    }

    // Bottom
    if (stairNeedFace(0, -1, 0)) {
      addQuadRot(uBot0, vBot0, uBot1, vBot1, stepBottomC01, stepBottomC11, stepBottomC00, stepBottomC10,
                 wx, wy, wz + 1.0f, wx + 1.0f, wy, wz + 1.0f,
                 wx, wy, wz, wx + 1.0f, wy, wz);
      drawn = true;
    }

    // North face (front): half height
    if (stairNeedFace(0, 0, -1)) {
      addQuadRot(uSide0, vSideHalf, uSide1, vSide1, northMidR, northMidL, northC10, northC00,
                 wx + 1.0f, wy + 0.5f, wz, wx, wy + 0.5f, wz,
                 wx + 1.0f, wy, wz, wx, wy, wz);
      drawn = true;
    }

    // South face (rear): full height
    if (stairNeedFace(0, 0, 1)) {
      addQuadRot(uSide0, vSide0, uSide1, vSide1, southC01, southC11, southC00, southC10,
                 wx, wy + 1.0f, wz + 1.0f, wx + 1.0f, wy + 1.0f, wz + 1.0f,
                 wx, wy, wz + 1.0f, wx + 1.0f, wy, wz + 1.0f);
      drawn = true;
    }

    // West face lower
    if (stairNeedFace(-1, 0, 0)) {
      addQuadRot(uSide0, vSideHalf, uSide1, vSide1, westMidZ0, westMidZ1, westC00, westC10,
                 wx, wy + 0.5f, wz, wx, wy + 0.5f, wz + 1.0f,
                 wx, wy, wz, wx, wy, wz + 1.0f);
      // West face upper rear half
      addQuadRot(uSideHalf, vSide0, uSide1, vSideHalf, westC01, westC11, westMidZ0, westMidZ1,
                 wx, wy + 1.0f, wz + 0.5f, wx, wy + 1.0f, wz + 1.0f,
                 wx, wy + 0.5f, wz + 0.5f, wx, wy + 0.5f, wz + 1.0f);
      drawn = true;
    }

    // East face lower
    if (stairNeedFace(1, 0, 0)) {
      addQuadRot(uSide0, vSideHalf, uSide1, vSide1, eastMidZ1, eastMidZ0, eastC10, eastC00,
                 wx + 1.0f, wy + 0.5f, wz + 1.0f, wx + 1.0f, wy + 0.5f, wz,
                 wx + 1.0f, wy, wz + 1.0f, wx + 1.0f, wy, wz);
      // East face upper rear half
      addQuadRot(uSideHalf, vSide0, uSide1, vSideHalf, eastC11, eastC01, eastMidZ1, eastMidZ0,
                 wx + 1.0f, wy + 1.0f, wz + 1.0f, wx + 1.0f, wy + 1.0f, wz + 0.5f,
                 wx + 1.0f, wy + 0.5f, wz + 1.0f, wx + 1.0f, wy + 0.5f, wz + 0.5f);
      drawn = true;
    }

    // Internal riser at z = 0.5
    addQuadRot(uSide0, vSide0, uSide1, vSideHalf, midNorthC11, midNorthC01, northMidR, northMidL,
               wx + 1.0f, wy + 1.0f, wz + 0.5f, wx, wy + 1.0f, wz + 0.5f,
               wx + 1.0f, wy + 0.5f, wz + 0.5f, wx, wy + 0.5f, wz + 0.5f);
    drawn = true;

    return drawn;
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

  // Select tesselator by material. Keep solid faces in opaque/fancy passes;
  // routing them to emissive pass by "dominant block light" causes
  // disappearing/unstable neighbors around light sources on PSP.
  auto pickTess = [&](Tesselator *skyTess, Tesselator *fncTess,
                      float skyL, float blkL, bool fancy) -> Tesselator * {
    (void)skyL;
    (void)blkL;
    if (g_blockProps[id].isTransparent()) {
      return fancy ? fncTess : m_transTess;
    }
    return skyTess;
  };
  const bool selfLitBlock = (g_blockProps[id].light_emit > 0);
  const bool transparentBlock = g_blockProps[id].isTransparent();
  auto emitOnly = [&](float blkL, float skyL) -> float {
    // Keep intrinsically emissive and transparent/cutout blocks clearly lit.
    if (selfLitBlock || transparentBlock) {
      float e = blkL * 1.35f;
      return e > 1.0f ? 1.0f : e;
    }
    // For opaque non-emissive blocks, strongly damp in skylight to avoid
    // additive overexposure when both sun and block light are strong.
    float e = blkL * (1.0f - 0.75f * skyL);
    return e > 0.0f ? e : 0.0f;
  };
  // 4J logic to avoid Z-fighting on inner leaves is handled in per-face code
  float xMin = wx, xMax = wx + 1.0f;
  float yMin = wy, yMax = wy + 1.0f;
  float zMin = wz, zMax = wz + 1.0f;
  const float fullXMin = wx, fullXMax = wx + 1.0f;
  const float fullZMin = wz, fullZMax = wz + 1.0f;
  if (id == BLOCK_CACTUS || isSlabBlock(id)) {
    const BlockProps &bp = g_blockProps[id];
    xMin = wx + bp.minX; xMax = wx + bp.maxX;
    yMin = wy + bp.minY; yMax = wy + bp.maxY;
    zMin = wz + bp.minZ; zMax = wz + bp.maxZ;
  }
  const float cactusPx = ts / 16.0f; // one texel inside a 16x16 tile
  const float cactusStripW = 1.0f / 16.0f;
  const float cactusThornDepth = 1.0f / 64.0f;

  // TOP (+Y)
  if (needFace(lx, ly, lz, cx, cz, id, 0, 1, 0, isFancy)) {
    int lyOffset = (yMax < wy + 1.0f - 0.001f) ? 0 : 1;
    float sl00 = getVertexSkyLight(wX, wY+lyOffset, wZ, -1,0,0, 0,0,-1);
    float sl10 = getVertexSkyLight(wX, wY+lyOffset, wZ,  1,0,0, 0,0,-1);
    float sl01 = getVertexSkyLight(wX, wY+lyOffset, wZ, -1,0,0, 0,0, 1);
    float sl11 = getVertexSkyLight(wX, wY+lyOffset, wZ,  1,0,0, 0,0, 1);
    float bl00 = getVertexBlockLight(wX, wY+lyOffset, wZ, -1,0,0, 0,0,-1);
    float bl10 = getVertexBlockLight(wX, wY+lyOffset, wZ,  1,0,0, 0,0,-1);
    float bl01 = getVertexBlockLight(wX, wY+lyOffset, wZ, -1,0,0, 0,0, 1);
    float bl11 = getVertexBlockLight(wX, wY+lyOffset, wZ,  1,0,0, 0,0, 1);
    float avgBlk = (bl00+bl10+bl01+bl11)*0.25f;
    float avgSky = (sl00+sl10+sl01+sl11)*0.25f;

    Tesselator *t = pickTess(m_opaqueTess, m_fancyTess, avgSky, avgBlk, isFancy);
    uint32_t c00 = applyLightToFace(LIGHT_TOP, sl00);
    uint32_t c10 = applyLightToFace(LIGHT_TOP, sl10);
    uint32_t c01 = applyLightToFace(LIGHT_TOP, sl01);
    uint32_t c11 = applyLightToFace(LIGHT_TOP, sl11);
    float el00 = emitOnly(bl00, sl00);
    float el10 = emitOnly(bl10, sl10);
    float el01 = emitOnly(bl01, sl01);
    float el11 = emitOnly(bl11, sl11);
    float avgEmit = (el00 + el10 + el01 + el11) * 0.25f;
    uint32_t e00 = applyLightToFace(LIGHT_TOP, el00);
    uint32_t e10 = applyLightToFace(LIGHT_TOP, el10);
    uint32_t e01 = applyLightToFace(LIGHT_TOP, el01);
    uint32_t e11 = applyLightToFace(LIGHT_TOP, el11);

    float u0 = uv.top_x*ts+eps, v0 = uv.top_y*ts+eps;
    float u1 = (uv.top_x+1)*ts-eps, v1 = (uv.top_y+1)*ts-eps;
    float off = isFancy ? 0.005f : 0.0f;
    float tx0 = (id == BLOCK_CACTUS) ? fullXMin : xMin;
    float tx1 = (id == BLOCK_CACTUS) ? fullXMax : xMax;
    float tz0 = (id == BLOCK_CACTUS) ? fullZMin : zMin;
    float tz1 = (id == BLOCK_CACTUS) ? fullZMax : zMax;
    t->addQuad(u0,v0,u1,v1, c00,c10,c01,c11,
               tx0+off,yMax-off,tz0+off, tx1-off,yMax-off,tz0+off, tx0+off,yMax-off,tz1-off, tx1-off,yMax-off,tz1-off);
    if (avgEmit > 0.001f && !(id == BLOCK_LEAVES && isFancy)) {
      m_emitTess->addQuad(u0,v0,u1,v1, e00,e10,e01,e11,
                          tx0+off,yMax-off,tz0+off, tx1-off,yMax-off,tz0+off, tx0+off,yMax-off,tz1-off, tx1-off,yMax-off,tz1-off);
    }
    drawn = true;
  }

  // BOTTOM (-Y)
  if (needFace(lx, ly, lz, cx, cz, id, 0, -1, 0, isFancy)) {
    int lyOffset = (yMin > wy + 0.001f) ? 0 : -1;
    float sl00 = getVertexSkyLight(wX, wY+lyOffset, wZ, -1,0,0, 0,0,-1);
    float sl10 = getVertexSkyLight(wX, wY+lyOffset, wZ,  1,0,0, 0,0,-1);
    float sl01 = getVertexSkyLight(wX, wY+lyOffset, wZ, -1,0,0, 0,0, 1);
    float sl11 = getVertexSkyLight(wX, wY+lyOffset, wZ,  1,0,0, 0,0, 1);
    float bl00 = getVertexBlockLight(wX, wY+lyOffset, wZ, -1,0,0, 0,0,-1);
    float bl10 = getVertexBlockLight(wX, wY+lyOffset, wZ,  1,0,0, 0,0,-1);
    float bl01 = getVertexBlockLight(wX, wY+lyOffset, wZ, -1,0,0, 0,0, 1);
    float bl11 = getVertexBlockLight(wX, wY+lyOffset, wZ,  1,0,0, 0,0, 1);
    float avgBlk=(bl00+bl10+bl01+bl11)*0.25f, avgSky=(sl00+sl10+sl01+sl11)*0.25f;

    Tesselator *t = pickTess(m_opaqueTess, m_fancyTess, avgSky, avgBlk, isFancy);
    uint32_t c00=applyLightToFace(LIGHT_BOT, sl00);
    uint32_t c10=applyLightToFace(LIGHT_BOT, sl10);
    uint32_t c01=applyLightToFace(LIGHT_BOT, sl01);
    uint32_t c11=applyLightToFace(LIGHT_BOT, sl11);
    float el00=emitOnly(bl00, sl00);
    float el10=emitOnly(bl10, sl10);
    float el01=emitOnly(bl01, sl01);
    float el11=emitOnly(bl11, sl11);
    float avgEmit=(el00+el10+el01+el11)*0.25f;
    uint32_t e00=applyLightToFace(LIGHT_BOT, el00);
    uint32_t e10=applyLightToFace(LIGHT_BOT, el10);
    uint32_t e01=applyLightToFace(LIGHT_BOT, el01);
    uint32_t e11=applyLightToFace(LIGHT_BOT, el11);

    float u0=uv.bot_x*ts+eps, v0=uv.bot_y*ts+eps;
    float u1=(uv.bot_x+1)*ts-eps, v1=(uv.bot_y+1)*ts-eps;
    float off = isFancy ? 0.005f : 0.0f;
    float bx0 = (id == BLOCK_CACTUS) ? fullXMin : xMin;
    float bx1 = (id == BLOCK_CACTUS) ? fullXMax : xMax;
    float bz0 = (id == BLOCK_CACTUS) ? fullZMin : zMin;
    float bz1 = (id == BLOCK_CACTUS) ? fullZMax : zMax;
    t->addQuad(u0,v0,u1,v1, c01,c11,c00,c10,
               bx0+off,yMin+off,bz1-off, bx1-off,yMin+off,bz1-off, bx0+off,yMin+off,bz0+off, bx1-off,yMin+off,bz0+off);
    if (avgEmit > 0.001f && !(id == BLOCK_LEAVES && isFancy)) {
      m_emitTess->addQuad(u0,v0,u1,v1, e01,e11,e00,e10,
                          bx0+off,yMin+off,bz1-off, bx1-off,yMin+off,bz1-off, bx0+off,yMin+off,bz0+off, bx1-off,yMin+off,bz0+off);
    }
    drawn = true;
  }

  // NORTH (-Z)
  if (needFace(lx, ly, lz, cx, cz, id, 0, 0, -1, isFancy)) {
    int lzOffset = (zMin > wz + 0.001f) ? 0 : -1;
    float sl11=getVertexSkyLight(wX,wY,wZ+lzOffset, 1,0,0, 0,1,0);
    float sl01=getVertexSkyLight(wX,wY,wZ+lzOffset,-1,0,0, 0,1,0);
    float sl10=getVertexSkyLight(wX,wY,wZ+lzOffset, 1,0,0, 0,-1,0);
    float sl00=getVertexSkyLight(wX,wY,wZ+lzOffset,-1,0,0, 0,-1,0);
    float bl11=getVertexBlockLight(wX,wY,wZ+lzOffset, 1,0,0, 0,1,0);
    float bl01=getVertexBlockLight(wX,wY,wZ+lzOffset,-1,0,0, 0,1,0);
    float bl10=getVertexBlockLight(wX,wY,wZ+lzOffset, 1,0,0, 0,-1,0);
    float bl00=getVertexBlockLight(wX,wY,wZ+lzOffset,-1,0,0, 0,-1,0);
    float avgBlk=(bl11+bl01+bl10+bl00)*0.25f, avgSky=(sl11+sl01+sl10+sl00)*0.25f;

    Tesselator *t=pickTess(m_opaqueTess,m_fancyTess,avgSky,avgBlk,isFancy);
    uint32_t c11=applyLightToFace(LIGHT_SIDE, sl11);
    uint32_t c01=applyLightToFace(LIGHT_SIDE, sl01);
    uint32_t c10=applyLightToFace(LIGHT_SIDE, sl10);
    uint32_t c00=applyLightToFace(LIGHT_SIDE, sl00);
    float el11=emitOnly(bl11, sl11);
    float el01=emitOnly(bl01, sl01);
    float el10=emitOnly(bl10, sl10);
    float el00=emitOnly(bl00, sl00);
    float avgEmit=(el11+el01+el10+el00)*0.25f;
    uint32_t e11=applyLightToFace(LIGHT_SIDE, el11);
    uint32_t e01=applyLightToFace(LIGHT_SIDE, el01);
    uint32_t e10=applyLightToFace(LIGHT_SIDE, el10);
    uint32_t e00=applyLightToFace(LIGHT_SIDE, el00);

    float u0=uv.side_x*ts+eps, v0=uv.side_y*ts+eps;
    float u1=(uv.side_x+1)*ts-eps, v1=(uv.side_y+1)*ts-eps;
    if (isSlabBlock(id)) v1 = v0 + (v1 - v0) * 0.5f;
    float mu0 = u0, mu1 = u1;
    float lu0 = u0, lu1 = u0, ru0 = u1, ru1 = u1;
    if (id == BLOCK_CACTUS) {
      float tileU0 = uv.side_x * ts;
      float tileU1 = (uv.side_x + 1) * ts;
      mu0 = tileU0 + cactusPx;
      mu1 = tileU1 - cactusPx;
      lu0 = tileU0;
      lu1 = tileU0 + cactusPx;
      ru0 = tileU1 - cactusPx;
      ru1 = tileU1;
    }
    float off = isFancy ? 0.005f : 0.0f;
    t->addQuad(mu0,v0,mu1,v1, c11,c01,c10,c00,
               xMax-off,yMax-off,zMin+off, xMin+off,yMax-off,zMin+off, xMax-off,yMin+off,zMin+off, xMin+off,yMin+off,zMin+off);
    if (avgEmit > 0.001f && !(id == BLOCK_LEAVES && isFancy)) {
      m_emitTess->addQuad(mu0,v0,mu1,v1, e11,e01,e10,e00,
                          xMax-off,yMax-off,zMin+off, xMin+off,yMax-off,zMin+off, xMax-off,yMin+off,zMin+off, xMin+off,yMin+off,zMin+off);
    }
    if (id == BLOCK_CACTUS) {
      float zThorn = zMin - cactusThornDepth + off;
      t->addQuad(lu0,v0,lu1,v1, c11,c01,c10,c00,
                 fullXMin + cactusStripW, yMax-off, zThorn, fullXMin, yMax-off, zThorn, fullXMin + cactusStripW, yMin+off, zThorn, fullXMin, yMin+off, zThorn);
      t->addQuad(ru0,v0,ru1,v1, c11,c01,c10,c00,
                 fullXMax, yMax-off, zThorn, fullXMax - cactusStripW, yMax-off, zThorn, fullXMax, yMin+off, zThorn, fullXMax - cactusStripW, yMin+off, zThorn);
    }
    drawn = true;
  }

  // SOUTH (+Z)
  if (needFace(lx, ly, lz, cx, cz, id, 0, 0, 1, isFancy)) {
    int lzOffset = (zMax < wz + 1.0f - 0.001f) ? 0 : 1;
    float sl01=getVertexSkyLight(wX,wY,wZ+lzOffset,-1,0,0, 0,1,0);
    float sl11=getVertexSkyLight(wX,wY,wZ+lzOffset, 1,0,0, 0,1,0);
    float sl00=getVertexSkyLight(wX,wY,wZ+lzOffset,-1,0,0, 0,-1,0);
    float sl10=getVertexSkyLight(wX,wY,wZ+lzOffset, 1,0,0, 0,-1,0);
    float bl01=getVertexBlockLight(wX,wY,wZ+lzOffset,-1,0,0, 0,1,0);
    float bl11=getVertexBlockLight(wX,wY,wZ+lzOffset, 1,0,0, 0,1,0);
    float bl00=getVertexBlockLight(wX,wY,wZ+lzOffset,-1,0,0, 0,-1,0);
    float bl10=getVertexBlockLight(wX,wY,wZ+lzOffset, 1,0,0, 0,-1,0);
    float avgBlk=(bl01+bl11+bl00+bl10)*0.25f, avgSky=(sl01+sl11+sl00+sl10)*0.25f;

    Tesselator *t=pickTess(m_opaqueTess,m_fancyTess,avgSky,avgBlk,isFancy);
    uint32_t c01=applyLightToFace(LIGHT_SIDE, sl01);
    uint32_t c11=applyLightToFace(LIGHT_SIDE, sl11);
    uint32_t c00=applyLightToFace(LIGHT_SIDE, sl00);
    uint32_t c10=applyLightToFace(LIGHT_SIDE, sl10);
    float el01=emitOnly(bl01, sl01);
    float el11=emitOnly(bl11, sl11);
    float el00=emitOnly(bl00, sl00);
    float el10=emitOnly(bl10, sl10);
    float avgEmit=(el01+el11+el00+el10)*0.25f;
    uint32_t e01=applyLightToFace(LIGHT_SIDE, el01);
    uint32_t e11=applyLightToFace(LIGHT_SIDE, el11);
    uint32_t e00=applyLightToFace(LIGHT_SIDE, el00);
    uint32_t e10=applyLightToFace(LIGHT_SIDE, el10);

    float u0=uv.side_x*ts+eps, v0=uv.side_y*ts+eps;
    float u1=(uv.side_x+1)*ts-eps, v1=(uv.side_y+1)*ts-eps;
    if (isSlabBlock(id)) v1 = v0 + (v1 - v0) * 0.5f;
    float mu0 = u0, mu1 = u1;
    float lu0 = u0, lu1 = u0, ru0 = u1, ru1 = u1;
    if (id == BLOCK_CACTUS) {
      float tileU0 = uv.side_x * ts;
      float tileU1 = (uv.side_x + 1) * ts;
      mu0 = tileU0 + cactusPx;
      mu1 = tileU1 - cactusPx;
      lu0 = tileU0;
      lu1 = tileU0 + cactusPx;
      ru0 = tileU1 - cactusPx;
      ru1 = tileU1;
    }
    float off = isFancy ? 0.005f : 0.0f;
    t->addQuad(mu0,v0,mu1,v1, c01,c11,c00,c10,
               xMin+off,yMax-off,zMax-off, xMax-off,yMax-off,zMax-off, xMin+off,yMin+off,zMax-off, xMax-off,yMin+off,zMax-off);
    if (avgEmit > 0.001f && !(id == BLOCK_LEAVES && isFancy)) {
      m_emitTess->addQuad(mu0,v0,mu1,v1, e01,e11,e00,e10,
                          xMin+off,yMax-off,zMax-off, xMax-off,yMax-off,zMax-off, xMin+off,yMin+off,zMax-off, xMax-off,yMin+off,zMax-off);
    }
    if (id == BLOCK_CACTUS) {
      float zThorn = zMax + cactusThornDepth - off;
      t->addQuad(lu0,v0,lu1,v1, c01,c11,c00,c10,
                 fullXMax - cactusStripW, yMax-off, zThorn, fullXMax, yMax-off, zThorn, fullXMax - cactusStripW, yMin+off, zThorn, fullXMax, yMin+off, zThorn);
      t->addQuad(ru0,v0,ru1,v1, c01,c11,c00,c10,
                 fullXMin, yMax-off, zThorn, fullXMin + cactusStripW, yMax-off, zThorn, fullXMin, yMin+off, zThorn, fullXMin + cactusStripW, yMin+off, zThorn);
    }
    drawn = true;
  }

  // WEST (-X)
  if (needFace(lx, ly, lz, cx, cz, id, -1, 0, 0, isFancy)) {
    int lxOffset = (xMin > wx + 0.001f) ? 0 : -1;
    float sl01=getVertexSkyLight(wX+lxOffset,wY,wZ, 0,1,0, 0,0,-1);
    float sl11=getVertexSkyLight(wX+lxOffset,wY,wZ, 0,1,0, 0,0, 1);
    float sl00=getVertexSkyLight(wX+lxOffset,wY,wZ, 0,-1,0, 0,0,-1);
    float sl10=getVertexSkyLight(wX+lxOffset,wY,wZ, 0,-1,0, 0,0, 1);
    float bl01=getVertexBlockLight(wX+lxOffset,wY,wZ, 0,1,0, 0,0,-1);
    float bl11=getVertexBlockLight(wX+lxOffset,wY,wZ, 0,1,0, 0,0, 1);
    float bl00=getVertexBlockLight(wX+lxOffset,wY,wZ, 0,-1,0, 0,0,-1);
    float bl10=getVertexBlockLight(wX+lxOffset,wY,wZ, 0,-1,0, 0,0, 1);
    float avgBlk=(bl01+bl11+bl00+bl10)*0.25f, avgSky=(sl01+sl11+sl00+sl10)*0.25f;

    Tesselator *t=pickTess(m_opaqueTess,m_fancyTess,avgSky,avgBlk,isFancy);
    uint32_t c01=applyLightToFace(LIGHT_SIDE, sl01);
    uint32_t c11=applyLightToFace(LIGHT_SIDE, sl11);
    uint32_t c00=applyLightToFace(LIGHT_SIDE, sl00);
    uint32_t c10=applyLightToFace(LIGHT_SIDE, sl10);
    float el01=emitOnly(bl01, sl01);
    float el11=emitOnly(bl11, sl11);
    float el00=emitOnly(bl00, sl00);
    float el10=emitOnly(bl10, sl10);
    float avgEmit=(el01+el11+el00+el10)*0.25f;
    uint32_t e01=applyLightToFace(LIGHT_SIDE, el01);
    uint32_t e11=applyLightToFace(LIGHT_SIDE, el11);
    uint32_t e00=applyLightToFace(LIGHT_SIDE, el00);
    uint32_t e10=applyLightToFace(LIGHT_SIDE, el10);

    float u0=uv.side_x*ts+eps, v0=uv.side_y*ts+eps;
    float u1=(uv.side_x+1)*ts-eps, v1=(uv.side_y+1)*ts-eps;
    if (isSlabBlock(id)) v1 = v0 + (v1 - v0) * 0.5f;
    float mu0 = u0, mu1 = u1;
    float lu0 = u0, lu1 = u0, ru0 = u1, ru1 = u1;
    if (id == BLOCK_CACTUS) {
      float tileU0 = uv.side_x * ts;
      float tileU1 = (uv.side_x + 1) * ts;
      mu0 = tileU0 + cactusPx;
      mu1 = tileU1 - cactusPx;
      lu0 = tileU0;
      lu1 = tileU0 + cactusPx;
      ru0 = tileU1 - cactusPx;
      ru1 = tileU1;
    }
    float off = isFancy ? 0.005f : 0.0f;
    t->addQuad(mu0,v0,mu1,v1, c01,c11,c00,c10,
               xMin+off,yMax-off,zMin+off, xMin+off,yMax-off,zMax-off, xMin+off,yMin+off,zMin+off, xMin+off,yMin+off,zMax-off);
    if (avgEmit > 0.001f && !(id == BLOCK_LEAVES && isFancy)) {
      m_emitTess->addQuad(mu0,v0,mu1,v1, e01,e11,e00,e10,
                          xMin+off,yMax-off,zMin+off, xMin+off,yMax-off,zMax-off, xMin+off,yMin+off,zMin+off, xMin+off,yMin+off,zMax-off);
    }
    if (id == BLOCK_CACTUS) {
      float xThorn = xMin - cactusThornDepth + off;
      t->addQuad(lu0,v0,lu1,v1, c01,c11,c00,c10,
                 xThorn, yMax-off, fullZMax - cactusStripW, xThorn, yMax-off, fullZMax, xThorn, yMin+off, fullZMax - cactusStripW, xThorn, yMin+off, fullZMax);
      t->addQuad(ru0,v0,ru1,v1, c01,c11,c00,c10,
                 xThorn, yMax-off, fullZMin, xThorn, yMax-off, fullZMin + cactusStripW, xThorn, yMin+off, fullZMin, xThorn, yMin+off, fullZMin + cactusStripW);
    }
    drawn = true;
  }

  // EAST (+X)
  if (needFace(lx, ly, lz, cx, cz, id, 1, 0, 0, isFancy)) {
    int lxOffset = (xMax < wx + 1.0f - 0.001f) ? 0 : 1;
    float sl11=getVertexSkyLight(wX+lxOffset,wY,wZ, 0,1,0, 0,0, 1);
    float sl01=getVertexSkyLight(wX+lxOffset,wY,wZ, 0,1,0, 0,0,-1);
    float sl10=getVertexSkyLight(wX+lxOffset,wY,wZ, 0,-1,0, 0,0, 1);
    float sl00=getVertexSkyLight(wX+lxOffset,wY,wZ, 0,-1,0, 0,0,-1);
    float bl11=getVertexBlockLight(wX+lxOffset,wY,wZ, 0,1,0, 0,0, 1);
    float bl01=getVertexBlockLight(wX+lxOffset,wY,wZ, 0,1,0, 0,0,-1);
    float bl10=getVertexBlockLight(wX+lxOffset,wY,wZ, 0,-1,0, 0,0, 1);
    float bl00=getVertexBlockLight(wX+lxOffset,wY,wZ, 0,-1,0, 0,0,-1);
    float avgBlk=(bl11+bl01+bl10+bl00)*0.25f, avgSky=(sl11+sl01+sl10+sl00)*0.25f;

    Tesselator *t=pickTess(m_opaqueTess,m_fancyTess,avgSky,avgBlk,isFancy);
    uint32_t c11=applyLightToFace(LIGHT_SIDE, sl11);
    uint32_t c01=applyLightToFace(LIGHT_SIDE, sl01);
    uint32_t c10=applyLightToFace(LIGHT_SIDE, sl10);
    uint32_t c00=applyLightToFace(LIGHT_SIDE, sl00);
    float el11=emitOnly(bl11, sl11);
    float el01=emitOnly(bl01, sl01);
    float el10=emitOnly(bl10, sl10);
    float el00=emitOnly(bl00, sl00);
    float avgEmit=(el11+el01+el10+el00)*0.25f;
    uint32_t e11=applyLightToFace(LIGHT_SIDE, el11);
    uint32_t e01=applyLightToFace(LIGHT_SIDE, el01);
    uint32_t e10=applyLightToFace(LIGHT_SIDE, el10);
    uint32_t e00=applyLightToFace(LIGHT_SIDE, el00);

    float u0=uv.side_x*ts+eps, v0=uv.side_y*ts+eps;
    float u1=(uv.side_x+1)*ts-eps, v1=(uv.side_y+1)*ts-eps;
    if (isSlabBlock(id)) v1 = v0 + (v1 - v0) * 0.5f;
    float mu0 = u0, mu1 = u1;
    float lu0 = u0, lu1 = u0, ru0 = u1, ru1 = u1;
    if (id == BLOCK_CACTUS) {
      float tileU0 = uv.side_x * ts;
      float tileU1 = (uv.side_x + 1) * ts;
      mu0 = tileU0 + cactusPx;
      mu1 = tileU1 - cactusPx;
      lu0 = tileU0;
      lu1 = tileU0 + cactusPx;
      ru0 = tileU1 - cactusPx;
      ru1 = tileU1;
    }
    float off = isFancy ? 0.005f : 0.0f;
    t->addQuad(mu0,v0,mu1,v1, c11,c01,c10,c00,
               xMax-off,yMax-off,zMax-off, xMax-off,yMax-off,zMin+off, xMax-off,yMin+off,zMax-off, xMax-off,yMin+off,zMin+off);
    if (avgEmit > 0.001f && !(id == BLOCK_LEAVES && isFancy)) {
      m_emitTess->addQuad(mu0,v0,mu1,v1, e11,e01,e10,e00,
                          xMax-off,yMax-off,zMax-off, xMax-off,yMax-off,zMin+off, xMax-off,yMin+off,zMax-off, xMax-off,yMin+off,zMin+off);
    }
    if (id == BLOCK_CACTUS) {
      float xThorn = xMax + cactusThornDepth - off;
      t->addQuad(lu0,v0,lu1,v1, c11,c01,c10,c00,
                 xThorn, yMax-off, fullZMin + cactusStripW, xThorn, yMax-off, fullZMin, xThorn, yMin+off, fullZMin + cactusStripW, xThorn, yMin+off, fullZMin);
      t->addQuad(ru0,v0,ru1,v1, c11,c01,c10,c00,
                 xThorn, yMax-off, fullZMax, xThorn, yMax-off, fullZMax - cactusStripW, xThorn, yMin+off, fullZMax, xThorn, yMin+off, fullZMax - cactusStripW);
    }
    drawn = true;
  }

  return drawn;
}
