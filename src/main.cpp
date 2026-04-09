// MinecraftPSP - main.cpp
// PSP Entry point, basic game loop

#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psputility.h>
#include <psputility_osk.h>

#include "input/PSPInput.h"
#include "render/BlockHighlight.h"
#include "render/ChunkRenderer.h"
#include "render/CloudRenderer.h"
#include "render/PSPRenderer.h"
#include "render/SkyRenderer.h"
#include "render/TextureAtlas.h"
#include "ui/ConsoleMainMenu.h"
#include "ui/WorldLoadingScreen.h"
#include "world/AABB.h"
#include "world/Blocks.h"
#include "world/Level.h"
#include "world/Mth.h"
#include "world/Random.h"
#include "world/Raycast.h"
#include <ctype.h>
#include <math.h>
#include <string.h>

// PSP module metadata
PSP_MODULE_INFO("MinecraftPSP", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024); // Use all available RAM minus 1MB for the kernel

// Exit callback (HOME button)
int exit_callback(int arg1, int arg2, void *common) {
  sceKernelExitGame();
  return 0;
}

int callback_thread(SceSize args, void *argp) {
  int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
  sceKernelRegisterExitCallback(cbid);
  sceKernelSleepThreadCB();
  return 0;
}

void setup_callbacks() {
  int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11,
                                   0xFA0, PSP_THREAD_ATTR_USER, NULL);
  if (thid >= 0)
    sceKernelStartThread(thid, 0, NULL);
}

#include "world/Player.h"

#include "render/BitmapFont.h"

// Global state
static Player *g_player = nullptr;
static Level *g_level = nullptr;
static SkyRenderer *g_skyRenderer = nullptr;
static CloudRenderer *g_cloudRenderer = nullptr;
static ChunkRenderer *g_chunkRenderer = nullptr;
static TextureAtlas *g_atlas = nullptr;
static ConsoleMainMenu g_consoleMenu;
static WorldLoadingScreen g_loadingScreen;
static bool g_gameInitialized = false;

// HUD UI state
static BitmapFont g_font;
static Texture g_texBtnSquare;
static Texture g_texBtnTriangle;
static Texture g_texBtnR;
static Texture g_texBtnL;
static SimpleTexture g_guiInvCreativeTex;
static SimpleTexture g_guiCursorTex;
static SimpleTexture g_guiSliderTex;
static SimpleTexture g_guiCellTex;
static char g_statusText[96] = {0};
static float g_statusTimer = 0.0f;
static uint32_t g_statusColor = 0xFFFFFFFF;


enum class AppMode {
  MainMenu = 0,
  Gameplay,
};

static AppMode g_appMode = AppMode::MainMenu;
static const char *kTexInvCreativePath = "res/gui/inventory_creative.png";
static const char *kTexCursorPath = "res/gui/cursor.png";
static const char *kTexSliderPath = "res/gui/slider.png";
static const char *kTexCellPath = "res/gui/cell.png";
// Inventory layout defaults from archive.
static const float kInvCellStep = 21.300f;
static const float kInvStretchX = 0.750f;
static const float kInvCompressY = 1.100f;
static const float kInvOffsetX = -10.084f;
static const float kInvOffsetY = 5.204f;
static const float kInvHotbarStepX = 21.300f;
static const float kInvHotbarStretchX = 0.800f;
static const float kInvHotbarOffsetX = 0.0f;
static const float kInvHotbarOffsetY = -13.000f;
static const float kInvDeleteOffsetX = -37.874f;
static const float kInvDeleteOffsetY = -0.484f;
static const float kInvTitleOffsetX = -17.314f;
static const float kInvTitleOffsetY = 1.532f;

static void setStatusMessage(const char *msg, float seconds, uint32_t color = 0xFFFFFFFF) {
  if (!msg) msg = "";
  snprintf(g_statusText, sizeof(g_statusText), "%s", msg);
  g_statusTimer = seconds;
  g_statusColor = color;
}

static void asciiToUtf16(const char *src, unsigned short *dst, int maxChars) {
  if (!dst || maxChars <= 0) return;
  int i = 0;
  if (src) {
    while (src[i] && i < maxChars - 1) {
      dst[i] = (unsigned short)(unsigned char)src[i];
      ++i;
    }
  }
  dst[i] = 0;
}

static void utf16ToAscii(const unsigned short *src, char *dst, int maxChars) {
  if (!dst || maxChars <= 0) return;
  int i = 0;
  if (src) {
    while (src[i] && i < maxChars - 1) {
      unsigned short c = src[i];
      dst[i] = (c < 0x80) ? (char)c : '?';
      ++i;
    }
  }
  dst[i] = '\0';
}

static bool showCommandKeyboard(char *out, int outSize) {
  if (!out || outSize <= 1) return false;
  out[0] = '\0';

  static unsigned short inText[128];
  static unsigned short outText[128];
  static unsigned short descText[32];
  static unsigned short initText[2];
  asciiToUtf16("Command (/time set day)", descText, (int)(sizeof(descText) / sizeof(descText[0])));
  asciiToUtf16("", initText, (int)(sizeof(initText) / sizeof(initText[0])));
  memset(inText, 0, sizeof(inText));
  memset(outText, 0, sizeof(outText));

  SceUtilityOskData oskData;
  memset(&oskData, 0, sizeof(oskData));
  oskData.language = PSP_UTILITY_OSK_LANGUAGE_ENGLISH;
  oskData.lines = 1;
  oskData.unk_24 = 1;
  oskData.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
  oskData.desc = descText;
  oskData.intext = initText;
  oskData.outtextlength = (int)(sizeof(outText) / sizeof(outText[0]));
  oskData.outtextlimit = oskData.outtextlength - 1;
  oskData.outtext = outText;

  SceUtilityOskParams params;
  memset(&params, 0, sizeof(params));
  params.base.size = sizeof(params);
  sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &params.base.language);
  sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &params.base.buttonSwap);
  params.base.graphicsThread = 17;
  params.base.accessThread = 19;
  params.base.fontThread = 18;
  params.base.soundThread = 16;
  params.datacount = 1;
  params.data = &oskData;

  int ret = sceUtilityOskInitStart(&params);
  if (ret < 0) return false;
  static unsigned int __attribute__((aligned(16))) oskList[262144];

  while (true) {
    sceGuStart(GU_DIRECT, oskList);
    sceGuClearColor(0xFF000000);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);

    sceUtilityOskUpdate(1);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();

    int status = sceUtilityOskGetStatus();
    if (status == PSP_UTILITY_DIALOG_VISIBLE) continue;
    if (status == PSP_UTILITY_DIALOG_QUIT) {
      sceUtilityOskShutdownStart();
    } else if (status == PSP_UTILITY_DIALOG_FINISHED || status == PSP_UTILITY_DIALOG_NONE) {
      break;
    }
  }

  if (oskData.result != PSP_UTILITY_OSK_RESULT_CHANGED) return false;
  utf16ToAscii(outText, out, outSize);
  return out[0] != '\0';
}

static void executeConsoleCommand(const char *rawCmd) {
  if (!rawCmd || !rawCmd[0]) return;

  char cmd[128];
  snprintf(cmd, sizeof(cmd), "%s", rawCmd);
  for (int i = 0; cmd[i]; ++i) cmd[i] = (char)tolower((unsigned char)cmd[i]);

  if (cmd[0] != '/') {
    setStatusMessage("Commands must start with /", 2.5f, 0xFF60A0FF);
    return;
  }
  if (strcmp(cmd, "/time set day") == 0) {
    if (g_level) g_level->setTime((g_level->getDay() * TICKS_PER_DAY) + 1000LL);
    setStatusMessage("Set time: day", 2.0f, 0xFF80FF80);
    return;
  }
  if (strcmp(cmd, "/time set night") == 0) {
    if (g_level) g_level->setTime((g_level->getDay() * TICKS_PER_DAY) + 13000LL);
    setStatusMessage("Set time: night", 2.0f, 0xFF80FF80);
    return;
  }

  setStatusMessage("Unknown command", 2.0f, 0xFF6060FF);
}

static bool app_init() {
  // Overclock PSP to max for performance.
  scePowerSetClockFrequency(333, 333, 166);

  // Core systems needed by both menu and gameplay.
  Blocks_Init();
  Mth::init();

  if (!PSPRenderer_Init())
    return false;
  if (!g_consoleMenu.init())
    return false;

  return true;
}

// Gameplay initialization (called when player starts game from main menu)
static bool game_init() {
  // Load terrain.png from MS0:/PSP/GAME/MinecraftPSP/res/
  g_atlas = new TextureAtlas();
  if (!g_atlas->load("res/terrain.png"))
    return false;

  g_font.load("res/font/Default.png");
  g_texBtnSquare.load("res/gui/buttons/psp_square.png");
  g_texBtnTriangle.load("res/gui/buttons/psp_triangle.png");
  g_texBtnR.load("res/gui/buttons/psp_r.png");
  g_texBtnL.load("res/gui/buttons/psp_l.png");
  g_guiInvCreativeTex.load(kTexInvCreativePath);
  g_guiCursorTex.load(kTexCursorPath);
  g_guiSliderTex.load(kTexSliderPath);
  g_guiCellTex.load(kTexCellPath);

  g_level = new Level();
  g_skyRenderer = new SkyRenderer(g_level);
  g_cloudRenderer = new CloudRenderer(g_level);

  // Init chunk renderer
  g_chunkRenderer = new ChunkRenderer(g_atlas);
  g_chunkRenderer->setLevel(g_level);

  // Generate a test world
  Random rng(12345LL);
  
  if (g_loadingScreen.init()) {
    g_loadingScreen.setScrollOffset(g_consoleMenu.getScrollOffset());
    auto cb = [](float progress, const char* status, void* userdata) {
      WorldLoadingScreen* ls = (WorldLoadingScreen*)userdata;

      // Set up 2D ortho projection for the loading screen
      sceGumMatrixMode(GU_PROJECTION);
      sceGumLoadIdentity();
      sceGumOrtho(0.0f, 480.0f, 272.0f, 0.0f, -1.0f, 1.0f);
      sceGumMatrixMode(GU_VIEW);
      sceGumLoadIdentity();
      sceGumMatrixMode(GU_MODEL);
      sceGumLoadIdentity();

      ls->render(progress, status);
      PSPRenderer_EndFrame();
      PSPRenderer_BeginFrame(0xFF000000, 1.0f, 16.0f, 0xFF000000, 70.0f);
    };

    PSPRenderer_BeginFrame(0xFF000000, 1.0f, 16.0f, 0xFF000000, 70.0f);
    g_level->generate(&rng, cb, &g_loadingScreen);
    PSPRenderer_EndFrame();

    g_loadingScreen.releaseResources();
  } else {
    g_level->generate(&rng);
  }

  // Player start position
  g_player = new Player(g_level);
  g_player->spawn(8.0f, 65.0f, 8.0f);

  return true;
}

// Game loop update
static void game_update(float dt) {
  PSPInput_Update();
  if (PSPInput_IsHeld(PSP_CTRL_SELECT) && PSPInput_JustPressed(PSP_CTRL_START)) {
    char cmd[128];
    if (showCommandKeyboard(cmd, sizeof(cmd))) executeConsoleCommand(cmd);
    return;
  }

  if (g_statusTimer > 0.0f) {
    g_statusTimer -= dt;
    if (g_statusTimer < 0.0f) g_statusTimer = 0.0f;
  }

  if (g_level) {
    static float s_levelTickAccum = 0.0f;
    const float tickStep = 1.0f / 20.0f;
    s_levelTickAccum += dt;
    if (s_levelTickAccum > 0.25f) s_levelTickAccum = 0.25f;
    while (s_levelTickAccum >= tickStep) {
      if (g_player) {
        g_level->setSimulationFocus((int)floorf(g_player->getX()),
                                    (int)floorf(g_player->getY()),
                                    (int)floorf(g_player->getZ()),
                                    24);
      }
      g_level->tick();
      s_levelTickAccum -= tickStep;
    }
  }

  // Animation for WATER blocks
  static float textureTimer = 0.0f;
  textureTimer += dt;
  if (textureTimer >= 0.125f) { 
    textureTimer = 0.0f;
    static int textureIdx = 0;
    textureIdx = (textureIdx + 1) % 4;

    if (g_atlas) {
      g_atlas->animateWater(textureIdx);
    }
  }


  if (g_player) {
    g_player->update(dt);
  }
}

struct VertTCO {
  float u, v;
  unsigned int c;
  float x, y, z;
};

struct HudColVert {
  uint32_t color;
  float x, y, z;
};

struct HudTexVert {
  float u, v;
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
                 GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                 6, nullptr, v);
}

static void drawSkewQuad2D(float x0, float y0, float x1, float y1,
                           float x2, float y2, float x3, float y3,
                           float u0, float v0, float u1, float v1,
                           uint32_t color = 0xFFFFFFFF) {
  VertTCO *v = (VertTCO *)sceGuGetMemory(6 * sizeof(VertTCO));
  v[0] = {u0, v0, color, x0, y0, 0.0f};
  v[1] = {u1, v0, color, x1, y1, 0.0f};
  v[2] = {u0, v1, color, x3, y3, 0.0f};
  v[3] = {u1, v0, color, x1, y1, 0.0f};
  v[4] = {u1, v1, color, x2, y2, 0.0f};
  v[5] = {u0, v1, color, x3, y3, 0.0f};
  sceGuDrawArray(GU_TRIANGLES,
                 GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                 6, nullptr, v);
}

static inline void hudDrawTexture(SimpleTexture &tex, float x, float y, float w, float h) {
  if (!tex.data || tex.width == 0 || tex.height == 0) return;
  tex.bind();
  sceGuColor(0xFFFFFFFF);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  sceGuTexFilter(GU_NEAREST, GU_NEAREST);
  HudTexVert *v = (HudTexVert *)sceGuGetMemory(2 * sizeof(HudTexVert));
  v[0].u = 0.5f;             v[0].v = 0.5f;              v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
  v[1].u = tex.width - 0.5f; v[1].v = tex.height - 0.5f; v[1].x = x + w; v[1].y = y + h; v[1].z = 0.0f;
  sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
}

static inline void hudDrawRect(float x, float y, float w, float h, uint32_t abgr) {
  sceGuDisable(GU_TEXTURE_2D);
  HudColVert *v = (HudColVert *)sceGuGetMemory(2 * sizeof(HudColVert));
  v[0].color = abgr; v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
  v[1].color = abgr; v[1].x = x + w; v[1].y = y + h; v[1].z = 0.0f;
  sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
  sceGuEnable(GU_TEXTURE_2D);
}

static inline void hudDrawTile(TextureAtlas *atlas, int tx, int ty, float x, float y, float size) {
  if (!atlas) return;
  atlas->bind();
  sceGuColor(0xFFFFFFFF);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  sceGuTexFilter(GU_NEAREST, GU_NEAREST);
  HudTexVert *v = (HudTexVert *)sceGuGetMemory(2 * sizeof(HudTexVert));
  float u0 = (float)(tx * 16) + 0.5f;
  float v0 = (float)(ty * 16) + 0.5f;
  float us = 15.0f;
  float vs = 15.0f;
  v[0].u = u0;      v[0].v = v0;      v[0].x = x;        v[0].y = y;        v[0].z = 0.0f;
  v[1].u = u0 + us; v[1].v = v0 + vs; v[1].x = x + size; v[1].y = y + size; v[1].z = 0.0f;
  sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
}

static inline void hudDrawBlockIso(TextureAtlas *atlas, uint8_t id, float x, float y, float size) {
  if (!atlas) return;
  atlas->bind();
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  sceGuTexFilter(GU_NEAREST, GU_NEAREST);

  const BlockUV &uv = g_blockUV[id];
  const float tile = 16.0f;
  const float eps = 0.5f;

  float topU0 = uv.top_x * tile + eps;
  float topV0 = uv.top_y * tile + eps;
  float topU1 = (uv.top_x + 1) * tile - eps;
  float topV1 = (uv.top_y + 1) * tile - eps;

  float sideU0 = uv.side_x * tile + eps;
  float sideV0 = uv.side_y * tile + eps;
  float sideU1 = (uv.side_x + 1) * tile - eps;
  float sideV1 = (uv.side_y + 1) * tile - eps;
  bool isSlab = (id == BLOCK_STONE_SLAB || id == BLOCK_WOOD_SLAB || id == BLOCK_COBBLE_SLAB ||
                 id == BLOCK_SANDSTONE_SLAB || id == BLOCK_BRICK_SLAB || id == BLOCK_STONE_BRICK_SLAB ||
                 id == BLOCK_STONE_SLAB_TOP || id == BLOCK_WOOD_SLAB_TOP || id == BLOCK_COBBLE_SLAB_TOP ||
                 id == BLOCK_SANDSTONE_SLAB_TOP || id == BLOCK_BRICK_SLAB_TOP || id == BLOCK_STONE_BRICK_SLAB_TOP);
  if (isSlab) {
    sideV1 = sideV0 + (sideV1 - sideV0) * 0.5f;
  }

  float cx = x + size * 0.5f;
  // Inventory cubes looked vertically squashed after switching from 2D sprites.
  // Use a taller body + slightly larger top to better match MCPE icon proportions.
  float topHalfH = size * 0.22f;
  float bodyH = isSlab ? (size * 0.32f) : (size * 0.64f);

  float tx0 = cx;
  float ty0 = y;
  float tx1 = x + size;
  float ty1 = y + topHalfH;
  float tx2 = cx;
  float ty2 = y + topHalfH * 2.0f;
  float tx3 = x;
  float ty3 = y + topHalfH;

  drawSkewQuad2D(tx0, ty0, tx1, ty1, tx2, ty2, tx3, ty3, topU0, topV0, topU1, topV1, 0xFFFFFFFF);

  drawSkewQuad2D(tx3, ty3, tx2, ty2, tx2, ty2 + bodyH, tx3, ty3 + bodyH,
                 sideU0, sideV0, sideU1, sideV1, 0xFFC0C0C0);
  drawSkewQuad2D(tx2, ty2, tx1, ty1, tx1, ty1 + bodyH, tx2, ty2 + bodyH,
                 sideU0, sideV0, sideU1, sideV1, 0xFF9A9A9A);
}

static inline bool isCrossModelBlock(uint8_t id) {
  return id == BLOCK_TALLGRASS || id == BLOCK_FLOWER || id == BLOCK_ROSE ||
         id == BLOCK_SAPLING || id == BLOCK_REEDS || id == BLOCK_MUSHROOM_BROWN ||
         id == BLOCK_MUSHROOM_RED || id == BLOCK_CROPS || id == BLOCK_FIRE;
}

static inline void hudDrawInventoryBlockIcon(TextureAtlas *atlas, uint8_t id, float x, float y, float size) {
  if (isCrossModelBlock(id)) {
    const BlockUV &uv = g_blockUV[id];
    hudDrawTile(atlas, uv.top_x, uv.top_y, x, y, size);
    return;
  }
  hudDrawBlockIso(atlas, id, x, y, size);
}

static const char* getBlockDisplayName(uint8_t id) {
  switch (id) {
    case BLOCK_STONE: return "Stone";
    case BLOCK_GRASS: return "Grass";
    case BLOCK_DIRT: return "Dirt";
    case BLOCK_COBBLESTONE: return "Cobblestone";
    case BLOCK_WOOD_PLANK: return "Wood Planks";
    case BLOCK_SAND: return "Sand";
    case BLOCK_GRAVEL: return "Gravel";
    case BLOCK_LOG: return "Log";
    case BLOCK_LEAVES: return "Leaves";
    case BLOCK_GLASS: return "Glass";
    case BLOCK_SANDSTONE: return "Sandstone";
    case BLOCK_STONE_BRICKS: return "Stone Bricks";
    case BLOCK_STONE_SLAB: return "Stone Slab";
    case BLOCK_STONE_SLAB_TOP: return "Stone Slab";
    case BLOCK_WOOD_SLAB: return "Wooden Slab";
    case BLOCK_WOOD_SLAB_TOP: return "Wooden Slab";
    case BLOCK_COBBLE_SLAB: return "Cobblestone Slab";
    case BLOCK_COBBLE_SLAB_TOP: return "Cobblestone Slab";
    case BLOCK_SANDSTONE_SLAB: return "Sandstone Slab";
    case BLOCK_SANDSTONE_SLAB_TOP: return "Sandstone Slab";
    case BLOCK_BRICK_SLAB: return "Brick Slab";
    case BLOCK_BRICK_SLAB_TOP: return "Brick Slab";
    case BLOCK_STONE_BRICK_SLAB: return "Stone Brick Slab";
    case BLOCK_STONE_BRICK_SLAB_TOP: return "Stone Brick Slab";
    case BLOCK_STONE_STAIR: return "Stone Stairs";
    case BLOCK_STONE_STAIR_EAST: return "Stone Stairs";
    case BLOCK_STONE_STAIR_SOUTH: return "Stone Stairs";
    case BLOCK_STONE_STAIR_WEST: return "Stone Stairs";
    case BLOCK_STONE_STAIR_TOP: return "Stone Stairs";
    case BLOCK_STONE_STAIR_TOP_EAST: return "Stone Stairs";
    case BLOCK_STONE_STAIR_TOP_SOUTH: return "Stone Stairs";
    case BLOCK_STONE_STAIR_TOP_WEST: return "Stone Stairs";
    case BLOCK_WOOD_STAIR: return "Wooden Stairs";
    case BLOCK_WOOD_STAIR_EAST: return "Wooden Stairs";
    case BLOCK_WOOD_STAIR_SOUTH: return "Wooden Stairs";
    case BLOCK_WOOD_STAIR_WEST: return "Wooden Stairs";
    case BLOCK_WOOD_STAIR_TOP: return "Wooden Stairs";
    case BLOCK_WOOD_STAIR_TOP_EAST: return "Wooden Stairs";
    case BLOCK_WOOD_STAIR_TOP_SOUTH: return "Wooden Stairs";
    case BLOCK_WOOD_STAIR_TOP_WEST: return "Wooden Stairs";
    case BLOCK_COBBLE_STAIR: return "Cobblestone Stairs";
    case BLOCK_COBBLE_STAIR_EAST: return "Cobblestone Stairs";
    case BLOCK_COBBLE_STAIR_SOUTH: return "Cobblestone Stairs";
    case BLOCK_COBBLE_STAIR_WEST: return "Cobblestone Stairs";
    case BLOCK_COBBLE_STAIR_TOP: return "Cobblestone Stairs";
    case BLOCK_COBBLE_STAIR_TOP_EAST: return "Cobblestone Stairs";
    case BLOCK_COBBLE_STAIR_TOP_SOUTH: return "Cobblestone Stairs";
    case BLOCK_COBBLE_STAIR_TOP_WEST: return "Cobblestone Stairs";
    case BLOCK_SANDSTONE_STAIR: return "Sandstone Stairs";
    case BLOCK_SANDSTONE_STAIR_EAST: return "Sandstone Stairs";
    case BLOCK_SANDSTONE_STAIR_SOUTH: return "Sandstone Stairs";
    case BLOCK_SANDSTONE_STAIR_WEST: return "Sandstone Stairs";
    case BLOCK_SANDSTONE_STAIR_TOP: return "Sandstone Stairs";
    case BLOCK_SANDSTONE_STAIR_TOP_EAST: return "Sandstone Stairs";
    case BLOCK_SANDSTONE_STAIR_TOP_SOUTH: return "Sandstone Stairs";
    case BLOCK_SANDSTONE_STAIR_TOP_WEST: return "Sandstone Stairs";
    case BLOCK_BRICK_STAIR: return "Brick Stairs";
    case BLOCK_BRICK_STAIR_EAST: return "Brick Stairs";
    case BLOCK_BRICK_STAIR_SOUTH: return "Brick Stairs";
    case BLOCK_BRICK_STAIR_WEST: return "Brick Stairs";
    case BLOCK_BRICK_STAIR_TOP: return "Brick Stairs";
    case BLOCK_BRICK_STAIR_TOP_EAST: return "Brick Stairs";
    case BLOCK_BRICK_STAIR_TOP_SOUTH: return "Brick Stairs";
    case BLOCK_BRICK_STAIR_TOP_WEST: return "Brick Stairs";
    case BLOCK_STONE_BRICK_STAIR: return "Stone Brick Stairs";
    case BLOCK_STONE_BRICK_STAIR_EAST: return "Stone Brick Stairs";
    case BLOCK_STONE_BRICK_STAIR_SOUTH: return "Stone Brick Stairs";
    case BLOCK_STONE_BRICK_STAIR_WEST: return "Stone Brick Stairs";
    case BLOCK_STONE_BRICK_STAIR_TOP: return "Stone Brick Stairs";
    case BLOCK_STONE_BRICK_STAIR_TOP_EAST: return "Stone Brick Stairs";
    case BLOCK_STONE_BRICK_STAIR_TOP_SOUTH: return "Stone Brick Stairs";
    case BLOCK_STONE_BRICK_STAIR_TOP_WEST: return "Stone Brick Stairs";
    case BLOCK_WOOL: return "Wool";
    case BLOCK_WOOL_ORANGE: return "Orange Wool";
    case BLOCK_WOOL_MAGENTA: return "Magenta Wool";
    case BLOCK_WOOL_LIGHT_BLUE: return "Light Blue Wool";
    case BLOCK_WOOL_YELLOW: return "Yellow Wool";
    case BLOCK_WOOL_LIME: return "Lime Wool";
    case BLOCK_WOOL_PINK: return "Pink Wool";
    case BLOCK_WOOL_GRAY: return "Gray Wool";
    case BLOCK_WOOL_LIGHT_GRAY: return "Light Gray Wool";
    case BLOCK_WOOL_CYAN: return "Cyan Wool";
    case BLOCK_WOOL_PURPLE: return "Purple Wool";
    case BLOCK_WOOL_BLUE: return "Blue Wool";
    case BLOCK_WOOL_BROWN: return "Brown Wool";
    case BLOCK_WOOL_GREEN: return "Green Wool";
    case BLOCK_WOOL_RED: return "Red Wool";
    case BLOCK_WOOL_BLACK: return "Black Wool";
    case BLOCK_BRICK: return "Bricks";
    case BLOCK_BOOKSHELF: return "Bookshelf";
    case BLOCK_MOSSY_COBBLE: return "Moss Stone";
    case BLOCK_OBSIDIAN: return "Obsidian";
    case BLOCK_NETHERRACK: return "Netherrack";
    case BLOCK_SOULSAND: return "Soul Sand";
    case BLOCK_GLOWSTONE: return "Glowstone";
    case BLOCK_COAL_ORE: return "Coal Ore";
    case BLOCK_IRON_ORE: return "Iron Ore";
    case BLOCK_GOLD_ORE: return "Gold Ore";
    case BLOCK_DIAMOND_ORE: return "Diamond Ore";
    case BLOCK_REDSTONE_ORE: return "Redstone Ore";
    case BLOCK_LAPIS_ORE: return "Lapis Ore";
    case BLOCK_EMERALD_ORE: return "Emerald Ore";
    case BLOCK_SAPLING: return "Sapling";
    case BLOCK_TALLGRASS: return "Tallgrass";
    case BLOCK_FLOWER: return "Dandelion";
    case BLOCK_ROSE: return "Rose";
    case BLOCK_WATER_STILL: return "Water";
    case BLOCK_LAVA_STILL: return "Lava";
    case BLOCK_IRON_BLOCK: return "Iron Block";
    case BLOCK_GOLD_BLOCK: return "Gold Block";
    case BLOCK_DIAMOND_BLOCK: return "Diamond Block";
    case BLOCK_TNT: return "TNT";
    case BLOCK_CHEST: return "Chest";
    case BLOCK_CRAFTING_TABLE: return "Crafting Table";
    case BLOCK_FURNACE: return "Furnace";
    case BLOCK_PUMPKIN: return "Pumpkin";
    case BLOCK_CACTUS: return "Cactus";
    case BLOCK_REEDS: return "Sugar Cane";
    case BLOCK_SNOW_BLOCK: return "Snow Block";
    case BLOCK_ICE: return "Ice";
    case BLOCK_CLAY: return "Clay";
    default: return "Block";
  }
}

static void drawHUD() {
  sceGuDisable(GU_DEPTH_TEST);
  sceGuDisable(GU_FOG);
  sceGuDisable(GU_LIGHTING);
  sceGuDisable(GU_CULL_FACE);
  sceGuEnable(GU_BLEND);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuEnable(GU_TEXTURE_2D);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);

  sceGumMatrixMode(GU_PROJECTION);
  sceGumLoadIdentity();
  sceGumOrtho(0.0f, 480.0f, 272.0f, 0.0f, -1.0f, 1.0f);
  sceGumMatrixMode(GU_VIEW);
  sceGumLoadIdentity();
  sceGumMatrixMode(GU_MODEL);
  sceGumLoadIdentity();

  float xOffset = 10.0f;
  float yBase = 250.0f; // Bottom 
  float icoW = 16.0f; // Button pixel size

  // DEBUG: show player Y coordinate top-left
  if (g_player) {
    char yBuf[32];
    int playerY = (int)floorf(g_player->getY());
    snprintf(yBuf, sizeof(yBuf), "Y: %d", playerY);
    g_font.drawString(4.0f, 4.0f, yBuf, 0xFFFFFF00, 1.0f);  // yellow
  }
  if (g_statusTimer > 0.0f) {
    float msgW = g_font.getStringWidth(g_statusText, 1.0f);
    float msgX = (480.0f - msgW) * 0.5f;
    hudDrawRect(msgX - 4.0f, 16.0f, msgW + 8.0f, 12.0f, 0xA0000000);
    g_font.drawString(msgX, 18.0f, g_statusText, g_statusColor, 1.0f);
  }

  if (g_player && g_player->isInventoryOpen()) {
    const CreativeInventory &inv = g_player->getCreativeInventory();

    const int invCount = inv.visibleItemCount();
    const int itemsPerPage = 50;
    const float cellStepX = kInvCellStep;
    const float cellStepY = kInvCellStep;
    const float cellSize = kInvCellStep - 1.0f;
    const float cellX0 = 125.0f + (cellStepX * 0.5f) + kInvOffsetX;
    const float cellY0 = 93.0f - (cellStepY * 0.25f) + kInvOffsetY;
    const float hotbarY = 209.0f + kInvOffsetY + kInvHotbarOffsetY;
    const float sliderX = 372.0f + kInvOffsetX;
    const float sliderW = 16.0f;
    const float iconSize = 16.0f;
    const float iconPad = (cellSize - iconSize) * 0.5f;
    auto gridX = [&](int col) { return cellX0 + col * cellStepX + col * kInvStretchX; };
    auto gridY = [&](int row) { return cellY0 + row * cellStepY - row * kInvCompressY; };
    auto hotbarX = [&](int col) { return (cellX0 + kInvHotbarOffsetX) + col * kInvHotbarStepX + col * kInvHotbarStretchX; };

    if (g_guiInvCreativeTex.data) hudDrawTexture(g_guiInvCreativeTex, 0.0f, 0.0f, 480.0f, 272.0f);

    int base = inv.creativePage() * itemsPerPage;
    for (int r = 0; r < 5; ++r) {
      for (int c = 0; c < 10; ++c) {
        float sx = gridX(c);
        float sy = gridY(r);
        if (g_guiCellTex.data) hudDrawTexture(g_guiCellTex, sx, sy, cellSize, cellSize);

        int idx = base + r * 10 + c;
        if (idx >= invCount) continue;
        uint8_t id = inv.visibleItemAt(idx);
        hudDrawInventoryBlockIcon(g_atlas, id, sx + iconPad, sy + iconPad, iconSize);
      }
    }
    for (int i = 0; i < 9; ++i) {
      float sx = hotbarX(i);
      if (g_guiCellTex.data) hudDrawTexture(g_guiCellTex, sx, hotbarY, cellSize, cellSize);
      uint8_t id = inv.hotbarAt(i);
      if (id != BLOCK_AIR) {
        hudDrawInventoryBlockIcon(g_atlas, id, sx + iconPad, hotbarY + iconPad, iconSize);
      }
      if (i == inv.hotbarSel()) {
        hudDrawRect(sx - 1.0f, hotbarY - 1.0f, cellSize + 2.0f, cellSize + 2.0f, 0xD0FFFFFF);
      }
    }

    if (g_guiSliderTex.data) hudDrawTexture(g_guiSliderTex, sliderX, cellY0, sliderW, 5.0f * cellStepY + (cellSize - cellStepY));
    const float deleteX = sliderX + kInvDeleteOffsetX;
    const float deleteY = hotbarY + kInvDeleteOffsetY;

    float cursorX = gridX(inv.cursorX());
    float cursorY = (inv.cursorY() < 5) ? (gridY(inv.cursorY())) : hotbarY;
    if (inv.cursorY() == 5 && inv.cursorX() == 10) {
      cursorX = deleteX;
      cursorY = deleteY;
    }
    if (inv.cursorHasItem()) {
      hudDrawInventoryBlockIcon(g_atlas, inv.cursorItem(), cursorX + 3.0f, cursorY + 3.0f, iconSize * 0.8f);
    } else if (g_guiCursorTex.data) {
      hudDrawTexture(g_guiCursorTex, cursorX - 1.0f, cursorY - 1.0f, cellSize + 2.0f, cellSize + 2.0f);
    }

    int maxPage = (invCount / itemsPerPage);
    if (maxPage > 0) {
      float t = (float)inv.creativePage() / (float)maxPage;
      hudDrawRect(sliderX + 4.0f, 102.0f + t * 84.0f, 8.0f, 16.0f, 0xD0FFFFFF);
    }
    const float titleAreaX = 206.0f + kInvOffsetX;
    const float titleY = 73.0f + kInvOffsetY + kInvTitleOffsetY;
    const float titleAreaW = 120.0f;
    const float titleScale = 1.1f;
    const float titleW = g_font.getStringWidth(inv.categoryName(), titleScale);
    const float titleX = titleAreaX + (titleAreaW - titleW) * 0.5f + kInvTitleOffsetX;
    hudDrawRect(titleAreaX - 2.0f, titleY - 2.0f, titleAreaW, 12.0f, 0x90D0D0D0);
    g_font.drawString(titleX, titleY, inv.categoryName(), 0xFF303030, titleScale);

    const char *hoverName = nullptr;
    if (inv.cursorY() < 5 && inv.cursorX() < 10) {
      int hover = base + inv.cursorY() * 10 + inv.cursorX();
      if (hover >= 0 && hover < invCount) hoverName = getBlockDisplayName(inv.visibleItemAt(hover));
    } else if (inv.cursorY() == 5 && inv.cursorX() < 9) {
      uint8_t hotbarItem = inv.hotbarAt(inv.cursorX());
      if (hotbarItem != BLOCK_AIR) hoverName = getBlockDisplayName(hotbarItem);
    } else if (inv.cursorY() == 5 && inv.cursorX() == 10) {
      hoverName = "Delete";
    } else if (inv.cursorX() == 10) {
      hoverName = "Page Slider";
    }
    if (hoverName) {
      const float hoverScale = 1.0f;
      const float hoverW = g_font.getStringWidth(hoverName, hoverScale);
      float hoverX = cursorX + cellSize + 6.0f;
      float hoverY = cursorY + (cellSize - 8.0f) * 0.5f;
      if (hoverX + hoverW + 6.0f > 475.0f) hoverX = cursorX - hoverW - 8.0f;
      hudDrawRect(hoverX - 2.0f, hoverY - 2.0f, hoverW + 4.0f, 12.0f, 0xA0000000);
      g_font.drawString(hoverX, hoverY, hoverName, 0xFFFFFFFF, hoverScale);
    }
    g_font.drawShadow(16.0f, 250.0f, "Circle: Close  L/R: Category  DPad/Cross: Move/Select", 0xFFFFFFFF, 1.0f);

    sceGuEnable(GU_CULL_FACE);
    sceGuEnable(GU_DEPTH_TEST);
    return;
  }

  // 1. [R] Mine (if looking at a block)
  if (g_player && g_player->getHitResult().hit) {
    g_texBtnR.bind();
    drawQuad2D(xOffset, yBase, icoW, icoW, 0, 0, g_texBtnR.origWidth, g_texBtnR.origHeight);
    xOffset += icoW + 4.0f;
    g_font.drawString(xOffset, yBase + 4.0f, "Mine", 0xFFFFFFFF, 1.0f);
    xOffset += g_font.getStringWidth("Mine", 1.0f) + 16.0f;
  }

  // 2. [L] + [R] Creative
  g_texBtnL.bind();
  drawQuad2D(xOffset, yBase, icoW, icoW, 0, 0, g_texBtnL.origWidth, g_texBtnL.origHeight);
  xOffset += icoW + 2.0f; 
  g_font.drawString(xOffset, yBase + 4.0f, "+", 0xFFFFFFFF, 1.0f);
  xOffset += g_font.getStringWidth("+", 1.0f) + 2.0f;
  g_texBtnR.bind();
  drawQuad2D(xOffset, yBase, icoW, icoW, 0, 0, g_texBtnR.origWidth, g_texBtnR.origHeight);
  xOffset += icoW + 4.0f;
  g_font.drawString(xOffset, yBase + 4.0f, "Creative", 0xFFFFFFFF, 1.0f);
  xOffset += g_font.getStringWidth("Creative", 1.0f) + 16.0f;

  // 3. [L] Place [BlockIcon] (if holding block and hit result hit)
  if (g_player && g_player->getHitResult().hit && g_player->getHeldBlock() != BLOCK_AIR) {
    g_texBtnL.bind();
    drawQuad2D(xOffset, yBase, icoW, icoW, 0, 0, g_texBtnL.origWidth, g_texBtnL.origHeight);
    xOffset += icoW + 4.0f;
    g_font.drawString(xOffset, yBase + 4.0f, "Place", 0xFFFFFFFF, 1.0f);
    xOffset += g_font.getStringWidth("Place", 1.0f) + 8.0f;

    // Draw the held block icon from the terrain atlas
    g_atlas->bind();
    // A quick hack to get UV from g_blockUV. 
    // We'll draw just the first face (side face) 
    float u0 = g_blockUV[g_player->getHeldBlock()].side_x * 16.0f;
    float v0 = g_blockUV[g_player->getHeldBlock()].side_y * 16.0f;
    float u1 = u0 + 16.0f;
    float v1 = v0 + 16.0f;
    drawQuad2D(xOffset + 2.0f, yBase - 2.0f, 16.0f, 16.0f, u0, v0, u1, v1);
  }

  sceGuEnable(GU_CULL_FACE);
  sceGuEnable(GU_DEPTH_TEST);
}

struct FallingBlockVertex {
  float u, v;
  uint32_t color;
  float x, y, z;
};

struct FallingUVSet {
  float tu0, tv0, tu1, tv1;
  float bu0, bv0, bu1, bv1;
  float su0, sv0, su1, sv1;
};

static void renderFallingBlocks() {
  if (!g_level || !g_atlas) return;
  const std::vector<Level::FallingBlockEntity> &falling = g_level->getFallingBlocks();
  if (falling.empty()) return;

  static FallingUVSet s_fallingUV[256];
  static bool s_fallingUVInited = false;
  if (!s_fallingUVInited) {
    const float ts = 1.0f / 16.0f;
    const float eps = 0.125f / 256.0f;
    for (int id = 0; id < 256; ++id) {
      const BlockUV &uv = g_blockUV[id];
      s_fallingUV[id].tu0 = uv.top_x * ts + eps;
      s_fallingUV[id].tv0 = uv.top_y * ts + eps;
      s_fallingUV[id].tu1 = (uv.top_x + 1) * ts - eps;
      s_fallingUV[id].tv1 = (uv.top_y + 1) * ts - eps;
      s_fallingUV[id].bu0 = uv.bot_x * ts + eps;
      s_fallingUV[id].bv0 = uv.bot_y * ts + eps;
      s_fallingUV[id].bu1 = (uv.bot_x + 1) * ts - eps;
      s_fallingUV[id].bv1 = (uv.bot_y + 1) * ts - eps;
      s_fallingUV[id].su0 = uv.side_x * ts + eps;
      s_fallingUV[id].sv0 = uv.side_y * ts + eps;
      s_fallingUV[id].su1 = (uv.side_x + 1) * ts - eps;
      s_fallingUV[id].sv1 = (uv.side_y + 1) * ts - eps;
    }
    s_fallingUVInited = true;
  }

  const bool hasPlayer = (g_player != nullptr);
  const float px = hasPlayer ? g_player->getX() : 0.0f;
  const float py = hasPlayer ? g_player->getY() : 0.0f;
  const float pz = hasPlayer ? g_player->getZ() : 0.0f;
  const float maxDistSq = 64.0f * 64.0f;

  std::vector<size_t> visibleIndices;
  visibleIndices.reserve(falling.size());
  for (size_t i = 0; i < falling.size(); ++i) {
    const Level::FallingBlockEntity &e = falling[i];
    if (e.removed || e.id == BLOCK_AIR) continue;
    if (hasPlayer) {
      float dx = e.x - px;
      float dy = e.y - py;
      float dz = e.z - pz;
      if ((dx * dx + dy * dy + dz * dz) > maxDistSq) continue;
    }
    visibleIndices.push_back(i);
  }
  if (visibleIndices.empty()) return;

  g_atlas->bind();
  sceGuEnable(GU_CULL_FACE);
  sceGuFrontFace(GU_CW);

  // Match chunk opaque pass day/night brightness so falling blocks are not fullbright at night.
  float sunBr = g_level->getSunBrightness();
  float sunMul = sunBr * 0.85f + 0.15f; // same range as chunk ambient: [0.15, 1.0]
  auto scaleGray = [&](uint8_t gray) -> uint8_t {
    int v = (int)(gray * sunMul);
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
  };
  const uint8_t topG = scaleGray(255);
  const uint8_t botG = scaleGray(191);
  const uint8_t side1G = scaleGray(221);
  const uint8_t side2G = scaleGray(204);
  const uint32_t topCol = 0xFF000000u | ((uint32_t)topG << 16) | ((uint32_t)topG << 8) | (uint32_t)topG;
  const uint32_t botCol = 0xFF000000u | ((uint32_t)botG << 16) | ((uint32_t)botG << 8) | (uint32_t)botG;
  const uint32_t side1Col = 0xFF000000u | ((uint32_t)side1G << 16) | ((uint32_t)side1G << 8) | (uint32_t)side1G;
  const uint32_t side2Col = 0xFF000000u | ((uint32_t)side2G << 16) | ((uint32_t)side2G << 8) | (uint32_t)side2G;

  FallingBlockVertex *verts = (FallingBlockVertex *)sceGuGetMemory(visibleIndices.size() * 36 * sizeof(FallingBlockVertex));
  int v = 0;
  auto addFace = [&](float ax, float ay, float az, float bx, float by, float bz,
                     float cx, float cy, float cz, float dx, float dy, float dz,
                     float u0, float v0, float u1, float v1, uint32_t col) {
    // Triangle 1: A-B-C
    verts[v++] = {u0, v0, col, ax, ay, az};
    verts[v++] = {u1, v0, col, bx, by, bz};
    verts[v++] = {u1, v1, col, cx, cy, cz};
    // Triangle 2: A-C-D
    verts[v++] = {u0, v0, col, ax, ay, az};
    verts[v++] = {u1, v1, col, cx, cy, cz};
    verts[v++] = {u0, v1, col, dx, dy, dz};
  };

  for (size_t i = 0; i < visibleIndices.size(); ++i) {
    const size_t idx = visibleIndices[i];
    const Level::FallingBlockEntity &e = falling[idx];
    const FallingUVSet &uv = s_fallingUV[e.id];
    const float x0 = e.x - 0.49f, y0 = e.y - 0.49f, z0 = e.z - 0.49f;
    const float x1 = e.x + 0.49f, y1 = e.y + 0.49f, z1 = e.z + 0.49f;

    // Top
    addFace(x0, y1, z0, x1, y1, z0, x1, y1, z1, x0, y1, z1, uv.tu0, uv.tv0, uv.tu1, uv.tv1, topCol);
    // Bottom
    addFace(x0, y0, z1, x1, y0, z1, x1, y0, z0, x0, y0, z0, uv.bu0, uv.bv0, uv.bu1, uv.bv1, botCol);
    // North (-Z)
    addFace(x0, y0, z0, x1, y0, z0, x1, y1, z0, x0, y1, z0, uv.su0, uv.sv1, uv.su1, uv.sv0, side1Col);
    // South (+Z)
    addFace(x1, y0, z1, x0, y0, z1, x0, y1, z1, x1, y1, z1, uv.su0, uv.sv1, uv.su1, uv.sv0, side1Col);
    // West (-X)
    addFace(x0, y0, z1, x0, y0, z0, x0, y1, z0, x0, y1, z1, uv.su0, uv.sv1, uv.su1, uv.sv0, side2Col);
    // East (+X)
    addFace(x1, y0, z0, x1, y0, z1, x1, y1, z1, x1, y1, z0, uv.su0, uv.sv1, uv.su1, uv.sv0, side2Col);

  }
  sceGuDrawArray(GU_TRIANGLES,
                 GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                 v, 0, verts);
  sceGuFrontFace(GU_CCW);
}

static void game_render() {
  float _tod = g_level->getTimeOfDay();

  // Camera setup
  ScePspFVector3 camPos = {0.0f, 0.0f, 0.0f};
  float yawRad = 0.0f;
  float pitchRad = 0.0f;
  
  if (g_player) {
    camPos = {g_player->getX(), g_player->getY() + 1.62f, g_player->getZ()}; // 4J: heightOffset = 1.62
    yawRad = g_player->getYaw() * Mth::DEGRAD;
    pitchRad = g_player->getPitch() * Mth::DEGRAD;
  }

  ScePspFVector3 lookDir = {
      Mth::sin(yawRad) * Mth::cos(pitchRad), // X
      Mth::sin(pitchRad),                    // Y
      Mth::cos(yawRad) * Mth::cos(pitchRad)  // Z
  };

  ScePspFVector3 lookAt = {camPos.x + lookDir.x, camPos.y + lookDir.y,
                           camPos.z + lookDir.z};

  // Compute fog color
  uint32_t clearColor = 0xFF000000;
  if (g_skyRenderer) {
      clearColor = g_skyRenderer->getFogColor(_tod, lookDir);
  }

  // Underwater effect
  uint8_t headBlock = g_level->getBlock((int)floorf(camPos.x), (int)floorf(camPos.y), (int)floorf(camPos.z));
  auto isWaterId = [](uint8_t b) {
    return b == BLOCK_WATER_STILL || b == BLOCK_WATER_FLOW;
  };
  bool isUnderwater = isWaterId(headBlock);

  float fogNear = 32.0f;
  float fogFar = 64.0f;
  uint32_t fogColor = clearColor;
  float fov = 90.0f;

  // MCPE-like sprint FOV kick with smoothing (similar to GameRenderer::tickFov):
  // fov += (target - fov) * k
  static float s_fovMul = 1.0f;
  float targetFovMul = 1.0f;
  if (g_player && g_player->isSprinting()) {
    // from LocalPlayer::getFieldOfViewModifier:
    // ((walkingSpeed * sprintMod) / defaultWalk + 1) / 2 => (1.3 + 1) / 2 = 1.15
    targetFovMul = 1.15f;
  }
  s_fovMul += (targetFovMul - s_fovMul) * 0.20f;
  fov *= s_fovMul;

  if (isUnderwater) {
    fov = 90.0f * 60.0f / 70.0f;
    fogNear = 0.05f;
    fogFar = 13.0f;

    int eyeX = (int)floorf(camPos.x);
    int eyeY = (int)floorf(camPos.y);
    int eyeZ = (int)floorf(camPos.z);
    int depth = 1;
    for (int i = 1; i <= 3; ++i) {
      if (isWaterId(g_level->getBlock(eyeX, eyeY + i, eyeZ))) depth++;
      else break;
    }
    if (depth >= 3) fogFar = 9.5f;
    else if (depth == 2) fogFar = 11.0f;

    // MCPE-like underwater blue haze.
    fogColor = 0xFFB45A30;
    clearColor = fogColor;
  }

  PSPRenderer_BeginFrame(clearColor, fogNear, fogFar, fogColor, fov);

  PSPRenderer_SetCamera(&camPos, &lookAt);

  if (g_skyRenderer) {
    if (g_player) {
      g_skyRenderer->renderSky(g_player->getX(), g_player->getY(), g_player->getZ(), lookDir);
    }
    sceGuFog(fogNear, fogFar, fogColor);
  }

  // Render chunks: opaque first, then entities, then transparent.
  if (g_player) {
    g_chunkRenderer->renderOpaque(g_player->getX(), g_player->getY(), g_player->getZ());
  }
  renderFallingBlocks();
  if (g_player) {
    g_chunkRenderer->renderTransparent();
  }

  // Render block highlight wireframe
  if (g_player && g_player->getHitResult().hit) {
    BlockHighlight_Draw(g_player->getHitResult().x, g_player->getHitResult().y, g_player->getHitResult().z, g_player->getHitResult().id);
  }

  if (g_cloudRenderer && g_player)
    g_cloudRenderer->renderClouds(g_player->getX(), g_player->getY(), g_player->getZ(), 0.0f, fogColor);

  drawHUD();

  PSPRenderer_EndFrame();
}

// Main entry point
int main(int argc, char *argv[]) {
  setup_callbacks();

  if (!app_init()) {
    pspDebugScreenInit();
    pspDebugScreenPrintf("Init error!\n");
    sceKernelSleepThread();
    return 1;
  }

  uint64_t lastTime = sceKernelGetSystemTimeWide();

  while (true) {
    uint64_t now = sceKernelGetSystemTimeWide();
    float dt = (float)(now - lastTime) / 1000000.0f; // microseconds -> seconds
    if (dt > 0.05f)
      dt = 0.05f; // cap at 20 FPS min
    lastTime = now;

    if (g_appMode == AppMode::MainMenu) {
      g_consoleMenu.update(dt);

      MainMenuAction action = g_consoleMenu.consumeAction();
      if (action == MainMenuAction::ExitGame) {
        break;
      }
      if (action == MainMenuAction::StartGame) {
        g_consoleMenu.releaseResources();
        if (!g_gameInitialized) {
          if (!game_init()) {
            pspDebugScreenInit();
            pspDebugScreenPrintf("Game init error!\n");
            sceKernelSleepThread();
            return 1;
          }
          g_gameInitialized = true;
        }
        g_appMode = AppMode::Gameplay;
      }

      PSPRenderer_BeginFrame(0xFF000000, 1.0f, 16.0f, 0xFF000000, 70.0f);
      sceGumMatrixMode(GU_PROJECTION);
      sceGumLoadIdentity();
      sceGumOrtho(0.0f, 480.0f, 272.0f, 0.0f, -1.0f, 1.0f);
      sceGumMatrixMode(GU_VIEW);
      sceGumLoadIdentity();
      sceGumMatrixMode(GU_MODEL);
      sceGumLoadIdentity();
      g_consoleMenu.render(480, 272);
      PSPRenderer_EndFrame();
      continue;
    }

    game_update(dt);
    game_render();
  }

  sceKernelExitGame();
  return 0;
}
