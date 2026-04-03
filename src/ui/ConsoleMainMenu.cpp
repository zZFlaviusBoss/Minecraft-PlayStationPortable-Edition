#include "ConsoleMainMenu.h"

#include "../input/PSPInput.h"

#include <math.h>
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <psprtc.h>
#include <string.h>
#include <stdlib.h>
#include <string>

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

static void drawTex2D(Texture& t, float sx, float sy, float sw, float sh) {
  if (t.vramPtr) {
    t.bind();
    drawQuad2D(sx, sy, sw, sh, 0, 0, t.origWidth, t.origHeight);
  }
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

ConsoleMainMenu::ConsoleMainMenu()
    : m_selectedIndex(0),
      m_saveSelectedIndex(0),
      m_menuRenderTimer(0.0f),
      m_splashScaleTimer(0.0f),
      m_scrollOffset(0.0f),
      m_joinLoadingTimer(0.0f),
      m_tabTransition(0.0f),
      m_pendingAction(MainMenuAction::None),
      m_mode(MenuStateMode::MainButtons),
      m_worldSelectTab(WorldSelectTab::StartGame),
      m_currentSplash("Minecraft PSP!"),
      m_resourcesLoaded(false) {
  m_splashStorage[0] = '\0';
  m_items[0] = {"Play Game", true};
  m_items[1] = {"Leaderboards", false};
  m_items[2] = {"Trophies", false};
  m_items[3] = {"Help & Options", false};
  m_items[4] = {"Minecraft Store", false};
  m_items[5] = {"Exit Game", false};
}

ConsoleMainMenu::~ConsoleMainMenu() {}

void ConsoleMainMenu::loadSplash() {
  SceUID fd = sceIoOpen("res/splashes.txt", PSP_O_RDONLY, 0777);
  if (fd < 0) {
    strcpy(m_splashStorage, "Minecraft PSP!");
    m_currentSplash = m_splashStorage;
    return;
  }

  int sz = (int)sceIoLseek(fd, 0, PSP_SEEK_END);
  sceIoLseek(fd, 0, PSP_SEEK_SET);
  if (sz <= 0 || sz >= 64 * 1024) {
    sceIoClose(fd);
    strcpy(m_splashStorage, "Minecraft PSP!");
    m_currentSplash = m_splashStorage;
    return;
  }

  char* buf = new char[sz + 1];
  sceIoRead(fd, buf, sz);
  sceIoClose(fd);
  buf[sz] = '\0';

  int lineStarts[256];
  int lineCount = 0;
  char* line = buf;
  for (int i = 0; i <= sz && lineCount < 256; ++i) {
    if (buf[i] == '\n' || buf[i] == '\0') {
      buf[i] = '\0';
      int len = (int)strlen(line);
      if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';
      if (line[0] != '\0') {
        lineStarts[lineCount++] = (int)(line - buf);
      }
      line = buf + i + 1;
    }
  }

  if (lineCount <= 0) {
    strcpy(m_splashStorage, "Minecraft PSP!");
  } else {
    static bool s_seeded = false;
    if (!s_seeded) {
      u64 tick;
      sceRtcGetCurrentTick(&tick);
      srand((unsigned int)(tick ^ sceKernelGetSystemTimeLow()));
      s_seeded = true;
    }
    int idx = rand() % lineCount;
    strncpy(m_splashStorage, buf + lineStarts[idx], sizeof(m_splashStorage) - 1);
    m_splashStorage[sizeof(m_splashStorage) - 1] = '\0';
  }
  delete[] buf;
  m_currentSplash = m_splashStorage;
}

bool ConsoleMainMenu::init() {
  if (m_resourcesLoaded) return true;

  m_texGui.load("res/gui/gui.png");
  m_texLogo.load("res/title/mclogo.png");
  m_texPanoL.load("res/title/background/panorama_0.png");
  m_texPanoR.load("res/title/background/panorama_1.png");
  m_texBtnCross.load("res/gui/buttons/psp_cross.png");
  m_texBtnCircle.load("res/gui/buttons/psp_circle.png");
  m_texListNorm.load("res/gui/ListButton_Norm.png");
  m_texListOver.load("res/gui/ListButton_Over.png");
  m_font.load("res/font/Default.png");
  loadSplash();

  m_texFullPanel.load("res/gui/PanelsAndTabs/panel.png");
  m_mainPanel.setTexture(m_texFullPanel);
  m_mainPanel.setCornerSize(16);
  m_mainPanel.setScale(0.5f);

  m_texRecessPanel.load("res/gui/PanelsAndTabs/panel_recess.png");
  m_leftRecess.setTexture(m_texRecessPanel);
  m_leftRecess.setCornerSize(16);
  m_leftRecess.setScale(0.5f);

  m_rightSquare.setTexture(m_texRecessPanel);
  m_rightSquare.setCornerSize(16);
  m_rightSquare.setScale(0.5f);
  m_rightSquare.setColor(0xFFFFFFFF);

  m_selectedIndex = 0;
  m_saveSelectedIndex = 0;
  m_mode = MenuStateMode::MainButtons;
  m_worldSelectTab = WorldSelectTab::StartGame;
  m_menuRenderTimer = 0.0f;
  m_splashScaleTimer = 0.0f;
  m_scrollOffset = 0.0f;
  m_joinLoadingTimer = 0.0f;
  m_pendingAction = MainMenuAction::None;
  m_resourcesLoaded = m_texGui.vramPtr && m_texLogo.vramPtr && m_texPanoL.vramPtr &&
                      m_texPanoR.vramPtr && m_font.texture.vramPtr &&
                      m_texBtnCross.vramPtr && m_texBtnCircle.vramPtr;
  return m_resourcesLoaded;
}

void ConsoleMainMenu::releaseResources() {
  m_texGui.free();
  m_texLogo.free();
  m_texPanoL.free();
  m_texPanoR.free();
  m_texBtnCross.free();
  m_texBtnCircle.free();
  m_texFullPanel.free();
  m_texRecessPanel.free();
  m_texListNorm.free(); m_texListOver.free();
  m_font.free();
  m_resourcesLoaded = false;
}

void ConsoleMainMenu::moveSelection(int dir) {
  int next = m_selectedIndex + dir;
  if (next < 0 || next >= kMenuItemCount) return;
  m_items[m_selectedIndex].selected = false;
  m_selectedIndex = next;
  m_items[m_selectedIndex].selected = true;
}

void ConsoleMainMenu::update(float dt) {
  PSPInput_Update();
  m_menuRenderTimer += dt;
  m_splashScaleTimer += dt * 2.5f;
  m_scrollOffset += dt * 0.5f;
  m_joinLoadingTimer += dt;
  // Animate tab transition: 0=StartGame, 1=JoinGame
  float tabTarget = (m_worldSelectTab == WorldSelectTab::JoinGame) ? 1.0f : 0.0f;
  m_tabTransition += (tabTarget - m_tabTransition) * dt * 8.0f;  // ~0.25s fade

  if (m_mode == MenuStateMode::MainButtons) {
    if (PSPInput_JustPressed(PSP_CTRL_UP)) moveSelection(-1);
    if (PSPInput_JustPressed(PSP_CTRL_DOWN)) moveSelection(1);
    if (PSPInput_JustPressed(PSP_CTRL_CIRCLE)) {
      m_pendingAction = MainMenuAction::ExitGame;
      return;
    }

    if (PSPInput_JustPressed(PSP_CTRL_CROSS) ||
        PSPInput_JustPressed(PSP_CTRL_START)) {
      if (m_selectedIndex == 0) {
        m_mode = MenuStateMode::WorldSelect;
        m_worldSelectTab = WorldSelectTab::StartGame;
        m_saveSelectedIndex = 0;
        m_joinLoadingTimer = 0.0f;  // reset load timers
      } else if (m_selectedIndex == 5) {
        m_pendingAction = MainMenuAction::ExitGame;
      }
    }
  } else if (m_mode == MenuStateMode::WorldSelect) {
    if (PSPInput_JustPressed(PSP_CTRL_LEFT) || PSPInput_JustPressed(PSP_CTRL_RIGHT)) {
      if (m_worldSelectTab == WorldSelectTab::StartGame) {
        m_worldSelectTab = WorldSelectTab::JoinGame;
      } else {
        m_worldSelectTab = WorldSelectTab::StartGame;
      }
    }

    if (m_worldSelectTab == WorldSelectTab::StartGame) {
    if (PSPInput_JustPressed(PSP_CTRL_UP)) {
      if (m_saveSelectedIndex > 0) m_saveSelectedIndex--;
    }
    if (PSPInput_JustPressed(PSP_CTRL_DOWN)) {
      if (m_saveSelectedIndex < (kWorldItemCount - 1)) m_saveSelectedIndex++;
    }
    }
    if (PSPInput_JustPressed(PSP_CTRL_CIRCLE)) {
      m_mode = MenuStateMode::MainButtons;
    }
    if (PSPInput_JustPressed(PSP_CTRL_CROSS) || PSPInput_JustPressed(PSP_CTRL_START)) {
      if (m_worldSelectTab == WorldSelectTab::StartGame) {
        if (m_joinLoadingTimer >= 0.8f) {
           m_pendingAction = MainMenuAction::StartGame;
        }
      }
    }
  }
}

void ConsoleMainMenu::render(int screenWidth, int screenHeight) {
  (void)screenWidth;
  (void)screenHeight;

  sceGuDisable(GU_DEPTH_TEST);
  sceGuDisable(GU_FOG);
  sceGuDisable(GU_LIGHTING);
  sceGuDisable(GU_CULL_FACE);
  sceGuEnable(GU_BLEND);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuEnable(GU_TEXTURE_2D);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  sceGuTexFilter(GU_NEAREST, GU_NEAREST);

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

  if (m_mode == MenuStateMode::MainButtons) {
    uint64_t timeUs = sceKernelGetSystemTimeWide();
    float timeS = (timeUs % 1000000) / 1000000.0f;
    float splashS = 1.8f - fabsf(sinf(timeS * 3.14159265f * 2.0f)) * 0.1f;
    float txtW = m_font.getStringWidth(m_currentSplash, 1.0f);
    splashS = splashS * 100.0f / (txtW + 32.0f);

    float cx = 480.0f * 0.5f + 90.0f;
    float cy = 70.0f;
    float angle = -20.0f * (3.14159265358979323846f / 180.0f);
    float cosA = cosf(angle);
    float sinA = sinf(angle);

    float startX = (-txtW / 2.0f) * splashS;
    float startY = -4.0f * splashS;

    uint32_t col = 0xFF00FFFF;
    uint32_t shadowCol = 0xFF003F3F;

    for (int pass = 0; pass < 2; pass++) {
      uint32_t c = (pass == 0) ? shadowCol : col;
      float ox = (pass == 0) ? 1.0f * splashS : 0.0f;
      float oy = (pass == 0) ? 1.0f * splashS : 0.0f;

      if (m_font.texture.vramPtr) {
        m_font.texture.bind();

        int numChars = 0;
        for (const char* p = m_currentSplash; *p; ++p) {
          if (*p != ' ') numChars++;
        }

        if (numChars > 0) {
          VertTCO* v = (VertTCO*)sceGuGetMemory(6 * numChars * sizeof(VertTCO));
          int vIdx = 0;

          float cellW = (float)(m_font.texture.origWidth / 16);
          float cellH = (float)(m_font.texture.origHeight / 16);

          float currentX = startX + ox;
          float y = startY + oy;

          for (const char* p = m_currentSplash; *p; ++p) {
            unsigned char ch = (unsigned char)*p;
            if (ch == ' ') {
              currentX += m_font.charWidth[ch] * splashS;
              continue;
            }

            int row = ch / 16;
            int colPos = ch % 16;
            float u0 = colPos * cellW;
            float v0 = row * cellH;
            float u1 = u0 + cellW;
            float v1 = v0 + cellH;

            float lx0 = currentX;
            float ly0 = y;
            float lx1 = currentX + cellW * splashS;
            float ly1 = y + cellH * splashS;

            auto transform = [&](float lx, float ly, float& sx, float& sy) {
              sx = cx + (lx * cosA - ly * sinA);
              sy = cy + (lx * sinA + ly * cosA);
            };

            float p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y;
            transform(lx0, ly0, p0x, p0y);
            transform(lx0, ly1, p1x, p1y);
            transform(lx1, ly0, p2x, p2y);
            transform(lx1, ly1, p3x, p3y);

            v[vIdx++] = {u0, v0, c, p0x, p0y, 0};
            v[vIdx++] = {u0, v1, c, p1x, p1y, 0};
            v[vIdx++] = {u1, v0, c, p2x, p2y, 0};
            v[vIdx++] = {u1, v0, c, p2x, p2y, 0};
            v[vIdx++] = {u0, v1, c, p1x, p1y, 0};
            v[vIdx++] = {u1, v1, c, p3x, p3y, 0};

            currentX += m_font.charWidth[ch] * splashS;
          }

          sceGuDrawArray(GU_TRIANGLES,
                         GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF |
                             GU_TRANSFORM_2D,
                         6 * numChars, 0, v);
        }
      }
    }
  }

  if (m_mode == MenuStateMode::MainButtons) {
    if (m_texGui.vramPtr) {
      m_texGui.bind();
      const float BTN_W = 200.0f;
      const float BTN_H = 20.0f;
      const float BTN_GAP = 4.0f;
      const float startX = (480.0f - BTN_W) * 0.5f;
      const float startY = 100.0f;

      for (int i = 0; i < kMenuItemCount; ++i) {
        float bx = startX;
        float by = startY + i * (BTN_H + BTN_GAP);
        float srcY = m_items[i].selected ? 86.0f : 66.0f;

        drawQuad2D(bx, by, BTN_W / 2, BTN_H, 0.0f, srcY, BTN_W / 2, srcY + BTN_H);
        drawQuad2D(bx + BTN_W / 2, by, BTN_W / 2, BTN_H, 200.0f - BTN_W / 2, srcY,
                   200.0f, srcY + BTN_H);
      }
    }

    {
      const float BTN_H = 20.0f;
      const float BTN_GAP = 4.0f;
      const float startY = 100.0f;

      for (int i = 0; i < kMenuItemCount; ++i) {
        float by = startY + i * (BTN_H + BTN_GAP);
        float textY = by + (BTN_H - 8.0f) / 2.0f;
        uint32_t col = m_items[i].selected ? 0xFF00FFFF : 0xFFE0E0E0;
        
        m_font.drawShadowCentered(480.0f * 0.5f, textY, m_items[i].text, col, 1.0f);
      }
    }
  } else if (m_mode == MenuStateMode::WorldSelect) {
    renderWorldSelectUI();
  }

  {
    float xOff = 8.0f;
    if (m_texBtnCross.vramPtr) {
      m_texBtnCross.bind();
      drawQuad2D(xOff, 272.0f - 24.0f, 16.0f, 16.0f, 0, 0, m_texBtnCross.origWidth, m_texBtnCross.origHeight);
      xOff += 20.0f;
      m_font.drawShadow(xOff, 272.0f - 20.0f, "Select", 0xFFFFFFFF, 1.0f);
      xOff += m_font.getStringWidth("Select", 1.0f) + 16.0f;
    }
    
    // Only show Back button on World Select screen
    if (m_mode == MenuStateMode::WorldSelect && m_texBtnCircle.vramPtr) {
      m_texBtnCircle.bind();
      drawQuad2D(xOff, 272.0f - 24.0f, 16.0f, 16.0f, 0, 0, m_texBtnCircle.origWidth, m_texBtnCircle.origHeight);
      xOff += 20.0f;
      m_font.drawShadow(xOff, 272.0f - 20.0f, "Back", 0xFFFFFFFF, 1.0f);
    }
  }

  sceGuDisable(GU_BLEND);
  sceGuEnable(GU_DEPTH_TEST);
  sceGuEnable(GU_FOG);
  sceGuEnable(GU_CULL_FACE);
}

MainMenuAction ConsoleMainMenu::consumeAction() {
  MainMenuAction out = m_pendingAction;
  m_pendingAction = MainMenuAction::None;
  return out;
}

void ConsoleMainMenu::renderWorldSelectUI() {
  // Main panel - pushed down so the Minecraft logo is visible above
  int px = 18, py = 78, pw = 444, ph = 168;

  m_mainPanel.setSize(pw, ph);
  m_mainPanel.render(px, py);

  // Recess panels fill most of the main panel, with a thin border
  int recessY = py + 8;
  int recessH = ph - 16;
  int recessW = (pw - 30) / 2;
  
  int leftRecessX = px + 8;
  int rightRecessX = px + pw - recessW - 8;

  // Lerp panel color: active=darker (0xAA), inactive=lighter (0xFF)
  auto lerpByte = [](float a, float b, float t) -> uint8_t {
    return (uint8_t)(a + (b - a) * t);
  };
  uint8_t leftCh  = lerpByte(0xAA, 0xFF, m_tabTransition);   // 0xAA when Start focused, 0xFF when Join focused
  uint8_t rightCh = lerpByte(0xFF, 0xAA, m_tabTransition);   // 0xFF when Start focused, 0xAA when Join focused
  m_leftRecess.setColor(0xFF000000 | ((uint32_t)leftCh << 16) | ((uint32_t)leftCh << 8) | leftCh);
  m_rightSquare.setColor(0xFF000000 | ((uint32_t)rightCh << 16) | ((uint32_t)rightCh << 8) | rightCh);

  m_leftRecess.setSize(recessW, recessH);
  m_leftRecess.render(leftRecessX, recessY);
  
  m_rightSquare.setSize(recessW, recessH);
  m_rightSquare.render(rightRecessX, recessY);


  // Labels INSIDE the recess panels, at the top
  const uint32_t selectedTitleColor = 0xFF2C2C2C;
  const uint32_t unselectedTitleColor = 0xFF585858;
  const uint32_t startTitleColor = (m_worldSelectTab == WorldSelectTab::StartGame)
                                       ? selectedTitleColor
                                       : unselectedTitleColor;
  const uint32_t joinTitleColor = (m_worldSelectTab == WorldSelectTab::JoinGame)
                                      ? selectedTitleColor
                                      : unselectedTitleColor;

  m_font.drawStringCentered(leftRecessX + recessW / 2.0f, recessY + 6, "Start Game", startTitleColor, 1.0f);
  m_font.drawStringCentered(rightRecessX + recessW / 2.0f, recessY + 6, "Join Game", joinTitleColor, 1.0f);

  // List items start below the label text
  int startY = recessY + 18;
  int listX = leftRecessX + 4;
  int itemWidth = recessW - 8;
  int itemHeight = 22;

  // Spinner helper lambda - draws the 8-square ring spinner in a given rect
  const float sq      = 22.0f;
  const float step    = 28.0f;
  const int   count   = 8;
  const float cycle   = 80.0f / 30.0f;
  const float active  = 30.0f / 30.0f;
  const float fireGap = cycle / count;
  const float rx8[count] = { 0,    step, step*2, step*2, step*2, step,   0,      0    };
  const float ry8[count] = { 0,    0,    0,       step,   step*2, step*2, step*2, step };
  const float totalW = step * 2 + sq;
  const float totalH = step * 2 + sq;

  auto drawSpinner = [&](float cx, float cy) {
    const float sx = cx - totalW * 0.5f;
    const float sy = cy - totalH * 0.5f;
    const float phase = fmodf(m_joinLoadingTimer, cycle);
    for (int i = 0; i < count; ++i) {
      const float fireTime = (float)i * fireGap;
      float elapsed = phase - fireTime;
      if (elapsed < 0.0f) elapsed += cycle;
      if (elapsed >= active) continue;
      const float t = elapsed / active;
      const uint8_t ch = (uint8_t)(0xFF - (uint8_t)(t * 91.0f));
      const uint32_t color = 0xFF000000 | ((uint32_t)ch << 16) | ((uint32_t)ch << 8) | ch;
      drawRect2D(sx + rx8[i], sy + ry8[i], sq, sq, color);
    }
  };

  // LEFT panel: spinner while loading (< 0.8s), then world list
  {
    const float leftCX = leftRecessX + recessW * 0.5f;
    const float leftCY = recessY + recessH * 0.5f;
    if (m_joinLoadingTimer < 0.8f) {
      drawSpinner(leftCX, leftCY);
    } else {
      const char* worlds[kWorldItemCount] = {"Create New World", "Play Tutorial"};
      for (int i = 0; i < kWorldItemCount; ++i) {
        int y = startY + i * itemHeight;
        bool isSelected = (m_worldSelectTab == WorldSelectTab::StartGame) && (i == m_saveSelectedIndex);
        Texture& bgTex = isSelected ? m_texListOver : m_texListNorm;
        drawTex2D(bgTex, listX, y, itemWidth, itemHeight);
        uint32_t txtColor = isSelected ? 0xFF00FFFF : 0xFFFFFFFF;
        // Text shifted right to leave ~28px for a future icon on the left
        float textY = y + (itemHeight - 8.0f) * 0.5f;
        m_font.drawShadow(listX + 28.0f, textY, worlds[i], txtColor, 1.0f);
      }
    }
  }

  // RIGHT panel: spinner for 2s, then "No Games Found"
  {
    const float rightCX = rightRecessX + recessW * 0.5f;
    const float rightCY = recessY + recessH * 0.5f;
    if (m_joinLoadingTimer < 2.0f) {
      drawSpinner(rightCX, rightCY);
    } else {
      m_font.drawShadowCentered(rightCX, rightCY - 4.0f, "No Games Found", 0xFF404040, 1.0f);
    }
  }
}
