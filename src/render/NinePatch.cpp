#include "NinePatch.h"
#include <pspgu.h>
#include <cmath>

struct VertTCO {
    float u, v;
    unsigned int c;
    float x, y, z;
};

void NinePatch::drawTexturedQuad(Texture* tex, float sx, float sy, float sw, float sh, 
                                 float u0, float v0, float u1, float v1, uint32_t color) {
    if (!tex || !tex->vramPtr) return;
    
    VertTCO* v = (VertTCO*)sceGuGetMemory(6 * sizeof(VertTCO));
    v[0] = {u0, v0, color, sx, sy, 0};
    v[1] = {u0, v1, color, sx, sy + sh, 0};
    v[2] = {u1, v0, color, sx + sw, sy, 0};
    v[3] = {u1, v0, color, sx + sw, sy, 0};
    v[4] = {u0, v1, color, sx, sy + sh, 0};
    v[5] = {u1, v1, color, sx + sw, sy + sh, 0};
    sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 6, nullptr, v);
}

NinePatch::NinePatch() : m_texture(nullptr), m_cornerSize(0), m_width(0.0f), m_height(0.0f), m_scale(1.0f), m_color(0xFFFFFFFF) {}

NinePatch::NinePatch(Texture& texture, int cornerSize) 
    : m_texture(&texture), m_cornerSize(cornerSize), m_width(0.0f), m_height(0.0f), m_scale(1.0f), m_color(0xFFFFFFFF) {}

void NinePatch::setTexture(Texture& texture) {
    m_texture = &texture;
}

void NinePatch::setCornerSize(int size) {
    m_cornerSize = size;
}

void NinePatch::setSize(float width, float height) {
    m_width = width;
    m_height = height;
}

void NinePatch::setScale(float scale) {
    m_scale = scale;
}

void NinePatch::setColor(uint32_t color) {
    m_color = color;
}

void NinePatch::render(float dstX, float dstY) {
    if (!m_texture || !m_texture->vramPtr || m_cornerSize <= 0) return;
    
    m_texture->bind();

    int texWidth = m_texture->origWidth;
    int texHeight = m_texture->origHeight;

    int edgeWidth = texWidth - 2 * m_cornerSize;
    int edgeHeight = texHeight - 2 * m_cornerSize;

    if (edgeWidth < 0 || edgeHeight < 0) return;

    float renderedCornerSize = (float)m_cornerSize * m_scale;

    // x array: left, left-inner, right-inner, right
    float x[4] = {dstX, dstX + renderedCornerSize, dstX + m_width - renderedCornerSize, dstX + m_width};
    // y array: top, top-inner, bottom-inner, bottom
    float y[4] = {dstY, dstY + renderedCornerSize, dstY + m_height - renderedCornerSize, dstY + m_height};

    if (m_width < 2 * renderedCornerSize) {
        float mid = dstX + m_width / 2.0f;
        x[1] = mid;
        x[2] = mid;
    }
    if (m_height < 2 * renderedCornerSize) {
        float mid = dstY + m_height / 2.0f;
        y[1] = mid;
        y[2] = mid;
    }

    // u, v source maps
    float u[4] = {0.0f, (float)m_cornerSize, (float)(texWidth - m_cornerSize), (float)texWidth};
    float v[4] = {0.0f, (float)m_cornerSize, (float)(texHeight - m_cornerSize), (float)texHeight};

    for(int i = 0; i < 4; i++) {
        x[i] = roundf(x[i]);
        y[i] = roundf(y[i]);
    }

    // 0: Top-Left
    drawTexturedQuad(m_texture, x[0], y[0], x[1]-x[0], y[1]-y[0], u[0], v[0], u[1], v[1], m_color);
    // 1: Top
    drawTexturedQuad(m_texture, x[1], y[0], x[2]-x[1], y[1]-y[0], u[1], v[0], u[2], v[1], m_color);
    // 2: Top-Right
    drawTexturedQuad(m_texture, x[2], y[0], x[3]-x[2], y[1]-y[0], u[2], v[0], u[3], v[1], m_color);

    // 3: Left
    drawTexturedQuad(m_texture, x[0], y[1], x[1]-x[0], y[2]-y[1], u[0], v[1], u[1], v[2], m_color);
    // 4: Center
    drawTexturedQuad(m_texture, x[1], y[1], x[2]-x[1], y[2]-y[1], u[1], v[1], u[2], v[2], m_color);
    // 5: Right
    drawTexturedQuad(m_texture, x[2], y[1], x[3]-x[2], y[2]-y[1], u[2], v[1], u[3], v[2], m_color);

    // 6: Bottom-Left
    drawTexturedQuad(m_texture, x[0], y[2], x[1]-x[0], y[3]-y[2], u[0], v[2], u[1], v[3], m_color);
    // 7: Bottom
    drawTexturedQuad(m_texture, x[1], y[2], x[2]-x[1], y[3]-y[2], u[1], v[2], u[2], v[3], m_color);
    // 8: Bottom-Right
    drawTexturedQuad(m_texture, x[2], y[2], x[3]-x[2], y[3]-y[2], u[2], v[2], u[3], v[3], m_color);
}
