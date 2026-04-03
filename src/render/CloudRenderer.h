#pragma once

#include "SkyRenderer.h" // For SkyVertex
#include "world/Level.h"
#include <pspgu.h>
#include <pspgum.h>

class CloudRenderer {
public:
  CloudRenderer(Level *level);
  ~CloudRenderer();

  void renderClouds(float playerX, float playerY, float playerZ, float alpha, uint32_t fogColor);

private:
  Level *m_level;

  SkyVertex *m_cloudVertices;

  uint8_t *m_cloudData;
  int m_cloudTexWidth;
  int m_cloudTexHeight;

  float m_lastCloudPx;
  float m_lastCloudPz;
  float m_lastCloudSnappedOffset;
  bool m_lastWasInClouds;
  int m_numCloudVertices;

  float m_cloudOffset;

  SimpleTexture m_cloudsTex;
};
