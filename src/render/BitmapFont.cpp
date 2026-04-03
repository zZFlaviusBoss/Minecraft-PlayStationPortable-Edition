#include "BitmapFont.h"
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stdlib.h>
#include <string.h>


BitmapFont::BitmapFont() {
    for (int i = 0; i < 256; i++) charWidth[i] = 6; // fallback
}

BitmapFont::~BitmapFont() { free(); }

bool BitmapFont::load(const char* path) {
    if (!texture.load(path)) return false;

    // Auto-scan char widths from the texture's alpha channel
    int imgW = texture.origWidth;
    int imgH = texture.origHeight;
    int cellW = imgW / 16;
    int cellH = imgH / 16;
    unsigned char* pixels = (unsigned char*)texture.vramPtr;
    int stride = texture.width;

    for (int i = 0; i < 256; i++) {
        int col = i % 16;
        int row = i / 16;

        // Scan right-to-left to find last non-transparent column
        int x = cellW - 1;
        for (; x >= 0; x--) {
            int xPixel = col * cellW + x;
            bool emptyColumn = true;
            for (int y = 0; y < cellH && emptyColumn; y++) {
                int yPixel = row * cellW + y;
                unsigned char alpha = pixels[(yPixel * stride + xPixel) * 4 + 3];
                if (alpha != 0) emptyColumn = false;
            }
            if (!emptyColumn) break;
        }

        if (i == ' ') x = 4 - 2;
        charWidth[i] = x + 2;
    }

    return true;
}

void BitmapFont::free() { texture.free(); }

float BitmapFont::getStringWidth(const std::string& text, float scale) {
    float w = 0;
    for (size_t i = 0; i < text.length(); i++)
        w += charWidth[(unsigned char)text[i]] * scale;
    return w;
}

void BitmapFont::drawShadow(float x, float y, const std::string& text, uint32_t color, float scale) {
    uint32_t a = (color >> 24) & 0xFF;
    uint32_t shadowColor = (a << 24) | 0x000000; // Completely black shadow

    drawString(x + 0.5f * scale, y + 0.5f * scale, text, shadowColor, scale);
    drawString(x, y, text, color, scale);
}

void BitmapFont::drawShadowCentered(float x, float y, const std::string& text, uint32_t color, float scale) {
    float len = getStringWidth(text, scale);
    drawShadow(x - len * 0.5f, y, text, color, scale);
}

void BitmapFont::drawStringCentered(float x, float y, const std::string& text, uint32_t color, float scale) {
    float len = getStringWidth(text, scale);
    drawString(x - len * 0.5f, y, text, color, scale);
}


float BitmapFont::drawString(float x, float y, const std::string& text, uint32_t color, float scale) {
    if (!texture.vramPtr) return 0;

    int numChars = 0;
    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] != ' ') numChars++;
    }

    if (numChars == 0) {
        float w = 0;
        for (size_t i = 0; i < text.length(); i++) {
            w += charWidth[(unsigned char)text[i]] * scale;
        }
        return w;
    }

    texture.bind();

    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDisable(GU_DEPTH_TEST);

    VertexTCO *v = (VertexTCO*)sceGuGetMemory(6 * numChars * sizeof(VertexTCO));
    int vIdx = 0;

    float currentX = x;
    float cellW = (float)(texture.origWidth / 16);
    float cellH = (float)(texture.origHeight / 16);
    float drawH = cellH * scale;

    for (size_t i = 0; i < text.length(); i++) {
        unsigned char c = text[i];
        if (c == ' ') { currentX += charWidth[c] * scale; continue; }

        int row = c / 16;
        int col = c % 16;

        float u0 = col * cellW;
        float v0 = row * cellH;
        float u1 = u0 + cellW;
        float v1 = v0 + cellH;

        float px0 = currentX;
        float py0 = y;
        float px1 = currentX + cellW * scale;
        float py1 = y + drawH;

        v[vIdx++] = {u0, v0, color, px0, py0, 0};
        v[vIdx++] = {u0, v1, color, px0, py1, 0};
        v[vIdx++] = {u1, v0, color, px1, py0, 0};
        v[vIdx++] = {u1, v0, color, px1, py0, 0};
        v[vIdx++] = {u0, v1, color, px0, py1, 0};
        v[vIdx++] = {u1, v1, color, px1, py1, 0};

        currentX += charWidth[c] * scale;
    }

    sceGuDrawArray(GU_TRIANGLES,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   6 * numChars, 0, v);

    return currentX - x;
}
