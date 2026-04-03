// MinecraftPSP - main.cpp
// PSP Entry point, basic game loop

#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <psppower.h>

#include "input/PSPInput.h"
#include "render/BlockHighlight.h"
#include "render/ChunkRenderer.h"
#include "render/CloudRenderer.h"
#include "render/PSPRenderer.h"
#include "render/SkyRenderer.h"
#include "render/TextureAtlas.h"
#include "ui/ConsoleMainMenu.h"
#include "world/AABB.h"
#include "world/Blocks.h"
#include "world/Level.h"
#include "world/Mth.h"
#include "world/Random.h"
#include "world/Raycast.h"
#include <math.h>

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
static bool g_gameInitialized = false;

// HUD UI state
static BitmapFont g_font;
static Texture g_texBtnSquare;
static Texture g_texBtnTriangle;
static Texture g_texBtnR;
static Texture g_texBtnL;


enum class AppMode {
  MainMenu = 0,
  Gameplay,
};

static AppMode g_appMode = AppMode::MainMenu;

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

  g_level = new Level();
  g_skyRenderer = new SkyRenderer(g_level);
  g_cloudRenderer = new CloudRenderer(g_level);

  // Init chunk renderer
  g_chunkRenderer = new ChunkRenderer(g_atlas);
  g_chunkRenderer->setLevel(g_level);

  // Generate a test world
  Random rng(12345LL);
  g_level->generate(&rng);

  // Player start position
  g_player = new Player(g_level);
  g_player->spawn(8.0f, 65.0f, 8.0f);

  return true;
}

// Game loop update
static void game_update(float dt) {
  PSPInput_Update();
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

    uint8_t tx = 13, ty = 12;
    switch (textureIdx) {
      case 0: tx = 13; ty = 12; break; 
      case 1: tx = 14; ty = 12; break;
      case 2: tx = 15; ty = 12; break;
      case 3: tx = 14; ty = 13; break;
    }

    g_blockUV[BLOCK_WATER_STILL] = {tx, ty, tx, ty, tx, ty};
    g_blockUV[BLOCK_WATER_FLOW] = {tx, ty, tx, ty, tx, ty};

    // ONLY update chunks within a 4-block radius of the player
    int minCX = (int)(g_player->getX() - 4.0f) >> 4;
    int maxCX = (int)(g_player->getX() + 4.0f) >> 4;
    int minCZ = (int)(g_player->getZ() - 4.0f) >> 4;
    int maxCZ = (int)(g_player->getZ() + 4.0f) >> 4;

    for (int cx = minCX; cx <= maxCX; cx++) {
      for (int cz = minCZ; cz <= maxCZ; cz++) {
        if (cx >= 0 && cx < WORLD_CHUNKS_X && cz >= 0 && cz < WORLD_CHUNKS_Z) {
          Chunk* c = g_level->getChunk(cx, cz);
          if (c) {
            for (int sy = 0; sy < 4; sy++) {
              c->dirty[sy] = true;
            }
          }
        }
      }
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
    g_font.drawShadow(4.0f, 4.0f, yBuf, 0xFFFFFF00, 1.0f);  // yellow
  }

  // 1. [R] Mine (if looking at a block)
  if (g_player && g_player->getHitResult().hit) {
    g_texBtnR.bind();
    drawQuad2D(xOffset, yBase, icoW, icoW, 0, 0, g_texBtnR.origWidth, g_texBtnR.origHeight);
    xOffset += icoW + 4.0f;
    g_font.drawShadow(xOffset, yBase + 4.0f, "Mine", 0xFFFFFFFF, 1.0f);
    xOffset += g_font.getStringWidth("Mine", 1.0f) + 16.0f;
  }

  // 2. [L] + [R] Creative
  g_texBtnL.bind();
  drawQuad2D(xOffset, yBase, icoW, icoW, 0, 0, g_texBtnL.origWidth, g_texBtnL.origHeight);
  xOffset += icoW + 2.0f; 
  g_font.drawShadow(xOffset, yBase + 4.0f, "+", 0xFFFFFFFF, 1.0f);
  xOffset += g_font.getStringWidth("+", 1.0f) + 2.0f;
  g_texBtnR.bind();
  drawQuad2D(xOffset, yBase, icoW, icoW, 0, 0, g_texBtnR.origWidth, g_texBtnR.origHeight);
  xOffset += icoW + 4.0f;
  g_font.drawShadow(xOffset, yBase + 4.0f, "Creative", 0xFFFFFFFF, 1.0f);
  xOffset += g_font.getStringWidth("Creative", 1.0f) + 16.0f;

  // 3. [L] Place [BlockIcon] (if holding block and hit result hit)
  if (g_player && g_player->getHitResult().hit && g_player->getHeldBlock() != BLOCK_AIR) {
    g_texBtnL.bind();
    drawQuad2D(xOffset, yBase, icoW, icoW, 0, 0, g_texBtnL.origWidth, g_texBtnL.origHeight);
    xOffset += icoW + 4.0f;
    g_font.drawShadow(xOffset, yBase + 4.0f, "Place", 0xFFFFFFFF, 1.0f);
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
  bool isUnderwater = (g_blockProps[headBlock].isLiquid() && headBlock != BLOCK_LAVA_FLOW && headBlock != BLOCK_LAVA_STILL);

  float fogNear = 32.0f;
  float fogFar = 64.0f;
  uint32_t fogColor = clearColor;
  float fov = 90.0f;

  if (isUnderwater) {
    fov = 90.0f * 60.0f / 70.0f;
    fogNear = 0.1f;
    fogFar = 20.0f; 
    // Water fog color (0.4, 0.4, 0.9) -> BGR: 0xFFE56666
    fogColor = 0xFFE56666;
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

  // Render chunks
  if (g_player) {
    g_chunkRenderer->render(g_player->getX(), g_player->getY(), g_player->getZ());
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
