#pragma once

#include "../render/BitmapFont.h"
#include "../render/Texture.h"

enum class MainMenuAction {
  None = 0,
  StartGame,
  ExitGame,
};

class ConsoleMainMenu {
public:
  static constexpr int kMenuItemCount = 6;

  ConsoleMainMenu();
  ~ConsoleMainMenu();

  bool init();
  void update(float dt);
  void render(int screenWidth, int screenHeight);
  void releaseResources();

  MainMenuAction consumeAction();

private:
  struct MenuItem {
    const char* text;
    bool selected;
  };

  int m_selectedIndex;
  float m_menuRenderTimer;
  float m_splashScaleTimer;
  float m_scrollOffset;
  MainMenuAction m_pendingAction;

  Texture m_texGui;
  Texture m_texLogo;
  Texture m_texPanoL;
  Texture m_texPanoR;
  Texture m_texBtnCross;
  Texture m_texBtnCircle;
  BitmapFont m_font;
  MenuItem m_items[kMenuItemCount];

  const char* m_currentSplash;
  char m_splashStorage[160];
  bool m_resourcesLoaded;

  void moveSelection(int dir);
  void loadSplash();
};
