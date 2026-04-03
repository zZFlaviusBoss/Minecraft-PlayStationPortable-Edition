#pragma once
#include "../world/Chunk.h"
#include <stdint.h>

class Tesselator {
public:
  Tesselator();
  ~Tesselator();

  void begin(CraftPSPVertex *buffer, int maxVertices);

  void color(uint32_t c);
  void tex(float u, float v);
  void vertex(float x, float y, float z);

  // Helper for adding a quad directly
  void addQuad(float u0, float v0, float u1, float v1, uint32_t c, float x0,
               float y0, float z0, float x1, float y1, float z1, float x2,
               float y2, float z2, float x3, float y3, float z3);

  // Helper for adding a quad with per-vertex colors (for ambient occlusion)
  void addQuad(float u0, float v0, float u1, float v1, uint32_t c0, uint32_t c1,
               uint32_t c2, uint32_t c3, float x0, float y0, float z0, float x1,
               float y1, float z1, float x2, float y2, float z2, float x3,
               float y3, float z3);

  int end(); // returns vertex count

private:
  CraftPSPVertex *m_buffer;
  int m_maxVertices;
  int m_vertexCount;

  float m_u, m_v;
  uint32_t m_color;
};
