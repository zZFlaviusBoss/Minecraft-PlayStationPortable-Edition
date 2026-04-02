#include "SkyRenderer.h"
#include "../stb_image.h"
#include "../world/Random.h"
#include <cstring>
#include <malloc.h>
#include <math.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>

#define PI 3.14159265358979323846f

static unsigned int nextPow2(unsigned int v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

SimpleTexture::~SimpleTexture() {
  if (data)
    free(data);
}

void SimpleTexture::load(const char *path) {
  SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
  if (fd < 0)
    return;
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
    width = imgW;
    height = imgH;
    p2width = nextPow2(width);
    p2height = nextPow2(height);

    data = memalign(16, p2width * p2height * 4);
    memset(data, 0, p2width * p2height * 4); // Clear to 0 for padding

    for (unsigned int y = 0; y < (unsigned int)height; y++) {
      memcpy((uint32_t *)data + (y * p2width),
             (uint32_t *)pixels + (y * width),
             width * 4);
    }

    stbi_image_free(pixels);
    sceKernelDcacheWritebackAll();
  }
}

void SimpleTexture::bind() {
  if (!data)
    return;
  sceGuTexMode(GU_PSM_8888, 0, 0, 0);
  sceGuTexImage(0, p2width, p2height, p2width, data);
  sceGuTexScale(1.0f, 1.0f);
  sceGuTexOffset(0.0f, 0.0f);
  sceGuTexFilter(GU_NEAREST, GU_NEAREST);
}

SkyRenderer::SkyRenderer(Level *level) : m_level(level) {
  m_celestialVertices = (SkyVertex *)memalign(16, 32 * sizeof(SkyVertex));

  int s = 16;
  int d = 32;
  m_numSkyVertices = (d * 2) * (d * 2) * 6;
  m_skyVertices =
      (SkyPosVertex *)memalign(16, m_numSkyVertices * sizeof(SkyPosVertex));

  // Generate a mesh for the dark bottom plane (void)
  m_numBottomVertices = m_numSkyVertices;
  m_bottomVertices =
      (SkyPosVertex *)memalign(16, m_numBottomVertices * sizeof(SkyPosVertex));

  m_numStarVertices = 1500 * 6;      // 9000
  m_starVertices =
      (SkyVertex *)memalign(16, m_numStarVertices * sizeof(SkyVertex));

  int skyIdx = 0;
  float yy = 16.0f;
  for (int xx = -s * d; xx < s * d; xx += s) {
    for (int zz = -s * d; zz < s * d; zz += s) {
      m_skyVertices[skyIdx++] = {(float)(xx + 0), yy, (float)(zz + 0)};
      m_skyVertices[skyIdx++] = {(float)(xx + s), yy, (float)(zz + 0)};
      m_skyVertices[skyIdx++] = {(float)(xx + 0), yy, (float)(zz + s)};
      m_skyVertices[skyIdx++] = {(float)(xx + s), yy, (float)(zz + 0)};
      m_skyVertices[skyIdx++] = {(float)(xx + s), yy, (float)(zz + s)};
      m_skyVertices[skyIdx++] = {(float)(xx + 0), yy, (float)(zz + s)};
    }
  }
  sceKernelDcacheWritebackInvalidateRange(
      m_skyVertices, m_numSkyVertices * sizeof(SkyPosVertex));

  int botIdx = 0;
  yy = -16.0f;
  for (int xx = -s * d; xx < s * d; xx += s) {
    for (int zz = -s * d; zz < s * d; zz += s) {
      m_bottomVertices[botIdx++] = {(float)(xx + s), yy, (float)(zz + 0)};
      m_bottomVertices[botIdx++] = {(float)(xx + 0), yy, (float)(zz + 0)};
      m_bottomVertices[botIdx++] = {(float)(xx + s), yy, (float)(zz + s)};
      m_bottomVertices[botIdx++] = {(float)(xx + 0), yy, (float)(zz + 0)};
      m_bottomVertices[botIdx++] = {(float)(xx + 0), yy, (float)(zz + s)};
      m_bottomVertices[botIdx++] = {(float)(xx + s), yy, (float)(zz + s)};
    }
  }
  sceKernelDcacheWritebackInvalidateRange(
      m_bottomVertices, m_numBottomVertices * sizeof(SkyPosVertex));

  // Build stars
  int starIdx = 0;
  Random random(
      10842); // Initialize generator with exact Minecraft seed

  for (int i = 0; i < 1500; i++) {
    float x = random.nextFloat() * 2.0f - 1.0f;
    float y = random.nextFloat() * 2.0f - 1.0f;
    float z = random.nextFloat() * 2.0f - 1.0f;

    float ss = 0.28f + random.nextFloat() * 0.1f;

    float d_sq = x * x + y * y + z * z;
    if (d_sq < 1.0f && d_sq > 0.01f) {
      float id = 1.0f / sqrtf(d_sq);
      x *= id;
      y *= id;
      z *= id;

      float xp = x * 100.0f;
      float yp = y * 100.0f;
      float zp = z * 100.0f;

      float yRot = atan2f(x, z);
      float ySin = sinf(yRot);
      float yCos = cosf(yRot);

      float xRot = atan2f(sqrtf(x * x + z * z), y);
      float xSin = sinf(xRot);
      float xCos = cosf(xRot);

      // Z-axis rotation must use nextDouble() to match Vanilla/4J RNG consumption
      float zRot = (float)(random.nextDouble() * PI * 2.0);
      float zSin = sinf(zRot);
      float zCos = cosf(zRot);

      float pX[4], pY[4], pZ[4];
      for (int c = 0; c < 4; c++) {
        float ___xo = 0.0f;
        float ___yo = ((c & 2) - 1) * ss;
        float ___zo = (((c + 1) & 2) - 1) * ss;

        float __xo = ___xo;
        float __yo = ___yo * zCos - ___zo * zSin;
        float __zo = ___zo * zCos + ___yo * zSin;

        float _zo = __zo;
        float _yo = __yo * xSin + __xo * xCos;
        float _xo = __xo * xSin - __yo * xCos;

        pX[c] = xp + (_xo * ySin - _zo * yCos);
        pY[c] = yp + _yo;
        pZ[c] = zp + (_zo * ySin + _xo * yCos);
      }

      m_starVertices[starIdx++] = {0, 0, 0xFFFFFFFF, pX[0], pY[0], pZ[0]};
      m_starVertices[starIdx++] = {0, 0, 0xFFFFFFFF, pX[1], pY[1], pZ[1]};
      m_starVertices[starIdx++] = {0, 0, 0xFFFFFFFF, pX[2], pY[2], pZ[2]};
      m_starVertices[starIdx++] = {0, 0, 0xFFFFFFFF, pX[0], pY[0], pZ[0]};
      m_starVertices[starIdx++] = {0, 0, 0xFFFFFFFF, pX[2], pY[2], pZ[2]};
      m_starVertices[starIdx++] = {0, 0, 0xFFFFFFFF, pX[3], pY[3], pZ[3]};
    }
  }
  m_numStarVertices = starIdx;
  sceKernelDcacheWritebackInvalidateRange(
      m_starVertices, m_numStarVertices * sizeof(SkyVertex));

  m_sunTex.load("res/sun.png");
  m_moonTex.load("res/moon.png");
  m_moonPhasesTex.load("res/moon_phases.png");
}

SkyRenderer::~SkyRenderer() {
  free(m_skyVertices);
  free(m_bottomVertices);
  free(m_starVertices);
  free(m_celestialVertices);
}

uint32_t SkyRenderer::getSkyColor(float timeOfDay) {
  float f = cosf(timeOfDay * PI * 2.0f);

  // Brightness: peaks at noon, lowest at midnight
  float br = f * 2.0f + 0.5f;
  if (br < 0.0f) br = 0.0f;
  if (br > 1.0f) br = 1.0f;

  // Vanilla/4J sky blue: 120, 167, 255
  float r = 0.47f * br;
  float g = 0.655f * br;
  float b = 1.0f  * br;

  uint8_t R = (uint8_t)(r * 255.0f);
  uint8_t G = (uint8_t)(g * 255.0f);
  uint8_t B = (uint8_t)(b * 255.0f);
  return 0xFF000000 | ((uint32_t)B << 16) | ((uint32_t)G << 8) | R;
}

uint32_t SkyRenderer::getFogColor(float timeOfDay, const ScePspFVector3& lookDir) {
  float f = cosf(timeOfDay * PI * 2.0f);
  float br = f * 2.0f + 0.5f;
  if (br < 0.0f) br = 0.0f;
  if (br > 1.0f) br = 1.0f;

  // Vanilla/4J fog base color: 192, 216, 255
  float baseR = 0.75f;
  float baseG = 0.85f;
  float baseB = 1.0f;

  // Vanilla math: night horizon is never completely black! 
  // It locks at a minimum dark blue: (R*0.06, G*0.06, B*0.09)
  float fr = baseR * (br * 0.94f + 0.06f);
  float fg = baseG * (br * 0.94f + 0.06f);
  float fb = baseB * (br * 0.91f + 0.09f);

  // Console 2014 sunrise/sunset fog blend
  float sunV_x = sinf(timeOfDay * PI * 2.0f) > 0.0f ? -1.0f : 1.0f;
  float d = lookDir.x * sunV_x; // Dot product with pure West/East Vector
  if (d < 0.0f) d = 0.0f;
  if (d > 0.0f) {
    float srColor[4];
    if (getSunriseColor(timeOfDay, srColor)) {
      d *= srColor[3]; // Weight by dawn/dusk strength
      fr = fr * (1.0f - d) + srColor[0] * d;
      fg = fg * (1.0f - d) + srColor[1] * d;
      fb = fb * (1.0f - d) + srColor[2] * d;
    }
  }

  uint32_t horizonCol = 0xFF000000 | ((uint32_t)(fb * 255.0f) << 16) |
                        ((uint32_t)(fg * 255.0f) << 8) | (uint32_t)(fr * 255.0f);
  return horizonCol;
}

bool SkyRenderer::getSunriseColor(float timeOfDay, float *outColor) {
  float f = cosf(timeOfDay * PI * 2.0f);

  if (f >= -0.4f && f <= 0.4f) {
    float f1 = (f - 0.0f) / 0.4f * 0.5f + 0.5f;
    float f2 = 1.0f - (1.0f - sinf(f1 * PI)) * 0.99f;
    f2 = f2 * f2;

    // 4J constants: Dawn Dark (0xB23333) and Dawn Bright (0xFFE533)
    float r1 = 0.698f, g1 = 0.2f, b1 = 0.2f;   // Dark
    float r2 = 1.0f, g2 = 0.898f, b2 = 0.2f;   // Bright

    outColor[0] = f1 * (r2 - r1) + r1; // R
    outColor[1] = f1 * (g2 - g1) + g1; // G
    outColor[2] = f1 * (b2 - b1) + b1; // B
    outColor[3] = f2;                  // Alpha
    return true;
  }
  return false;
}

float SkyRenderer::getStarBrightness(float timeOfDay) {
  float br = 1.0f - (cosf(timeOfDay * PI * 2.0f) * 2.0f + 0.25f);
  if (br < 0.0f)
    br = 0.0f;
  if (br > 1.0f)
    br = 1.0f;
  return br * br * 0.5f;
}

// Get current moon phase (0-7)
int SkyRenderer::getMoonPhase(float /*timeOfDay*/) {
  return m_level->getDay() % 8;
}

// Sky brightness (0=night, 1=day)
float SkyRenderer::getBrightness(float timeOfDay) {
  float br = cosf(timeOfDay * PI * 2.0f) * 2.0f + 0.5f;
  if (br < 0.0f)
    br = 0.0f;
  if (br > 1.0f)
    br = 1.0f;
  return br;
}

void SkyRenderer::renderSky(float playerX, float playerY, float playerZ, const ScePspFVector3& lookDir) {
  float timeOfDay = m_level->getTimeOfDay();
  uint32_t skyCol = getSkyColor(timeOfDay);

  uint32_t horizonCol = getFogColor(timeOfDay, lookDir);

  sceGuClearColor(horizonCol);
  // Sky geometry is at y=+/-16. Need wide fog to cover even grazing angles.
  // At 5 deg elevation: 16/sin(5deg)=184u, so fog end >184 ensures full coverage.
  sceGuFog(0.0f, 200.0f, horizonCol);

  // Use wider near clip for sky to avoid hardware cutouts
  sceGumMatrixMode(GU_PROJECTION);
  sceGumPushMatrix();
  sceGumLoadIdentity();
  sceGumPerspective(70.0f, 480.0f / 272.0f, 4.0f, 2000.0f);

  sceGumMatrixMode(GU_MODEL);
  sceGumPushMatrix();
  sceGumLoadIdentity();

  // Position skybox center at player eye level
  ScePspFVector3 playerPos = {playerX, playerY, playerZ};
  sceGumTranslate(&playerPos);

  sceGuDisable(GU_TEXTURE_2D);
  sceGuDisable(GU_DEPTH_TEST); // Sky always draws behind everything
  sceGuDepthMask(GU_TRUE);     // No depth writes for sky

  sceGuDisable(GU_CULL_FACE);
  sceGuColor(skyCol);
  sceGuEnable(GU_FOG);
  sceGumDrawArray(GU_TRIANGLES, GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                  m_numSkyVertices, 0, m_skyVertices);

  // Bottom void plane — 4jcraft formula: sr*0.2+0.04, sg*0.2+0.04, sb*0.6+0.1
  {
    float sr = ((skyCol >>  0) & 0xFF) / 255.0f;
    float sg = ((skyCol >>  8) & 0xFF) / 255.0f;
    float sb = ((skyCol >> 16) & 0xFF) / 255.0f;
    uint8_t dr = (uint8_t)((sr * 0.2f + 0.04f) * 255.0f);
    uint8_t dg = (uint8_t)((sg * 0.2f + 0.04f) * 255.0f);
    uint8_t db = (uint8_t)((sb * 0.6f + 0.10f) * 255.0f);
    uint32_t darkCol = 0xFF000000 | ((uint32_t)db << 16) | ((uint32_t)dg << 8) | dr;
    sceGuColor(darkCol);
    sceGumDrawArray(GU_TRIANGLES, GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    m_numBottomVertices, 0, m_bottomVertices);
  }
  sceGuEnable(GU_CULL_FACE);

  sceGuDisable(GU_FOG);
  sceGuEnable(GU_BLEND);

  // Sun and moon (additive blending)
  sceGuEnable(GU_TEXTURE_2D);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
  sceGumPushMatrix();

  sceGumRotateY(-PI / 2.0f);
  sceGumRotateX(timeOfDay * PI * 2.0f);

  sceGuDisable(GU_CULL_FACE);

  float brightness = getBrightness(timeOfDay);
  float celBr = brightness < 0.3f ? 0.3f : brightness;
  uint8_t cb = (uint8_t)(celBr * 255.0f);
  uint32_t col32 = 0xFF000000 | (cb << 16) | (cb << 8) | cb;

  // Sun
  float s = 30.0f;
  m_sunTex.bind();
  m_celestialVertices[0] = {0.0f, 0.0f, col32, -s, 100.0f, -s};
  m_celestialVertices[1] = {1.0f, 0.0f, col32, s, 100.0f, -s};
  m_celestialVertices[2] = {0.0f, 1.0f, col32, -s, 100.0f, s};
  m_celestialVertices[3] = {1.0f, 0.0f, col32, s, 100.0f, -s};
  m_celestialVertices[4] = {1.0f, 1.0f, col32, s, 100.0f, s};
  m_celestialVertices[5] = {0.0f, 1.0f, col32, -s, 100.0f, s};
  sceKernelDcacheWritebackInvalidateRange(m_celestialVertices, 6 * sizeof(SkyVertex));
  sceGumDrawArray(GU_TRIANGLES,
                  GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                  6, 0, m_celestialVertices);

  // Moon
  s = 20.0f;
  float u0 = 0.0f, u1 = 1.0f, v0 = 0.0f, v1 = 1.0f;
  if (m_moonPhasesTex.data != nullptr) {
    int moonPhase = getMoonPhase(timeOfDay);
    int col = moonPhase % 4;
    int row = moonPhase / 4;
    u0 = col * 0.25f;
    u1 = (col + 1) * 0.25f;
    v0 = row * 0.5f;
    v1 = (row + 1) * 0.5f;
    m_moonPhasesTex.bind();
  } else {
    m_moonTex.bind();
  }
  m_celestialVertices[6]  = {u1, v1, col32, -s, -100.0f, s};
  m_celestialVertices[7]  = {u0, v1, col32,  s, -100.0f, s};
  m_celestialVertices[8]  = {u1, v0, col32, -s, -100.0f, -s};
  m_celestialVertices[9]  = {u0, v1, col32,  s, -100.0f, s};
  m_celestialVertices[10] = {u0, v0, col32,  s, -100.0f, -s};
  m_celestialVertices[11] = {u1, v0, col32, -s, -100.0f, -s};
  sceKernelDcacheWritebackInvalidateRange(&m_celestialVertices[6], 6 * sizeof(SkyVertex));
  sceGumDrawArray(GU_TRIANGLES,
                  GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                  6, 0, &m_celestialVertices[6]);

  // Stars drawn INSIDE the celestial rotation matrix so they rotate with the day/night cycle
  float starAlpha = getStarBrightness(timeOfDay);
  if (starAlpha > 0.0f) {
    if (starAlpha > 1.0f)
      starAlpha = 1.0f;

    uint8_t a = (uint8_t)(starAlpha * 255.0f);
    uint32_t fadeColor = (a << 24) | (a << 16) | (a << 8) | a;
    sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, fadeColor, 0xFFFFFFFF);
    sceGuDisable(GU_TEXTURE_2D);

    sceGumDrawArray(GU_TRIANGLES,
                    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    m_numStarVertices, 0, m_starVertices);

    sceGuEnable(GU_TEXTURE_2D);
  }

  sceGuEnable(GU_CULL_FACE);
  sceGumPopMatrix(); // Pops celestial rotation

  sceGuEnable(GU_FOG);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDepthMask(GU_FALSE);
  sceGuEnable(GU_DEPTH_TEST);
  sceGumPopMatrix(); // model matrix

  // Restore projection
  sceGumMatrixMode(GU_PROJECTION);
  sceGumPopMatrix();
  sceGumMatrixMode(GU_MODEL);

  // Restore world render state
  sceGuEnable(GU_DEPTH_TEST);
  sceGuDepthFunc(GU_GEQUAL);
  sceGuDepthMask(GU_FALSE);
  sceGuEnable(GU_CULL_FACE);
  sceGuEnable(GU_TEXTURE_2D);
  sceGuEnable(GU_FOG);

  // CRITICAL: Restore the shorter world fog distances! 
  // Otherwise chunks will cut off sharply at 64 blocks instead of fading into fog.
  sceGuFog(32.0f, 64.0f, horizonCol);
}

