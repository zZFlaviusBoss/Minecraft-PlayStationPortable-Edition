#pragma once

#include "../render/BitmapFont.h"
#include "../render/NinePatch.h"
#include "../render/Texture.h"

class WorldLoadingScreen {
public:
  WorldLoadingScreen();
  ~WorldLoadingScreen();

  bool init();
  void update(float dt);
  void render(float progress, const char* status);
  void setTip(const char* tip);
  void setScrollOffset(float offset);
  void releaseResources();

private:
  Texture m_texPano;
  Texture m_texLogo;
  float m_scrollOffset;

  // Tip panel
  Texture m_texTipPanel;
  NinePatch m_tipPanel;

  BitmapFont m_font;
  char m_currentTip[256];

  // Tip rotation
  float m_tipTimer;
  int m_tipCount;
  int m_tipLineStarts[256];
  char* m_tipsBuf;

  bool m_resourcesLoaded;

  void loadTips();
  void pickRandomTip();
  void drawOutlinedCentered(float cx, float cy, const char* text, uint32_t color, float scale);
};
