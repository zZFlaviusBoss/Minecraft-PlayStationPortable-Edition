#include "WorldLoadingScreen.h"
#include <pspgu.h>
#include <pspgum.h>
#include <pspiofilemgr.h>
#include <psprtc.h>
#include <pspkernel.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

namespace {
struct VertTCO {
  float u, v;
  unsigned int c;
  float x, y, z;
};

static void drawQuad2D(float sx, float sy, float sw, float sh, float u0, float v0,
                       float u1, float v1, uint32_t color = 0xFFFFFFFF) {
  VertTCO* v = (VertTCO*)sceGuGetMemory(6 * sizeof(VertTCO));
  v[0] = {u0, v0, color, sx, sy, 0};
  v[1] = {u0, v1, color, sx, sy + sh, 0};
  v[2] = {u1, v0, color, sx + sw, sy, 0};
  v[3] = {u1, v0, color, sx + sw, sy, 0};
  v[4] = {u0, v1, color, sx, sy + sh, 0};
  v[5] = {u1, v1, color, sx + sw, sy + sh, 0};
  sceGuDrawArray(GU_TRIANGLES,
                 GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF |
                     GU_TRANSFORM_2D,
                 6, nullptr, v);
}

static void drawRect2D(float sx, float sy, float sw, float sh, uint32_t color) {
  sceGuDisable(GU_TEXTURE_2D);
  struct Vert { unsigned int c; float x, y, z; };
  Vert *v = (Vert*)sceGuGetMemory(6 * sizeof(Vert));
  v[0] = {color, sx,      sy,      0};
  v[1] = {color, sx,      sy + sh, 0};
  v[2] = {color, sx + sw, sy,      0};
  v[3] = {color, sx + sw, sy,      0};
  v[4] = {color, sx,      sy + sh, 0};
  v[5] = {color, sx + sw, sy + sh, 0};
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 6, nullptr, v);
  sceGuEnable(GU_TEXTURE_2D);
}
}  // namespace

WorldLoadingScreen::WorldLoadingScreen()
    : m_scrollOffset(0.0f), m_tipTimer(0.0f), m_tipCount(0),
      m_tipsBuf(nullptr), m_resourcesLoaded(false) {
  strcpy(m_currentTip, "Loading...");
  memset(m_tipLineStarts, 0, sizeof(m_tipLineStarts));
}

WorldLoadingScreen::~WorldLoadingScreen() {
  releaseResources();
}

bool WorldLoadingScreen::init() {
  if (m_resourcesLoaded) return true;

  // Panorama + Logo (same assets as ConsoleMainMenu)
  m_texPanoL.load("res/title/background/panorama_0.png");
  m_texPanoR.load("res/title/background/panorama_1.png");
  m_texLogo.load("res/title/mclogo.png");

  // Tip panel - pointertextpanel.png is 24x24
  m_texTipPanel.load("res/gui/PanelsAndTabs/pointertextpanel.png");
  m_tipPanel.setTexture(m_texTipPanel);
  m_tipPanel.setCornerSize(8);
  m_tipPanel.setScale(1.0f);

  m_font.load("res/font/Default.png");

  // Load all tips and pick the first random one
  loadTips();
  pickRandomTip();

  m_resourcesLoaded = m_texPanoL.vramPtr && m_texPanoR.vramPtr &&
                      m_texLogo.vramPtr && m_font.texture.vramPtr;
  return m_resourcesLoaded;
}

void WorldLoadingScreen::loadTips() {
  SceUID fd = sceIoOpen("res/tips.txt", PSP_O_RDONLY, 0777);
  if (fd < 0) {
    m_tipsBuf = nullptr;
    m_tipCount = 0;
    return;
  }

  int sz = (int)sceIoLseek(fd, 0, PSP_SEEK_END);
  sceIoLseek(fd, 0, PSP_SEEK_SET);
  if (sz <= 0 || sz >= 64 * 1024) {
    sceIoClose(fd);
    m_tipsBuf = nullptr;
    m_tipCount = 0;
    return;
  }

  m_tipsBuf = new char[sz + 1];
  sceIoRead(fd, m_tipsBuf, sz);
  sceIoClose(fd);
  m_tipsBuf[sz] = '\0';

  // Parse lines
  m_tipCount = 0;
  char* line = m_tipsBuf;
  for (int i = 0; i <= sz && m_tipCount < 256; ++i) {
    if (m_tipsBuf[i] == '\n' || m_tipsBuf[i] == '\0') {
      m_tipsBuf[i] = '\0';
      int len = (int)strlen(line);
      if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';
      if (line[0] != '\0') {
        m_tipLineStarts[m_tipCount++] = (int)(line - m_tipsBuf);
      }
      line = m_tipsBuf + i + 1;
    }
  }

  // Seed RNG once
  u64 tick;
  sceRtcGetCurrentTick(&tick);
  srand((unsigned int)(tick ^ sceKernelGetSystemTimeLow()));
}

void WorldLoadingScreen::pickRandomTip() {
  if (m_tipCount <= 0 || !m_tipsBuf) {
    strcpy(m_currentTip, "Minecraft PSP!");
    return;
  }
  int idx = rand() % m_tipCount;
  strncpy(m_currentTip, m_tipsBuf + m_tipLineStarts[idx], sizeof(m_currentTip) - 1);
  m_currentTip[sizeof(m_currentTip) - 1] = '\0';
  m_tipTimer = 0.0f;
}

void WorldLoadingScreen::releaseResources() {
  m_texPanoL.free();
  m_texPanoR.free();
  m_texLogo.free();
  m_texTipPanel.free();
  m_font.free();
  if (m_tipsBuf) { delete[] m_tipsBuf; m_tipsBuf = nullptr; }
  m_resourcesLoaded = false;
}

void WorldLoadingScreen::update(float dt) {
  m_scrollOffset += dt * 0.5f;
}

// Draw text with a proper outline: 8-direction offset in black, then white on top
void WorldLoadingScreen::drawOutlinedCentered(float cx, float cy, const char* text,
                                               uint32_t color, float scale) {
  float w = m_font.getStringWidth(text, scale);
  float x = cx - w * 0.5f;
  uint32_t outlineColor = 0xFF000000;

  // Draw outline: 8 directions (cardinal + diagonal) for a solid border
  for (int dx = -1; dx <= 1; dx++) {
    for (int dy = -1; dy <= 1; dy++) {
      if (dx == 0 && dy == 0) continue;
      m_font.drawString(x + (float)dx, cy + (float)dy, text, outlineColor, scale);
    }
  }

  // Draw main text on top
  m_font.drawString(x, cy, text, color, scale);
}

void WorldLoadingScreen::render(float progress, const char* status) {
  if (!m_resourcesLoaded) return;

  // Advance scroll each frame (~60fps estimate)
  m_scrollOffset += 0.016f * 0.5f;

  // Rotate tips every 8 seconds
  m_tipTimer += 0.016f;
  if (m_tipTimer >= 8.0f) {
    pickRandomTip();
  }

  sceGuDisable(GU_DEPTH_TEST);
  sceGuDisable(GU_FOG);
  sceGuDisable(GU_LIGHTING);
  sceGuDisable(GU_CULL_FACE);
  sceGuEnable(GU_BLEND);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuEnable(GU_TEXTURE_2D);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  sceGuTexFilter(GU_NEAREST, GU_NEAREST);

  // ---- 1. Scrolling Panorama Background ----
  if (m_texPanoL.vramPtr && m_texPanoR.vramPtr) {
    sceGuDisable(GU_BLEND);

    float pLw = (float)m_texPanoL.origWidth;
    float pRw = (float)m_texPanoR.origWidth;
    float ph = (float)m_texPanoL.origHeight;
    float totalW = pLw + pRw;

    float scale = 272.0f / ph;
    float scaledTotalW = totalW * scale;

    float scrollPx = fmodf(m_scrollOffset * 30.0f, scaledTotalW);

    float clampU0_L = 0.0f, clampU1_L = pLw - 0.5f;
    float clampU0_R = 0.0f, clampU1_R = pRw - 0.5f;
    float clampV0 = 0.0f, clampV1 = ph - 0.5f;

    for (int pass = 0; pass < 2; pass++) {
      float baseX = -scrollPx + pass * scaledTotalW;

      float lx = baseX;
      float lw = pLw * scale;
      if (lx + lw > 0 && lx < 480) {
        m_texPanoL.bind();
        sceGuTexFilter(GU_LINEAR, GU_LINEAR);
        sceGuTexWrap(GU_CLAMP, GU_CLAMP);
        drawQuad2D(lx, 0, lw, 272, clampU0_L, clampV0, clampU1_L,
                   clampV1, 0xFFFFFFFF);
      }

      float rx = baseX + pLw * scale;
      float rw = pRw * scale;
      if (rx + rw > 0 && rx < 480) {
        m_texPanoR.bind();
        sceGuTexFilter(GU_LINEAR, GU_LINEAR);
        sceGuTexWrap(GU_CLAMP, GU_CLAMP);
        drawQuad2D(rx, 0, rw, 272, clampU0_R, clampV0, clampU1_R, clampV1,
                   0xFFFFFFFF);
      }
    }

    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuEnable(GU_BLEND);
  }

  // ---- 2. Minecraft Logo ----
  if (m_texLogo.vramPtr) {
    m_texLogo.bind();
    float lw = (float)m_texLogo.origWidth;
    float lh = (float)m_texLogo.origHeight;
    float logoW = 248.0f;
    float logoH = logoW * (lh / lw);
    float logoX = (480.0f - logoW) / 2.0f;
    float logoY = 27.0f;
    drawQuad2D(logoX, logoY, logoW, logoH, 0, 0, lw, lh);
  }

  // ---- Layout constants ----
  const float barW = 375.0f;
  const float barH = 10.0f;
  const float barX = (480.0f - barW) / 2.0f;
  const float barY = 140.0f;

  // ---- 3. Title: "Initializing server" with outline ----
  drawOutlinedCentered(240.0f, 100.0f, "Initializing server", 0xFFFFFFFF, 2.0f);

  // ---- 4. Status text: left-aligned at bar X, just above the bar ----
  if (status && status[0]) {
    m_font.drawShadow(barX + 2.0f, barY - 10.0f, status, 0xFFFFFFFF, 1.0f);
  }

  // ---- 5. Progress Bar: gray bg + green fill inset inside ----
  const float inset = 2.0f;
  drawRect2D(barX, barY, barW, barH, 0xFF606060);  // Gray background
  if (progress > 0.0f) {
    float maxFillW = barW - inset * 2.0f;
    float fillW = maxFillW * progress;
    if (fillW > maxFillW) fillW = maxFillW;
    drawRect2D(barX + inset, barY + inset, fillW, barH - inset * 2.0f, 0xFF00FF00);
  }

  // ---- 6. Tip Panel ----
  float tipPanelW = barW;
  float tipPanelH = 70.0f;
  float tipPanelX = barX;
  float tipPanelY = barY + barH + 16.0f;

  m_tipPanel.setSize(tipPanelW, tipPanelH);
  m_tipPanel.render(tipPanelX, tipPanelY);

  // ---- 7. Tip Text: word-wrap, each line centered ----
  if (m_currentTip[0]) {
    float tipScale = 1.0f;
    float maxLineW = tipPanelW - 20.0f;  // 10px padding each side
    float lineH = 12.0f;  // line height

    // Word-wrap into lines
    const char* src = m_currentTip;
    char lines[8][128];
    int lineCount = 0;

    while (*src && lineCount < 8) {
      // Build a line word by word
      char lineBuf[128];
      int lineLen = 0;
      lineBuf[0] = '\0';

      while (*src && lineCount < 8) {
        // Find next word
        const char* wordStart = src;
        while (*src && *src != ' ') src++;
        int wordLen = (int)(src - wordStart);

        // Try adding this word
        char testBuf[128];
        if (lineLen > 0) {
          snprintf(testBuf, sizeof(testBuf), "%s %.*s", lineBuf, wordLen, wordStart);
        } else {
          snprintf(testBuf, sizeof(testBuf), "%.*s", wordLen, wordStart);
        }

        float testW = m_font.getStringWidth(testBuf, tipScale);
        if (testW > maxLineW && lineLen > 0) {
          // This word doesn't fit, start a new line
          memcpy(lines[lineCount], lineBuf, 128);
          lines[lineCount][127] = '\0';
          lineCount++;
          // Don't advance src past the word - reprocess it on next line
          src = wordStart;
          break;
        } else {
          memcpy(lineBuf, testBuf, 128);
          lineBuf[127] = '\0';
          lineLen = (int)strlen(lineBuf);
        }

        // Skip spaces
        while (*src == ' ') src++;

        // If end of string, flush this line
        if (*src == '\0') {
          memcpy(lines[lineCount], lineBuf, 128);
          lines[lineCount][127] = '\0';
          lineCount++;
          break;
        }
      }
    }

    // Draw lines centered vertically and horizontally in the panel
    float totalTextH = lineCount * lineH;
    float startY = tipPanelY + (tipPanelH - totalTextH) / 2.0f;
    float centerX = tipPanelX + tipPanelW / 2.0f;

    for (int i = 0; i < lineCount; i++) {
      m_font.drawShadowCentered(centerX, startY + i * lineH, lines[i], 0xFFFFFFFF, tipScale);
    }
  }

  sceGuEnable(GU_DEPTH_TEST);
}

void WorldLoadingScreen::setTip(const char* tip) {
  if (tip) {
    strncpy(m_currentTip, tip, sizeof(m_currentTip) - 1);
    m_currentTip[sizeof(m_currentTip) - 1] = '\0';
  }
}
