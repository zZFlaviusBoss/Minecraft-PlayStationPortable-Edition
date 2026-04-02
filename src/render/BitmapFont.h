#pragma once
#include "Texture.h"
#include <string>
#include <stdint.h>
#include <pspgu.h>

// Vertex format for 2D textured + colored
struct VertexTCO {
    float u, v;
    unsigned int color;
    float x, y, z;
};

class BitmapFont {
public:
    BitmapFont();
    ~BitmapFont();

    bool load(const char* path);
    void free();

    // Draw with shadow (like 4J drawShadow)
    void drawShadow(float x, float y, const std::string& text, uint32_t color, float scale = 1.0f);
    void drawShadowCentered(float x, float y, const std::string& text, uint32_t color, float scale = 1.0f);

    // Draw without shadow
    float drawString(float x, float y, const std::string& text, uint32_t color, float scale = 1.0f);
    void drawStringCentered(float x, float y, const std::string& text, uint32_t color, float scale = 1.0f);
    float getStringWidth(const std::string& text, float scale = 1.0f);

    Texture texture;
    int charWidth[256];  // auto-scanned from alpha
};
