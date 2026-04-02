#include "CloudRenderer.h"
#include "../stb_image.h"
#include <cstring>
#include <malloc.h>
#include <math.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stdint.h>

CloudRenderer::CloudRenderer(Level *level)
    : m_level(level), m_cloudOffset(0.0f) {
  m_cloudData = nullptr;
  m_cloudTexWidth = 0;
  m_cloudTexHeight = 0;
  m_lastCloudPx = -999999.0f;
  m_lastCloudPz = -999999.0f;
  m_lastCloudSnappedOffset = -999999.0f;
  m_numCloudVertices = 0;

  m_cloudVertices = (SkyVertex *)memalign(16, 60000 * sizeof(SkyVertex));

  m_cloudsTex.load("res/clouds.png");

  SceUID fd = sceIoOpen("res/clouds.png", PSP_O_RDONLY, 0777);
  if (fd >= 0) {
    int size = (int)sceIoLseek(fd, 0, PSP_SEEK_END);
    sceIoLseek(fd, 0, PSP_SEEK_SET);
    unsigned char *buf = (unsigned char *)malloc(size);
    sceIoRead(fd, buf, size);
    sceIoClose(fd);
    int imgW, imgH, channels;
    unsigned char *pixels =
        stbi_load_from_memory(buf, size, &imgW, &imgH, &channels, 4);
    free(buf);
    if (pixels) {
      m_cloudTexWidth = imgW;
      m_cloudTexHeight = imgH;
      m_cloudData = (uint8_t *)malloc(imgW * imgH);
      for (int i = 0; i < imgW * imgH; i++) {
        m_cloudData[i] = pixels[i * 4 + 3];
      }
      stbi_image_free(pixels);
    }
  }
}

CloudRenderer::~CloudRenderer() {
  if (m_cloudVertices)
    free(m_cloudVertices);
  if (m_cloudData)
    free(m_cloudData);
}

void CloudRenderer::renderClouds(float playerX, float playerY, float playerZ,
                                 float alpha, uint32_t fogColor) {
  float cloudSpeed = 0.0375f;
  float cloudHeight = 108.0f;   // 4J genDepth = 108
  float cloudThickness = 4.0f;  // 4J h = 4
  float qS = 12.0f;
  int drawDistance = 48;

  float tod = m_level->getTimeOfDay();
  float br = cosf(tod * 3.14159265f * 2.0f) * 2.0f + 0.5f;
  if (br < 0.0f)
    br = 0.0f;
  if (br > 1.0f)
    br = 1.0f;

  // Base cloud colors derived from 4J table indices
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;

  r *= br * 0.90f + 0.10f;
  g *= br * 0.90f + 0.10f;
  b *= br * 0.85f + 0.15f;

  auto makeColor = [&](uint8_t a, uint8_t baseR, uint8_t baseG,
                       uint8_t baseB) -> uint32_t {
    float finalR = (float)baseR / 255.0f * r;
    float finalG = (float)baseG / 255.0f * g;
    float finalB = (float)baseB / 255.0f * b;
    return (a << 24) | ((uint8_t)(finalB * 255.0f) << 16) |
           ((uint8_t)(finalG * 255.0f) << 8) | (uint8_t)(finalR * 255.0f);
  };

  bool inClouds = (playerY >= cloudHeight - cloudThickness - 2.0f && playerY <= cloudHeight + 2.0f);

  uint8_t aTop  = inClouds ? 0xC0 : 0xD0;
  uint8_t aSide = inClouds ? 0xB0 : 0xC0;
  uint8_t aBot  = inClouds ? 0x90 : 0xAA;

  uint32_t colorTop    = makeColor(aTop, 0xFF, 0xFF, 0xFF); 
  uint32_t colorSide   = makeColor(aSide, 0xE5, 0xE5, 0xE5); 
  uint32_t colorBottom = makeColor(aBot, 0xCC, 0xCC, 0xCC); 
  // =========================================================================

  // Game time directly drives the scrolling offset
  float timeElapsed = (float)m_level->getTime() + alpha;

  // Calculate position based on movement speed
  m_cloudOffset = fmodf(timeElapsed * cloudSpeed, m_cloudTexWidth * qS);

  // Align grid to player position
  float px = floorf(playerX / qS) * qS;
  float pz = floorf(playerZ / qS) * qS;

  // Determine snapped vs fluid movement
  float snappedOffset = floorf(m_cloudOffset / qS) * qS;
  float slideOffset = m_cloudOffset - snappedOffset;

  // Rebuild if player moved or state changed
  bool meshRebuild = false;
  if (px != m_lastCloudPx || pz != m_lastCloudPz ||
      snappedOffset != m_lastCloudSnappedOffset ||
      inClouds != m_lastWasInClouds) {
    meshRebuild = true;
    m_lastCloudPx = px;
    m_lastCloudPz = pz;
    m_lastCloudSnappedOffset = snappedOffset;
    m_lastWasInClouds = inClouds;
  }

  // Geometry rebuild (Crucial PSP optimization)
  if (meshRebuild && m_cloudData && m_cloudTexWidth > 0 &&
      m_cloudTexHeight > 0) {
    int vIdx = 0;

    auto isSolid = [&](float worldX, float worldZ) {
      int px_x = (int)floorf((worldX + snappedOffset) / qS);
      int px_y = (int)floorf(worldZ / qS);

      px_x = (px_x % m_cloudTexWidth + m_cloudTexWidth) % m_cloudTexWidth;
      px_y = (px_y % m_cloudTexHeight + m_cloudTexHeight) % m_cloudTexHeight;
      return m_cloudData[px_y * m_cloudTexWidth + px_x] > 128;
    };

    for (int x = -drawDistance / 2; x < drawDistance / 2; x++) {
      for (int z = -drawDistance / 2; z < drawDistance / 2; z++) {
        float x0 = px + (float)(x * qS);
        float z0 = pz + (float)(z * qS);

        if (!isSolid(x0, z0))
          continue;

        float x1 = x0 + qS;
        float z1 = z0 + qS;
        float y0 = cloudHeight - cloudThickness;
        float y1 = cloudHeight;

        float texX = fmodf(x0 + snappedOffset, m_cloudTexWidth * qS);
        if (texX < 0)
          texX += m_cloudTexWidth * qS;
        
        float texZ = fmodf(z0, m_cloudTexHeight * qS);
        if (texZ < 0)
          texZ += m_cloudTexHeight * qS;

        float u0 = texX / (m_cloudTexWidth * qS);
        float u1 = (texX + qS) / (m_cloudTexWidth * qS);
        float v0 = texZ / (m_cloudTexHeight * qS);
        float v1 = (texZ + qS) / (m_cloudTexHeight * qS);

        // Neighbor culling
        bool solidLeft  = isSolid(x0 - qS, z0);
        bool solidRight = isSolid(x0 + qS, z0);
        bool solidFront = isSolid(x0, z0 - qS);
        bool solidBack  = isSolid(x0, z0 + qS);

        if (vIdx > 59000)
          break;

        auto emitQuad = [&](float tu0, float tv0, float tu1, float tv1,
                            uint32_t col, float vx0, float vy0, float vz0,
                            float vx1, float vy1, float vz1, float vx2,
                            float vy2, float vz2, float vx3, float vy3,
                            float vz3) {
          m_cloudVertices[vIdx++] = {tu0, tv0, col, vx0, vy0, vz0};
          m_cloudVertices[vIdx++] = {tu0, tv1, col, vx2, vy2, vz2};
          m_cloudVertices[vIdx++] = {tu1, tv0, col, vx1, vy1, vz1};

          m_cloudVertices[vIdx++] = {tu1, tv0, col, vx1, vy1, vz1};
          m_cloudVertices[vIdx++] = {tu0, tv1, col, vx2, vy2, vz2};
          m_cloudVertices[vIdx++] = {tu1, tv1, col, vx3, vy3, vz3};
        };

        // Top/Bottom faces
        emitQuad(u0, v0, u1, v1, colorBottom, x0, y0, z1, x1, y0, z1, x0, y0,
                 z0, x1, y0, z0);
        emitQuad(u0, v0, u1, v1, colorTop, x0, y1, z0, x1, y1, z0, x0, y1, z1,
                 x1, y1, z1);

        float uMid = u0 + (u1 - u0) * 0.5f;
        float vMid = v0 + (v1 - v0) * 0.5f;

        // Side faces
        if (!solidLeft)
          emitQuad(uMid, vMid, uMid, vMid, colorSide, x0, y1, z0, x0, y1, z1,
                   x0, y0, z0, x0, y0, z1);
        if (!solidRight)
          emitQuad(uMid, vMid, uMid, vMid, colorSide, x1, y1, z1, x1, y1, z0,
                   x1, y0, z1, x1, y0, z0);
        if (!solidFront)
          emitQuad(uMid, vMid, uMid, vMid, colorSide, x1, y1, z0, x0, y1, z0,
                   x1, y0, z0, x0, y0, z0);
        if (!solidBack)
          emitQuad(uMid, vMid, uMid, vMid, colorSide, x0, y1, z1, x1, y1, z1,
                   x0, y0, z1, x1, y0, z1);
      }
    }
    m_numCloudVertices = vIdx;

    // Flush cache before GE draws
    if (m_numCloudVertices > 0) {
      sceKernelDcacheWritebackInvalidateRange(
          m_cloudVertices, m_numCloudVertices * sizeof(SkyVertex));
    }
  }

  if (m_numCloudVertices == 0)
    return;

  m_cloudsTex.bind();

  // Setup hardware fog for clouds
  sceGuEnable(GU_FOG);
  float distToCloud = fabsf(cloudHeight - playerY);
  float cloudFogNear = distToCloud * 0.8f;
  if (cloudFogNear < 32.0f) cloudFogNear = 32.0f;
  float cloudFogFar = distToCloud + 64.0f;
  sceGuFog(cloudFogNear, cloudFogFar, fogColor);
  sceGuEnable(GU_CULL_FACE);
  sceGuEnable(GU_BLEND);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

  // Set texture properties
  sceGuEnable(GU_TEXTURE_2D);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  sceGuTexWrap(GU_REPEAT, GU_REPEAT);
  sceGuTexFilter(GU_NEAREST, GU_NEAREST); // Keep pixelated look

  sceGumMatrixMode(GU_MODEL);
  sceGumPushMatrix();
  sceGumLoadIdentity();

  // Translate by wind scroll
  ScePspFVector3 cloudTrans = {-slideOffset, 0.0f, 0.0f};
  sceGumTranslate(&cloudTrans);

  // Pass 1: depth prepass
  sceGuDepthMask(GU_FALSE);
  sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0x00000000, 0xFFFFFFFF);
  sceGuFrontFace(GU_CCW);
  sceGumDrawArray(GU_TRIANGLES,
                  GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                  m_numCloudVertices, 0, m_cloudVertices);
  if (inClouds) {
    sceGuFrontFace(GU_CW);
    sceGumDrawArray(GU_TRIANGLES,
                    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    m_numCloudVertices, 0, m_cloudVertices);
  }

  // Pass 2: color pass
  sceGuDepthMask(GU_TRUE);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuFrontFace(GU_CCW);;
  sceGumDrawArray(GU_TRIANGLES,
                  GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                  m_numCloudVertices, 0, m_cloudVertices);
  if (inClouds) {
    sceGuFrontFace(GU_CW);
    sceGumDrawArray(GU_TRIANGLES,
                    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    m_numCloudVertices, 0, m_cloudVertices);
  }
  // Restore state
  sceGuFrontFace(GU_CCW);
  sceGumPopMatrix();
  sceGuDisable(GU_BLEND);
  sceGuEnable(GU_FOG);
  sceGuDepthMask(GU_FALSE);
}
