#pragma once

#include "../render/BitmapFont.h"
#include "../render/NinePatch.h"
#include "../render/Texture.h"

enum class MainMenuAction {
  None = 0,
  StartGame,
  ExitGame,
};

enum class MenuStateMode {
  MainButtons,
  WorldSelect
};

enum class WorldSelectTab {
  StartGame = 0,
  JoinGame
};

class ConsoleMainMenu {
public:
  static constexpr int kMenuItemCount = 6;
  static constexpr int kWorldItemCount = 2;

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
  int m_saveSelectedIndex;
  float m_menuRenderTimer;
  float m_splashScaleTimer;
  float m_scrollOffset;
  float m_joinLoadingTimer;
  float m_tabTransition;  // 0 = Start Game focused, 1 = Join Game focused
  MainMenuAction m_pendingAction;
  MenuStateMode m_mode;
  WorldSelectTab m_worldSelectTab;

  Texture m_texGui;
  Texture m_texLogo;
  Texture m_texPanoL;
  Texture m_texPanoR;
  Texture m_texBtnCross;
  Texture m_texBtnCircle;

  Texture m_texFullPanel;
  NinePatch m_mainPanel;
  
  Texture m_texRecessPanel;
  NinePatch m_leftRecess;
  NinePatch m_rightSquare;

  Texture m_texListNorm;
  Texture m_texListOver;

  BitmapFont m_font;
  MenuItem m_items[kMenuItemCount];

  const char* m_currentSplash;
  char m_splashStorage[160];
  bool m_resourcesLoaded;

  void moveSelection(int dir);
  void loadSplash();
  void renderWorldSelectUI();
};
