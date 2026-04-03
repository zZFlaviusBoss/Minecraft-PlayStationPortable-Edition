#include "Tesselator.h"



Tesselator::Tesselator()
    : m_buffer(nullptr), m_maxVertices(0), m_vertexCount(0), m_u(0), m_v(0),
      m_color(0xFFFFFFFF) {}
Tesselator::~Tesselator() {}

void Tesselator::begin(CraftPSPVertex *buffer, int maxVertices) {
  m_buffer = buffer;
  m_maxVertices = maxVertices;
  m_vertexCount = 0;
}

void Tesselator::color(uint32_t c) { m_color = c; }

void Tesselator::tex(float u, float v) {
  m_u = u;
  m_v = v;
}

void Tesselator::vertex(float x, float y, float z) {
  if (m_vertexCount >= m_maxVertices - 1)
    return;
  CraftPSPVertex *v = &m_buffer[m_vertexCount++];
  v->u = m_u;
  v->v = m_v;
  v->color = m_color;
  v->x = x;
  v->y = y;
  v->z = z;
}

void Tesselator::addQuad(float u0, float v0, float u1, float v1, uint32_t c,
                         float x0, float y0, float z0, float x1, float y1,
                         float z1, float x2, float y2, float z2, float x3,
                         float y3, float z3) {
  addQuad(u0, v0, u1, v1, c, c, c, c, x0, y0, z0, x1, y1, z1, x2, y2, z2, x3,
          y3, z3);
}

void Tesselator::addQuad(float u0, float v0, float u1, float v1, uint32_t c0,
                         uint32_t c1, uint32_t c2, uint32_t c3, float x0,
                         float y0, float z0, float x1, float y1, float z1,
                         float x2, float y2, float z2, float x3, float y3,
                         float z3) {
  if (m_vertexCount + 6 > m_maxVertices)
    return;

  CraftPSPVertex *v = &m_buffer[m_vertexCount];

  // v0..v3 quad corners

  // Pick diagonal based on brightness (avoids light seams)
  uint32_t l03 = ((c0 >> 8) & 0xFF) + ((c3 >> 8) & 0xFF);
  uint32_t l12 = ((c1 >> 8) & 0xFF) + ((c2 >> 8) & 0xFF);

  if (l03 > l12) {
    // Flip diagonal: use triangles (0, 2, 3) and (0, 3, 1)
    *v++ = {u0, v0, c0, x0, y0, z0};
    *v++ = {u0, v1, c2, x2, y2, z2};
    *v++ = {u1, v1, c3, x3, y3, z3};

    *v++ = {u0, v0, c0, x0, y0, z0};
    *v++ = {u1, v1, c3, x3, y3, z3};
    *v++ = {u1, v0, c1, x1, y1, z1};
  } else {
    // Standard diagonal: use triangles (0, 2, 1) and (1, 2, 3)
    *v++ = {u0, v0, c0, x0, y0, z0};
    *v++ = {u0, v1, c2, x2, y2, z2};
    *v++ = {u1, v0, c1, x1, y1, z1};

    *v++ = {u1, v0, c1, x1, y1, z1};
    *v++ = {u0, v1, c2, x2, y2, z2};
    *v++ = {u1, v1, c3, x3, y3, z3};
  }

  m_vertexCount += 6;
}

int Tesselator::end() { return m_vertexCount; }
