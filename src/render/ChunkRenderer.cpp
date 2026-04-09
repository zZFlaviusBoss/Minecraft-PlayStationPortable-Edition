// ChunkRenderer.cpp
#include "ChunkRenderer.h"
#include "../math/Frustum.h"
#include "PSPRenderer.h"
#include "Tesselator.h"
#include "TileRenderer.h"
#include <malloc.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <string.h>

#define MAX_VERTS_PER_SUB_CHUNK 8000

static CraftPSPVertex g_opaqueBuf[4][MAX_VERTS_PER_SUB_CHUNK];
static CraftPSPVertex g_transBuf[4][MAX_VERTS_PER_SUB_CHUNK];
static CraftPSPVertex g_transFancyBuf[4][MAX_VERTS_PER_SUB_CHUNK];
static CraftPSPVertex g_waterBuf[4][MAX_VERTS_PER_SUB_CHUNK];
static CraftPSPVertex g_emitBuf[4][MAX_VERTS_PER_SUB_CHUNK];

ChunkRenderer::ChunkRenderer(TextureAtlas *atlas)
    : m_level(nullptr), m_atlas(atlas), m_compileStep(0), m_compileChunk(nullptr), m_compileSy(-1) {}

ChunkRenderer::~ChunkRenderer() {}

void ChunkRenderer::setLevel(Level *level) { m_level = level; }

#include <psprtc.h>

void ChunkRenderer::processCompileQueue(float camX, float camY, float camZ) {
  if (!m_level)
    return;

  // Limit compile time
  uint64_t tickStart;
  sceRtcGetCurrentTick(&tickStart);
  uint32_t tickRes = sceRtcGetTickResolution();
  
  while (true) {
    uint64_t currentTick;
    sceRtcGetCurrentTick(&currentTick);
    float elapsedMs = (float)(currentTick - tickStart) / ((float)tickRes / 1000.0f);
    if (elapsedMs > 4.0f) {
      break;
    }

  if (m_compileStep == 0) {
    // Find closest dirty subchunk
    Chunk *closestDirty = nullptr;
    int closestDirtySy = -1;

    for (int cx = 0; cx < WORLD_CHUNKS_X; cx++) {
      for (int cz = 0; cz < WORLD_CHUNKS_Z; cz++) {
        Chunk *c = m_level->getChunk(cx, cz);
        if (c) {
          for (int sy = 0; sy < 4; sy++) {
            if (c->dirty[sy]) {
              float chunkCenterX = c->cx * CHUNK_SIZE_X + CHUNK_SIZE_X / 2.0f;
              float chunkCenterZ = c->cz * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2.0f;
              float chunkCenterY = sy * 16 + 8.0f;
              float dx = chunkCenterX - camX;
              float dy = chunkCenterY - camY;
              float dz = chunkCenterZ - camZ;
              float distSq = dx * dx + dy * dy + dz * dz;
              
              // Score-based selection: priority overrides distance
              float score = (float)c->priority[sy] * 1000000.0f - distSq;
              static float bestScore = -99999999.0f;
              if (closestDirty == nullptr) bestScore = -99999999.0f;

              if (score > bestScore) {
                bestScore = score;
                closestDirty = c;
                closestDirtySy = sy;
              }
            }
          }
        }
      }
    }

    if (closestDirty) {
      m_compileChunk = closestDirty;
      m_compileSy = closestDirtySy;
      m_compileStep = 1;
      m_opaqueTess.begin(g_opaqueBuf[m_compileSy], MAX_VERTS_PER_SUB_CHUNK);
      m_transTess.begin(g_transBuf[m_compileSy], MAX_VERTS_PER_SUB_CHUNK);
      m_transFancyTess.begin(g_transFancyBuf[m_compileSy], MAX_VERTS_PER_SUB_CHUNK);
      m_waterTess.begin(g_waterBuf[m_compileSy], MAX_VERTS_PER_SUB_CHUNK);
      m_emitTess.begin(g_emitBuf[m_compileSy], MAX_VERTS_PER_SUB_CHUNK);
    }
  }

  if (m_compileStep == 1) {
    // Compile y-layers
    int yStart = m_compileSy * 16;
    int yEnd = yStart + 16;

    TileRenderer tileRenderer(m_level, &m_opaqueTess, &m_transTess, &m_transFancyTess, &m_waterTess, &m_emitTess);
    for (int lx = 0; lx < CHUNK_SIZE_X; lx++) {
      for (int lz = 0; lz < CHUNK_SIZE_Z; lz++) {
        for (int ly = yStart; ly < yEnd; ly++) {
          uint8_t id = m_compileChunk->blocks[lx][lz][ly];
          if (id == BLOCK_AIR)
            continue;

          const BlockProps &bp = g_blockProps[id];
          if (!bp.isSolid() && !bp.isTransparent() && !bp.isLiquid())
            continue;

          tileRenderer.tesselateBlockInWorld(id, lx, ly, lz, m_compileChunk->cx, m_compileChunk->cz);
        }
      }
    }
    
    // Upload immediately
    Chunk* c = m_compileChunk;
    int sy = m_compileSy;

    c->dirty[sy] = false;
    c->priority[sy] = 0; // Reset priority after compilation

    c->opaqueTriCount[sy] = m_opaqueTess.end();
    c->transTriCount[sy] = m_transTess.end();
    c->transFancyTriCount[sy] = m_transFancyTess.end();
    c->waterTriCount[sy] = m_waterTess.end();

    // Opaque Buffer
    if (c->opaqueTriCount[sy] > 0) {
      if (c->opaqueTriCount[sy] > c->opaqueCapacity[sy]) {
        if (c->opaqueVertices[sy]) free(c->opaqueVertices[sy]);
        c->opaqueCapacity[sy] = c->opaqueTriCount[sy] + (c->opaqueTriCount[sy] >> 1) + 128; 
        c->opaqueVertices[sy] = (CraftPSPVertex *)memalign(16, c->opaqueCapacity[sy] * sizeof(CraftPSPVertex));
      }
      if (c->opaqueVertices[sy]) {
        memcpy(c->opaqueVertices[sy], g_opaqueBuf[sy], c->opaqueTriCount[sy] * sizeof(CraftPSPVertex));
        sceKernelDcacheWritebackInvalidateRange(c->opaqueVertices[sy], c->opaqueTriCount[sy] * sizeof(CraftPSPVertex));
      } else {
        c->opaqueTriCount[sy] = 0;
        c->opaqueCapacity[sy] = 0;
      }
    }

    // Transparent Buffer (Outer Leaves, Glass, Water)
    if (c->transTriCount[sy] > 0) {
      if (c->transTriCount[sy] > c->transCapacity[sy]) {
        if (c->transVertices[sy]) free(c->transVertices[sy]);
        c->transCapacity[sy] = c->transTriCount[sy] + (c->transTriCount[sy] >> 1) + 128;
        c->transVertices[sy] = (CraftPSPVertex *)memalign(16, c->transCapacity[sy] * sizeof(CraftPSPVertex));
      }
      if (c->transVertices[sy]) {
        memcpy(c->transVertices[sy], g_transBuf[sy], c->transTriCount[sy] * sizeof(CraftPSPVertex));
        sceKernelDcacheWritebackInvalidateRange(c->transVertices[sy], c->transTriCount[sy] * sizeof(CraftPSPVertex));
      } else {
        c->transTriCount[sy] = 0;
        c->transCapacity[sy] = 0;
      }
    }

    // Transparent FANCY Buffer (Inner Leaves)
    if (c->transFancyTriCount[sy] > 0) {
      if (c->transFancyTriCount[sy] > c->transFancyCapacity[sy]) {
        if (c->transFancyVertices[sy]) free(c->transFancyVertices[sy]);
        c->transFancyCapacity[sy] = c->transFancyTriCount[sy] + (c->transFancyTriCount[sy] >> 1) + 128;
        c->transFancyVertices[sy] = (CraftPSPVertex *)memalign(16, c->transFancyCapacity[sy] * sizeof(CraftPSPVertex));
      }
      if (c->transFancyVertices[sy]) {
        memcpy(c->transFancyVertices[sy], g_transFancyBuf[sy], c->transFancyTriCount[sy] * sizeof(CraftPSPVertex));
        sceKernelDcacheWritebackInvalidateRange(c->transFancyVertices[sy], c->transFancyTriCount[sy] * sizeof(CraftPSPVertex));
      } else {
        c->transFancyTriCount[sy] = 0;
        c->transFancyCapacity[sy] = 0;
      }
    }

    // Water Buffer
    if (c->waterTriCount[sy] > 0) {
      if (c->waterTriCount[sy] > c->waterCapacity[sy]) {
        if (c->waterVertices[sy]) free(c->waterVertices[sy]);
        c->waterCapacity[sy] = c->waterTriCount[sy] + (c->waterTriCount[sy] >> 1) + 128;
        c->waterVertices[sy] = (CraftPSPVertex *)memalign(16, c->waterCapacity[sy] * sizeof(CraftPSPVertex));
      }
      if (c->waterVertices[sy]) {
        memcpy(c->waterVertices[sy], g_waterBuf[sy], c->waterTriCount[sy] * sizeof(CraftPSPVertex));
        sceKernelDcacheWritebackInvalidateRange(c->waterVertices[sy], c->waterTriCount[sy] * sizeof(CraftPSPVertex));
      } else {
        c->waterTriCount[sy] = 0;
        c->waterCapacity[sy] = 0;
      }
    }

    // Emit buffer
    c->emitTriCount[sy] = m_emitTess.end();
    if (c->emitTriCount[sy] > 0) {
      if (c->emitTriCount[sy] > c->emitCapacity[sy]) {
        if (c->emitVertices[sy]) free(c->emitVertices[sy]);
        c->emitCapacity[sy] = c->emitTriCount[sy] + (c->emitTriCount[sy] >> 1) + 128;
        c->emitVertices[sy] = (CraftPSPVertex *)memalign(16, c->emitCapacity[sy] * sizeof(CraftPSPVertex));
      }
      if (c->emitVertices[sy]) {
        memcpy(c->emitVertices[sy], g_emitBuf[sy], c->emitTriCount[sy] * sizeof(CraftPSPVertex));
        sceKernelDcacheWritebackInvalidateRange(c->emitVertices[sy], c->emitTriCount[sy] * sizeof(CraftPSPVertex));
      } else {
        c->emitTriCount[sy] = 0;
        c->emitCapacity[sy] = 0;
      }
    }

    m_compileStep = 0;
    m_compileChunk = nullptr;
  }
  
  // Only process one subchunk per frame to avoid buffer conflicts
  if (m_compileStep == 0) {
      break;
  }
  
  } // end while(true)
}

// Finish tessellation and upload vertex data
static void flushSubChunk(Chunk *c, int sy,
                          Tesselator &opT, Tesselator &trT, Tesselator &tfT, Tesselator &waT, Tesselator &emT) {
  c->dirty[sy] = false;
  c->priority[sy] = 0;

  c->opaqueTriCount[sy] = opT.end();
  c->transTriCount[sy]  = trT.end();
  c->transFancyTriCount[sy] = tfT.end();
  c->waterTriCount[sy]  = waT.end();
  c->emitTriCount[sy]   = emT.end();

  auto upload = [](CraftPSPVertex *&buf, int &count, int &cap, CraftPSPVertex *src) {
    if (count <= 0) return;
    if (count > cap) {
      if (buf) free(buf);
      cap = count + (count >> 1) + 128;
      buf = (CraftPSPVertex *)memalign(16, cap * sizeof(CraftPSPVertex));
    }
    if (buf) {
      memcpy(buf, src, count * sizeof(CraftPSPVertex));
      sceKernelDcacheWritebackInvalidateRange(buf, count * sizeof(CraftPSPVertex));
    } else {
      count = 0; cap = 0;
    }
  };
  upload(c->opaqueVertices[sy],    c->opaqueTriCount[sy],    c->opaqueCapacity[sy],    g_opaqueBuf[sy]);
  upload(c->transVertices[sy],     c->transTriCount[sy],     c->transCapacity[sy],     g_transBuf[sy]);
  upload(c->transFancyVertices[sy],c->transFancyTriCount[sy],c->transFancyCapacity[sy],g_transFancyBuf[sy]);
  upload(c->waterVertices[sy],     c->waterTriCount[sy],     c->waterCapacity[sy],     g_waterBuf[sy]);
  upload(c->emitVertices[sy],      c->emitTriCount[sy],      c->emitCapacity[sy],      g_emitBuf[sy]);
}

void ChunkRenderer::rebuildChunkNow(int cx, int cz, int sy) {
  if (!m_level) return;
  Chunk *c = m_level->getChunk(cx, cz);
  if (!c || sy < 0 || sy >= 4) return;

  // Tessellate the full subchunk in one call
  m_opaqueTess.begin(g_opaqueBuf[sy], MAX_VERTS_PER_SUB_CHUNK);
  m_transTess.begin(g_transBuf[sy], MAX_VERTS_PER_SUB_CHUNK);
  m_transFancyTess.begin(g_transFancyBuf[sy], MAX_VERTS_PER_SUB_CHUNK);
  m_waterTess.begin(g_waterBuf[sy], MAX_VERTS_PER_SUB_CHUNK);
  m_emitTess.begin(g_emitBuf[sy], MAX_VERTS_PER_SUB_CHUNK);

  TileRenderer tileRenderer(m_level, &m_opaqueTess, &m_transTess, &m_transFancyTess, &m_waterTess, &m_emitTess);
  int yStart = sy * 16;
  int yEnd   = yStart + 16;
  for (int lx = 0; lx < CHUNK_SIZE_X; lx++) {
    for (int lz = 0; lz < CHUNK_SIZE_Z; lz++) {
      for (int ly = yStart; ly < yEnd; ly++) {
        uint8_t id = c->blocks[lx][lz][ly];
        if (id == BLOCK_AIR) continue;
        const BlockProps &bp = g_blockProps[id];
        if (!bp.isSolid() && !bp.isTransparent() && !bp.isLiquid()) continue;
        tileRenderer.tesselateBlockInWorld(id, lx, ly, lz, c->cx, c->cz);
      }
    }
  }

  flushSubChunk(c, sy, m_opaqueTess, m_transTess, m_transFancyTess, m_waterTess, m_emitTess);
}

void ChunkRenderer::render(float camX, float camY, float camZ) {
  renderOpaque(camX, camY, camZ);
  renderTransparent();
}

void ChunkRenderer::renderOpaque(float camX, float camY, float camZ) {
  if (!m_level)
    return;
  m_lastCamY = camY;

  m_atlas->bind();

  ScePspFMatrix4 vp;
  PSPRenderer_GetViewProjMatrix(&vp);
  Frustum frustum;
  frustum.update(vp);

  static const float RENDER_DISTANCE = 64.0f;

  // Process compile queue
  processCompileQueue(camX, camY, camZ);

  m_visibleCount = 0;

  // Gather visible chunks/subchunks.
  for (int cx = 0; cx < WORLD_CHUNKS_X; cx++) {
    for (int cz = 0; cz < WORLD_CHUNKS_Z; cz++) {
      Chunk *c = m_level->getChunk(cx, cz);
      if (!c)
        continue;

      for (int sy = 0; sy < 4; sy++) {
        if ((c->opaqueTriCount[sy] == 0 && c->transTriCount[sy] == 0 && c->transFancyTriCount[sy] == 0 && c->waterTriCount[sy] == 0) ||
            (!c->opaqueVertices[sy] && !c->transVertices[sy] && !c->transFancyVertices[sy] && !c->waterVertices[sy]))
          continue;

        float chunkCenterX = c->cx * CHUNK_SIZE_X + CHUNK_SIZE_X / 2.0f;
        float chunkCenterZ = c->cz * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2.0f;
        float chunkCenterY = sy * 16 + 8.0f;

        float dx = chunkCenterX - camX;
        float dy = chunkCenterY - camY;
        float dz = chunkCenterZ - camZ;

        float maxDist = RENDER_DISTANCE + 11.5f;
        float distSqHoriz = dx * dx + dz * dz;
        if (distSqHoriz > maxDist * maxDist)
          continue;

        float distSq = dx * dx + dy * dy + dz * dz;

        AABB box;
        box.x0 = c->cx * CHUNK_SIZE_X - 4.0f;
        box.y0 = sy * 16 - 4.0f;
        box.z0 = c->cz * CHUNK_SIZE_Z - 4.0f;
        box.x1 = c->cx * CHUNK_SIZE_X + CHUNK_SIZE_X + 4.0f;
        box.y1 = sy * 16 + 16 + 4.0f;
        box.z1 = c->cz * CHUNK_SIZE_Z + CHUNK_SIZE_Z + 4.0f;

        if (frustum.testAABB(box) == Frustum::OUTSIDE)
          continue;

        m_visibleChunks[m_visibleCount].chunk = c;
        m_visibleChunks[m_visibleCount].subChunkIdx = sy;
        m_visibleChunks[m_visibleCount].distSq = distSq;
        m_visibleChunks[m_visibleCount].distSqHoriz = distSqHoriz;
        m_visibleCount++;
      }
    }
  }

  // Sort visible chunks front-to-back (used by opaque and emissive passes).
  for (int i = 0; i < m_visibleCount - 1; i++) {
    for (int j = 0; j < m_visibleCount - i - 1; j++) {
      if (m_visibleChunks[j].distSq > m_visibleChunks[j + 1].distSq) {
        VisibleChunk temp = m_visibleChunks[j];
        m_visibleChunks[j] = m_visibleChunks[j + 1];
        m_visibleChunks[j + 1] = temp;
      }
    }
  }

  float sunBr = m_level->getSunBrightness();
  uint8_t sunByte = (uint8_t)(sunBr * 0.85f * 255.0f + 0.15f * 255.0f); // [0.15, 1.0]
  uint32_t sunAmbient = (0xFF000000u) | ((uint32_t)sunByte << 16) | ((uint32_t)sunByte << 8) | sunByte;

  auto setChunkMatrix = [](Chunk *c) {
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumUpdateMatrix();
  };

  sceGuDisable(GU_ALPHA_TEST);
  sceGuDisable(GU_BLEND);
  sceGuEnable(GU_LIGHTING);
  sceGuAmbient(sunAmbient);

  for (int i = 0; i < m_visibleCount; i++) {
    Chunk *c = m_visibleChunks[i].chunk;
    int sy = m_visibleChunks[i].subChunkIdx;
    if (c->opaqueTriCount[sy] == 0 || !c->opaqueVertices[sy]) continue;
    setChunkMatrix(c);
    sceGumDrawArray(GU_TRIANGLES,
                    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    c->opaqueTriCount[sy], nullptr, c->opaqueVertices[sy]);
  }

  sceGuDisable(GU_LIGHTING);
}

void ChunkRenderer::renderTransparent() {
  if (!m_level)
    return;

  static const float FANCY_LOD_DIST = 32.0f;
  float camY = m_lastCamY;
  float sunBr = m_level->getSunBrightness();
  uint8_t sunByte = (uint8_t)(sunBr * 0.85f * 255.0f + 0.15f * 255.0f); // [0.15, 1.0]
  uint32_t sunAmbient = (0xFF000000u) | ((uint32_t)sunByte << 16) | ((uint32_t)sunByte << 8) | sunByte;

  auto setChunkMatrix = [](Chunk *c) {
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumUpdateMatrix();
  };

  // Draw inner leaves (Back-to-Front check)
  sceGuEnable(GU_LIGHTING);
  sceGuAmbient(sunAmbient);
  sceGuEnable(GU_ALPHA_TEST);
  sceGuEnable(GU_BLEND);
  sceGuDisable(GU_CULL_FACE); // Allow plants/water to be seen from both sides

  // MCPE-like: leaves/cutout-fancy pass should be alpha-tested, not alpha-blended.
  sceGuDisable(GU_BLEND);
  for (int i = m_visibleCount - 1; i >= 0; i--) {
    Chunk *c = m_visibleChunks[i].chunk;
    int sy = m_visibleChunks[i].subChunkIdx;
    if (c->transFancyTriCount[sy] == 0 || !c->transFancyVertices[sy]) continue;
    float distSqHoriz = m_visibleChunks[i].distSqHoriz;
    if (distSqHoriz > FANCY_LOD_DIST * FANCY_LOD_DIST || camY > 80.0f) continue;
    setChunkMatrix(c);
    sceGumDrawArray(GU_TRIANGLES,
                    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    c->transFancyTriCount[sy], nullptr, c->transFancyVertices[sy]);
  }

  // Draw transparent chunks (Back-to-Front)
  sceGuEnable(GU_BLEND);
  for (int i = m_visibleCount - 1; i >= 0; i--) {
    Chunk *c = m_visibleChunks[i].chunk;
    int sy = m_visibleChunks[i].subChunkIdx;
    if (c->transTriCount[sy] == 0 || !c->transVertices[sy]) continue;
    setChunkMatrix(c);
    sceGumDrawArray(GU_TRIANGLES,
                    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    c->transTriCount[sy], nullptr, c->transVertices[sy]);
  }

  // Draw water chunks separately with culling enabled to avoid visible inner
  // backfaces/sheets inside waterfalls and around foliage intersections.
  sceGuEnable(GU_CULL_FACE);
  for (int i = m_visibleCount - 1; i >= 0; i--) {
    Chunk *c = m_visibleChunks[i].chunk;
    int sy = m_visibleChunks[i].subChunkIdx;
    if (c->waterTriCount[sy] == 0 || !c->waterVertices[sy]) continue;
    setChunkMatrix(c);
    sceGumDrawArray(GU_TRIANGLES,
                    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    c->waterTriCount[sy], nullptr, c->waterVertices[sy]);
  }

  // Draw emissive chunks last so transparent/cutout geometry (leaves/ice/glass/plants)
  // also receives block-light overlay and does not stay dark at night.
  sceGuDisable(GU_LIGHTING);
  sceGuEnable(GU_BLEND);
  sceGuDepthFunc(GU_GEQUAL);
  sceGuDepthOffset(8);
  uint8_t emitByte = (uint8_t)(160.0f + (1.0f - sunBr) * 95.0f);
  uint32_t emitFix = ((uint32_t)emitByte << 24) | ((uint32_t)emitByte << 16) |
                     ((uint32_t)emitByte << 8) | (uint32_t)emitByte;
  sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, emitFix, 0xFFFFFFFF);
  sceGuDepthMask(GU_TRUE); // no depth writes
  sceGuAmbient(0xFFFFFFFF);
  for (int i = 0; i < m_visibleCount; i++) {
    Chunk *c = m_visibleChunks[i].chunk;
    int sy = m_visibleChunks[i].subChunkIdx;
    if (c->emitTriCount[sy] == 0 || !c->emitVertices[sy]) continue;
    setChunkMatrix(c);
    sceGumDrawArray(GU_TRIANGLES,
                    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    c->emitTriCount[sy], nullptr, c->emitVertices[sy]);
  }
  sceGuDepthMask(GU_FALSE);
  sceGuDepthOffset(0);
  sceGuDepthFunc(GU_GEQUAL);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDisable(GU_BLEND);

  sceGuEnable(GU_CULL_FACE); // Restore CULL_FACE
  sceGuDisable(GU_LIGHTING); // Restore default state
}
