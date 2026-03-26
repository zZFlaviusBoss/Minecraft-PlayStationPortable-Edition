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
#include "world/Level.h"
#include "world/AABB.h"
#include "render/PSPRenderer.h"
#include "render/SkyRenderer.h"
#include "render/TextureAtlas.h"
#include "world/Blocks.h"
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

// Player state
struct PlayerState {
  float x, y, z;          // position
  float yaw, pitch;       // camera rotation (degrees)
  float velY;             // vertical velocity (gravity)
  bool onGround;
  bool isFlying;          // creative flight active
  float jumpDoubleTapTimer; // countdown for double-tap detection
};

// Global state
static PlayerState g_player;
static Level *g_level = nullptr;
static SkyRenderer *g_skyRenderer = nullptr;
static CloudRenderer *g_cloudRenderer = nullptr;
static ChunkRenderer *g_chunkRenderer = nullptr;
static TextureAtlas *g_atlas = nullptr;
static RayHit g_hitResult;       // Block the player is currently looking at
static uint8_t g_heldBlock = BLOCK_COBBLESTONE; // Block to place

// Initialization
static bool game_init() {
  // Overclock PSP to max for performance
  scePowerSetClockFrequency(333, 333, 166);

  // Init block tables
  Blocks_Init();

  // Init sin/cos lookup table
  Mth::init();

  // Init PSP renderer (sceGu)
  if (!PSPRenderer_Init())
    return false;

  // Load terrain.png from MS0:/PSP/GAME/MinecraftPSP/res/
  g_atlas = new TextureAtlas();
  if (!g_atlas->load("res/terrain.png"))
    return false;

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
  g_player.x = 8.0f;
  g_player.y = 65.0f;
  g_player.z = 8.0f;
  g_player.yaw = 0.0f;
  g_player.pitch = 0.0f;
  g_player.velY = 0.0f;
  g_player.onGround = false;
  g_player.isFlying = false;
  g_player.jumpDoubleTapTimer = 0.0f;

  return true;
}

// Game loop update
static void game_update(float dt) {
  PSPInput_Update();
  if (g_level) {
    g_level->tick();
  }

  float moveSpeed = (g_player.isFlying ? 10.0f : 5.0f) * dt;
  float lookSpeed = 120.0f * dt;

  // Rotation with right stick (Face Buttons)
  float lx = PSPInput_StickX(1);
  float ly = PSPInput_StickY(1);
  g_player.yaw += lx * lookSpeed;
  g_player.pitch += ly * lookSpeed;
  g_player.pitch = Mth::clamp(g_player.pitch, -89.0f, 89.0f);

  // Movement with left stick (Analog)
  float fx = -PSPInput_StickX(0);
  float fz = -PSPInput_StickY(0);

  float yawRad = g_player.yaw * Mth::DEGRAD;

  float dx = (fx * Mth::cos(yawRad) + fz * Mth::sin(yawRad)) * moveSpeed;
  float dz = (-fx * Mth::sin(yawRad) + fz * Mth::cos(yawRad)) * moveSpeed;

  const float R = 0.3f;   // 4J: setSize(0.6, 1.8)
  const float H = 1.8f;   // 4J: player bounding box height

  // Vertical movement
  float dy = 0.0f;
  if (g_player.isFlying) {
    float flySpeed = 10.0f * dt;
    if (PSPInput_IsHeld(PSP_CTRL_SELECT))
      dy = flySpeed;  // Ascend
    if (PSPInput_IsHeld(PSP_CTRL_DOWN))
      dy = -flySpeed;  // Descend
    g_player.velY = 0.0f;
  } else {
    g_player.velY -= 20.0f * dt;
    dy = g_player.velY * dt;
  }

  // Collision
  AABB player_aabb(g_player.x - R, g_player.y, g_player.z - R,
                   g_player.x + R, g_player.y + H, g_player.z + R);

  AABB* expanded = player_aabb.expand(dx, dy, dz);
  std::vector<AABB> cubes = g_level->getCubes(*expanded);
  delete expanded;

  float dy_org = dy;
  for (auto& cube : cubes) dy = cube.clipYCollide(&player_aabb, dy);
  player_aabb.move(0, dy, 0);

  for (auto& cube : cubes) dx = cube.clipXCollide(&player_aabb, dx);
  player_aabb.move(dx, 0, 0);

  for (auto& cube : cubes) dz = cube.clipZCollide(&player_aabb, dz);
  player_aabb.move(0, 0, dz);

  g_player.onGround = (dy_org != dy && dy_org < 0.0f);
  if (g_player.onGround || dy_org != dy) {
    g_player.velY = 0.0f;
  }

  g_player.x = (player_aabb.x0 + player_aabb.x1) / 2.0f;
  g_player.y = player_aabb.y0;
  g_player.z = (player_aabb.z0 + player_aabb.z1) / 2.0f;

  // Enforce world bounds natively
  const float WORLD_MAX_X = (float)(WORLD_CHUNKS_X * CHUNK_SIZE_X - 1);
  const float WORLD_MAX_Z = (float)(WORLD_CHUNKS_Z * CHUNK_SIZE_Z - 1);
  if (g_player.x < 0.5f) g_player.x = 0.5f;
  if (g_player.x > WORLD_MAX_X) g_player.x = WORLD_MAX_X;
  if (g_player.z < 0.5f) g_player.z = 0.5f;
  if (g_player.z > WORLD_MAX_Z) g_player.z = WORLD_MAX_Z;

  // Controls: Jump/Fly
  static const float DOUBLE_TAP_WINDOW = 0.35f;
  if (g_player.jumpDoubleTapTimer > 0.0f)
    g_player.jumpDoubleTapTimer -= dt;

  if (PSPInput_JustPressed(PSP_CTRL_SELECT)) {
    if (g_player.jumpDoubleTapTimer > 0.0f) {
      g_player.isFlying = !g_player.isFlying;
      g_player.velY = 0.0f;
      g_player.jumpDoubleTapTimer = 0.0f;
    } else {
      if (!g_player.isFlying && g_player.onGround) {
        g_player.velY = 6.5f;
        g_player.onGround = false;
      }
      g_player.jumpDoubleTapTimer = DOUBLE_TAP_WINDOW;
    }
  }

  // Raycast block target
  {
    float eyeX = g_player.x;
    float eyeY = g_player.y + 1.6f;
    float eyeZ = g_player.z;
    float pitchRad = g_player.pitch * Mth::DEGRAD;
    float dirX = Mth::sin(yawRad) * Mth::cos(pitchRad);
    float dirY = Mth::sin(pitchRad);
    float dirZ = Mth::cos(yawRad) * Mth::cos(pitchRad);
    g_hitResult = raycast(g_level, eyeX, eyeY, eyeZ, dirX, dirY, dirZ, 5.0f);
  }

  // Block action cooldown
  static float breakCooldown = 0.0f;
  if (breakCooldown > 0.0f) breakCooldown -= dt;

  // Block breaking
  bool doBreak = false;
  if (PSPInput_IsHeld(PSP_CTRL_LTRIGGER) && breakCooldown <= 0.0f) {
    doBreak = true;
    breakCooldown = 0.15f;
  }

  if (doBreak && g_hitResult.hit) {
    uint8_t oldBlock = g_level->getBlock(g_hitResult.x, g_hitResult.y, g_hitResult.z);
    if (oldBlock != BLOCK_BEDROCK) {
      int bx = g_hitResult.x, by = g_hitResult.y, bz = g_hitResult.z;
      g_level->setBlock(bx, by, bz, BLOCK_AIR);
      g_level->markDirty(bx, by, bz);

      // Cascading plant break (if we broke the soil, the plant pops off)
      uint8_t topId = g_level->getBlock(bx, by + 1, bz);
      if (topId != BLOCK_AIR && !g_blockProps[topId].isSolid() && !g_blockProps[topId].isLiquid()) {
          g_level->setBlock(bx, by + 1, bz, BLOCK_AIR);
          g_level->markDirty(bx, by + 1, bz);
          g_chunkRenderer->rebuildChunkNow(bx >> 4, bz >> 4, (by + 1) >> 4);
      }

      // Rebuild the central chunk synchronously
      int cx = bx >> 4, cz = bz >> 4, sy = by >> 4;
      g_chunkRenderer->rebuildChunkNow(cx, cz, sy);
      
      // Rebuild neighbor chunks
      if ((bx & 0xF) == 0  && cx > 0)
        g_chunkRenderer->rebuildChunkNow(cx - 1, cz, sy);
      if ((bx & 0xF) == 15 && cx < WORLD_CHUNKS_X - 1)
        g_chunkRenderer->rebuildChunkNow(cx + 1, cz, sy);
      if ((bz & 0xF) == 0  && cz > 0)
        g_chunkRenderer->rebuildChunkNow(cx, cz - 1, sy);
      if ((bz & 0xF) == 15 && cz < WORLD_CHUNKS_Z - 1)
        g_chunkRenderer->rebuildChunkNow(cx, cz + 1, sy);
      if ((by & 0xF) == 0  && sy > 0)
        g_chunkRenderer->rebuildChunkNow(cx, cz, sy - 1);
      if ((by & 0xF) == 15 && sy < 3)
        g_chunkRenderer->rebuildChunkNow(cx, cz, sy + 1);
    }
  }

  // Place block
  if (PSPInput_JustPressed(PSP_CTRL_UP) && g_hitResult.hit) {
    int px = g_hitResult.nx;
    int py = g_hitResult.ny;
    int pz = g_hitResult.nz;

    // If we click on a plant, replace the plant directly instead of placing adjacent
    uint8_t hitId = g_level->getBlock(g_hitResult.x, g_hitResult.y, g_hitResult.z);
    if (hitId != BLOCK_AIR && !g_blockProps[hitId].isSolid() && !g_blockProps[hitId].isLiquid()) {
      px = g_hitResult.x;
      py = g_hitResult.y;
      pz = g_hitResult.z;
    }

    // If we are placing a plant, check if the block below is valid soil (grass/dirt/farmland)
    bool canPlace = true;
    if (g_heldBlock == BLOCK_SAPLING || g_heldBlock == BLOCK_TALLGRASS || g_heldBlock == BLOCK_FLOWER || 
        g_heldBlock == BLOCK_ROSE || g_heldBlock == BLOCK_MUSHROOM_BROWN || g_heldBlock == BLOCK_MUSHROOM_RED) {
      uint8_t floorId = g_level->getBlock(px, py - 1, pz);
      if (floorId != BLOCK_GRASS && floorId != BLOCK_DIRT && floorId != BLOCK_FARMLAND) {
        canPlace = false;
      }
    }

    // Don't place if it would overlap with the player
    int playerMinX = (int)floorf(g_player.x - R);
    int playerMaxX = (int)floorf(g_player.x + R);
    int playerMinY = (int)floorf(g_player.y);
    int playerMaxY = (int)floorf(g_player.y + H);
    int playerMinZ = (int)floorf(g_player.z - R);
    int playerMaxZ = (int)floorf(g_player.z + R);

    bool overlaps = (px >= playerMinX && px <= playerMaxX &&
                     py >= playerMinY && py <= playerMaxY &&
                     pz >= playerMinZ && pz <= playerMaxZ);

    uint8_t targetBlock = g_level->getBlock(px, py, pz);
    bool canReplaceTarget = (targetBlock == BLOCK_AIR || (!g_blockProps[targetBlock].isSolid() && !g_blockProps[targetBlock].isLiquid()));

    if (canPlace && !overlaps && canReplaceTarget) {
      g_level->setBlock(px, py, pz, g_heldBlock);
      g_level->markDirty(px, py, pz);

      // Immediately rebuild the central subchunk
      int cx = px >> 4, cz = pz >> 4, sy = py >> 4;
      g_chunkRenderer->rebuildChunkNow(cx, cz, sy);
      
      // Synchronously rebuild neighbor chunks at chunk boundaries
      if ((px & 0xF) == 0  && cx > 0)
        g_chunkRenderer->rebuildChunkNow(cx - 1, cz, sy);
      if ((px & 0xF) == 15 && cx < WORLD_CHUNKS_X - 1)
        g_chunkRenderer->rebuildChunkNow(cx + 1, cz, sy);
      if ((pz & 0xF) == 0  && cz > 0)
        g_chunkRenderer->rebuildChunkNow(cx, cz - 1, sy);
      if ((pz & 0xF) == 15 && cz < WORLD_CHUNKS_Z - 1)
        g_chunkRenderer->rebuildChunkNow(cx, cz + 1, sy);
      if ((py & 0xF) == 0  && sy > 0)
        g_chunkRenderer->rebuildChunkNow(cx, cz, sy - 1);
      if ((py & 0xF) == 15 && sy < 3)
        g_chunkRenderer->rebuildChunkNow(cx, cz, sy + 1);
    }
  }

  // Cycle hotbar
  static const uint8_t PLACEABLE[] = {
    BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT, BLOCK_COBBLESTONE,
    BLOCK_WOOD_PLANK, BLOCK_SAND, BLOCK_GRAVEL, BLOCK_LOG,
    BLOCK_LEAVES, BLOCK_GLASS, BLOCK_SANDSTONE, BLOCK_WOOL,
    BLOCK_GOLD_BLOCK, BLOCK_IRON_BLOCK, BLOCK_BRICK,
    BLOCK_BOOKSHELF, BLOCK_MOSSY_COBBLE, BLOCK_OBSIDIAN,
    BLOCK_GLOWSTONE, BLOCK_PUMPKIN,
    BLOCK_FLOWER, BLOCK_ROSE, BLOCK_SAPLING, BLOCK_TALLGRASS
  };
  static const int NUM_PLACEABLE = sizeof(PLACEABLE) / sizeof(PLACEABLE[0]);
  static int placeIdx = 3; // start at cobblestone

  if (PSPInput_JustPressed(PSP_CTRL_RIGHT)) {
    placeIdx = (placeIdx + 1) % NUM_PLACEABLE;
    g_heldBlock = PLACEABLE[placeIdx];
  }
  if (PSPInput_JustPressed(PSP_CTRL_LEFT)) {
    placeIdx = (placeIdx - 1 + NUM_PLACEABLE) % NUM_PLACEABLE;
    g_heldBlock = PLACEABLE[placeIdx];
  }
}

static void game_render() {
  float _tod = g_level->getTimeOfDay();

  // Camera setup
  ScePspFVector3 camPos = {g_player.x, g_player.y + 1.62f, g_player.z}; // 4J: heightOffset = 1.62
  
  float yawRad = g_player.yaw * Mth::DEGRAD;
  float pitchRad = g_player.pitch * Mth::DEGRAD;

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
    g_skyRenderer->renderSky(g_player.x, g_player.y, g_player.z, lookDir);
    sceGuFog(fogNear, fogFar, fogColor);
  }

  // Render chunks
  g_chunkRenderer->render(g_player.x, g_player.y, g_player.z);

  // Render block highlight wireframe
  if (g_hitResult.hit) {
    BlockHighlight_Draw(g_hitResult.x, g_hitResult.y, g_hitResult.z, g_hitResult.id);
  }

  if (g_cloudRenderer)
    g_cloudRenderer->renderClouds(g_player.x, g_player.y, g_player.z, 0.0f);

  // TODO: HUD (hotbar, crosshair)

  PSPRenderer_EndFrame();
}

// Main entry point
int main(int argc, char *argv[]) {
  setup_callbacks();

  if (!game_init()) {
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

    game_update(dt);
    game_render();
  }

  return 0;
}
