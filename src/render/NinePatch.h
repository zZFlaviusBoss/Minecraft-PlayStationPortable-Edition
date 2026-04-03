#pragma once

#include "Texture.h"

// Ported from PicToCraft sfml NinePatch logic for sceGu
class NinePatch {
public:
    NinePatch();
    
    // texture: the full panel texture (e.g. Crafting_Panel)
    // cornerSize: how many pixels in the corner to not stretch
    // edgeThickness: mostly for drawing the border thickness, similar to sfml class
    NinePatch(Texture& texture, int cornerSize);

    void setTexture(Texture& texture);
    void setCornerSize(int size);
    
    // Set the overall dimension to draw the nine patch at
    void setSize(float width, float height);
    
    void setScale(float scale);
    
    void setColor(uint32_t color);
    
    void render(float x, float y);

private:
    Texture* m_texture;
    int m_cornerSize;
    float m_width, m_height;
    float m_scale;
    uint32_t m_color;
    
    // Internal generic textured quad draw utility to avoid duplicating
    static void drawTexturedQuad(Texture* tex, float sx, float sy, float sw, float sh, 
                                 float u0, float v0, float u1, float v1, uint32_t color);
};
