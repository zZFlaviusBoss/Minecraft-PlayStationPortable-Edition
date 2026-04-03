#pragma once

#include "world/Level.h"
#include <pspgu.h>
#include <pspgum.h>

struct SkyVertex {
  float u, v;
  uint32_t color;
  float x, y, z;
};

struct SkyPosVertex {
  float x, y, z;
};

struct SimpleTexture {
  void *data = nullptr;
  unsigned int width = 0;
  unsigned int height = 0;
  unsigned int p2width = 0;
  unsigned int p2height = 0;

  void load(const char *path);
  void bind();
  ~SimpleTexture();
};

class SkyRenderer {
public:
  SkyRenderer(Level *level);
  ~SkyRenderer();

  void renderSky(float playerX, float playerY, float playerZ, const ScePspFVector3& lookDir);
  uint32_t getFogColor(float timeOfDay, const ScePspFVector3& lookDir);

private:
  Level *m_level;

  SkyPosVertex *m_skyVertices;
  SkyPosVertex *m_bottomVertices;
  SkyVertex *m_starVertices;
  SkyVertex *m_celestialVertices;

  int m_numSkyVertices;
  int m_numBottomVertices;
  int m_numStarVertices;

  SimpleTexture m_sunTex;
  SimpleTexture m_moonTex;
  SimpleTexture m_moonPhasesTex;

  uint32_t getSkyColor(float timeOfDay);
  bool getSunriseColor(float timeOfDay, float *outColor);
  float getStarBrightness(float timeOfDay);
  int getMoonPhase(float timeOfDay);
  float getBrightness(float timeOfDay);
};
