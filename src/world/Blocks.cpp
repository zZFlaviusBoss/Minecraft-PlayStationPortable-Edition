// Block property tables and UV atlas
// UV coordinates from original Minecraft terrain.png (16x16 grid)

#include "Blocks.h"
#include <string.h>

BlockProps g_blockProps[BLOCK_COUNT];
BlockUV g_blockUV[BLOCK_COUNT];

// Macros for compact initialization
// flags: S=Solid, T=Transparent, L=Liquid
#define SOLID 1
#define TRANSP 2
#define LIQUID 4

// props(hardness*10, light_emit, light_block, flags)
#define DEF_PROPS(id, h, le, lb, fl)                                           \
  do {                                                                         \
    g_blockProps[id].hardness_x10 = (uint8_t)(h);                              \
    g_blockProps[id].light_emit = (uint8_t)(le);                               \
    g_blockProps[id].light_block = (uint8_t)(lb);                              \
    g_blockProps[id].flags = (uint8_t)(fl);                                    \
  } while (0)

// uv(id, tx, ty, sx, sy, bx, by) -- coordinates in 0-15 grid
#define DEF_UV(id, tx, ty, sx, sy, bx, by)                                     \
  g_blockUV[id] = {(uint8_t)(tx), (uint8_t)(ty), (uint8_t)(sx),                \
                   (uint8_t)(sy), (uint8_t)(bx), (uint8_t)(by)}

void Blocks_Init() {
  // Initialize everything with zero (AIR)
  memset(g_blockProps, 0, sizeof(g_blockProps));
  memset(g_blockUV, 0, sizeof(g_blockUV));

  // Default AABB for all blocks is a full 1x1x1 cube
  for (int i = 0; i < BLOCK_COUNT; i++) {
    g_blockProps[i].minX = 0.0f;
    g_blockProps[i].minY = 0.0f;
    g_blockProps[i].minZ = 0.0f;
    g_blockProps[i].maxX = 1.0f;
    g_blockProps[i].maxY = 1.0f;
    g_blockProps[i].maxZ = 1.0f;
  }

  // === Properties ===
  //               id              hard  emit  block  flags
  DEF_PROPS(BLOCK_STONE, 15, 0, 15, SOLID);
  DEF_PROPS(BLOCK_GRASS, 6, 0, 15, SOLID);
  DEF_PROPS(BLOCK_DIRT, 5, 0, 15, SOLID);
  DEF_PROPS(BLOCK_COBBLESTONE, 20, 0, 15, SOLID);
  DEF_PROPS(BLOCK_WOOD_PLANK, 15, 0, 15, SOLID);
  DEF_PROPS(BLOCK_SAPLING, 0, 0, 0, TRANSP);
  DEF_PROPS(BLOCK_BEDROCK, 0, 0, 15, SOLID); // 0=indestructible
  DEF_PROPS(BLOCK_WATER_FLOW, 1, 0, 2, LIQUID | TRANSP);
  DEF_PROPS(BLOCK_WATER_STILL, 1, 0, 2, LIQUID | TRANSP);
  DEF_PROPS(BLOCK_LAVA_FLOW, 1, 15, 0, LIQUID);
  DEF_PROPS(BLOCK_LAVA_STILL, 1, 15, 0, LIQUID);
  DEF_PROPS(BLOCK_SAND, 5, 0, 15, SOLID);
  DEF_PROPS(BLOCK_GRAVEL, 6, 0, 15, SOLID);
  DEF_PROPS(BLOCK_GOLD_ORE, 30, 0, 15, SOLID);
  DEF_PROPS(BLOCK_IRON_ORE, 30, 0, 15, SOLID);
  DEF_PROPS(BLOCK_COAL_ORE, 30, 0, 15, SOLID);
  DEF_PROPS(BLOCK_LOG, 20, 0, 15, SOLID);
  DEF_PROPS(BLOCK_LEAVES, 2, 0, 1, SOLID | TRANSP);
  DEF_PROPS(BLOCK_GLASS, 3, 0, 0, SOLID | TRANSP);
  DEF_PROPS(BLOCK_SANDSTONE, 8, 0, 15, SOLID);
  DEF_PROPS(BLOCK_WOOL, 8, 0, 15, SOLID);
  DEF_PROPS(BLOCK_TALLGRASS, 0, 0, 0, TRANSP);
  DEF_PROPS(BLOCK_FLOWER, 0, 0, 0, TRANSP);
  DEF_PROPS(BLOCK_ROSE, 0, 0, 0, TRANSP);
  DEF_PROPS(BLOCK_GOLD_BLOCK, 30, 0, 15, SOLID);
  DEF_PROPS(BLOCK_IRON_BLOCK, 50, 0, 15, SOLID);
  DEF_PROPS(BLOCK_BRICK, 20, 0, 15, SOLID);
  DEF_PROPS(BLOCK_TNT, 0, 0, 15, SOLID);
  DEF_PROPS(BLOCK_OBSIDIAN, 500, 0, 15, SOLID);
  DEF_PROPS(BLOCK_TORCH, 0, 14, 0, TRANSP);
  DEF_PROPS(BLOCK_DIAMOND_ORE, 30, 0, 15, SOLID);
  DEF_PROPS(BLOCK_DIAMOND_BLOCK, 50, 0, 15, SOLID);
  DEF_PROPS(BLOCK_CRAFTING_TABLE, 25, 0, 15, SOLID);
  DEF_PROPS(BLOCK_FARMLAND, 6, 0, 15, SOLID);
  DEF_PROPS(BLOCK_FURNACE, 35, 0, 15, SOLID);
  DEF_PROPS(BLOCK_SNOW, 2, 0, 0, TRANSP);
  DEF_PROPS(BLOCK_ICE, 5, 0, 3, SOLID | TRANSP);
  DEF_PROPS(BLOCK_SNOW_BLOCK, 2, 0, 15, SOLID);
  DEF_PROPS(BLOCK_CACTUS, 2, 0, 0, TRANSP);
  DEF_PROPS(BLOCK_CLAY, 6, 0, 15, SOLID);
  DEF_PROPS(BLOCK_NETHERRACK, 4, 0, 15, SOLID);
  DEF_PROPS(BLOCK_GLOWSTONE, 3, 15, 15, SOLID);
  DEF_PROPS(BLOCK_FENCE, 20, 0, 0, TRANSP);
  DEF_PROPS(BLOCK_CHEST, 25, 0, 0, SOLID);
  DEF_PROPS(BLOCK_MOSSY_COBBLE, 20, 0, 15, SOLID);
  DEF_PROPS(BLOCK_LADDER, 4, 0, 0, TRANSP);
  DEF_PROPS(BLOCK_REEDS, 0, 0, 0, TRANSP);
  DEF_PROPS(BLOCK_PUMPKIN, 10, 0, 15, SOLID);

  // Custom hitboxes
  auto setBounds = [](uint8_t id, float mx, float my, float mz, float Mx, float My, float Mz) {
    g_blockProps[id].minX = mx; g_blockProps[id].minY = my; g_blockProps[id].minZ = mz;
    g_blockProps[id].maxX = Mx; g_blockProps[id].maxY = My; g_blockProps[id].maxZ = Mz;
  };

  setBounds(BLOCK_TALLGRASS, 0.2f, 0.0f, 0.2f, 0.8f, 0.8f, 0.8f);
  setBounds(BLOCK_FLOWER,    0.3f, 0.0f, 0.3f, 0.7f, 0.6f, 0.7f);
  setBounds(BLOCK_ROSE,      0.3f, 0.0f, 0.3f, 0.7f, 0.6f, 0.7f);
  setBounds(BLOCK_SAPLING,   0.2f, 0.0f, 0.2f, 0.8f, 0.8f, 0.8f);
  setBounds(BLOCK_TORCH,     0.4f, 0.0f, 0.4f, 0.6f, 0.6f, 0.6f);
  setBounds(BLOCK_CHEST,     0.0625f, 0.0f, 0.0625f, 0.9375f, 0.875f, 0.9375f);
  setBounds(BLOCK_SNOW,      0.0f, 0.0f, 0.0f, 1.0f, 0.125f, 1.0f);

  // === UV Atlas (terrain.png 16x16 grid) ===
  //       id                top      side     bottom
  DEF_UV(BLOCK_STONE, 1, 0, 1, 0, 1, 0);
  DEF_UV(BLOCK_GRASS, 0, 0, 3, 0, 2, 0); // top=grass,side=grass_side,bot=dirt
  DEF_UV(BLOCK_DIRT, 2, 0, 2, 0, 2, 0);
  DEF_UV(BLOCK_COBBLESTONE, 0, 1, 0, 1, 0, 1);
  DEF_UV(BLOCK_WOOD_PLANK, 4, 0, 4, 0, 4, 0);
  DEF_UV(BLOCK_BEDROCK, 1, 1, 1, 1, 1, 1);
  DEF_UV(BLOCK_SAND, 2, 1, 2, 1, 2, 1);
  DEF_UV(BLOCK_GRAVEL, 3, 1, 3, 1, 3, 1);
  DEF_UV(BLOCK_GOLD_ORE, 0, 2, 0, 2, 0, 2);
  DEF_UV(BLOCK_IRON_ORE, 1, 2, 1, 2, 1, 2);
  DEF_UV(BLOCK_COAL_ORE, 2, 2, 2, 2, 2, 2);
  DEF_UV(BLOCK_LOG, 5, 1, 4, 1, 5, 1); // top=log_top,side=log_side
  DEF_UV(BLOCK_LEAVES, 4, 3, 4, 3, 4, 3);
  DEF_UV(BLOCK_GLASS, 1, 3, 1, 3, 1, 3);
  DEF_UV(BLOCK_SANDSTONE, 0, 12, 0, 11, 0, 13);
  DEF_UV(BLOCK_WOOL, 0, 4, 0, 4, 0, 4);
  DEF_UV(BLOCK_GOLD_BLOCK, 7, 1, 7, 1, 7, 1);
  DEF_UV(BLOCK_IRON_BLOCK, 6, 1, 6, 1, 6, 1);
  DEF_UV(BLOCK_BRICK, 7, 0, 7, 0, 7, 0);
  DEF_UV(BLOCK_TNT, 9, 0, 8, 0, 10, 0);
  DEF_UV(BLOCK_OBSIDIAN, 5, 2, 5, 2, 5, 2);
  DEF_UV(BLOCK_DIAMOND_ORE, 3, 3, 3, 3, 3, 3);
  DEF_UV(BLOCK_DIAMOND_BLOCK, 8, 1, 8, 1, 8, 1);
  DEF_UV(BLOCK_CRAFTING_TABLE, 11, 2, 11, 3, 4, 0);
  DEF_UV(BLOCK_FARMLAND, 7, 5, 3, 0, 2, 0); // top=farmland,side=dirt
  DEF_UV(BLOCK_FURNACE, 14, 3, 12, 2, 6, 0);
  DEF_UV(BLOCK_SNOW, 2, 4, 2, 4, 2, 4);
  DEF_UV(BLOCK_ICE, 3, 4, 3, 4, 3, 4);
  DEF_UV(BLOCK_SNOW_BLOCK, 2, 4, 4, 4, 2, 4);
  DEF_UV(BLOCK_NETHERRACK, 7, 6, 7, 6, 7, 6);
  DEF_UV(BLOCK_GLOWSTONE, 9, 6, 9, 6, 9, 6);
  DEF_UV(BLOCK_CHEST, 10, 1, 9, 1, 10, 1);
  DEF_UV(BLOCK_MOSSY_COBBLE, 4, 2, 4, 2, 4, 2);
  DEF_UV(BLOCK_CLAY, 8, 4, 8, 4, 8, 4);
  DEF_UV(BLOCK_PUMPKIN, 6, 6, 7, 7, 6, 6);
  // Cross-sprite plants (UV tile used for all quads in the X-pattern)
  DEF_UV(BLOCK_TALLGRASS, 7, 2, 7, 2, 7, 2);    // tallgrass col=7, row=2
  DEF_UV(BLOCK_FLOWER, 13, 0, 13, 0, 13, 0); // dandelion col=13, row=0
  DEF_UV(BLOCK_ROSE, 12, 0, 12, 0, 12, 0);    // rose col=12, row=0
  // Water - use PE/MCPE-compatible water tiles:
  // still = col13,row12 ; flowing/side = col14,row12
  DEF_UV(BLOCK_WATER_STILL, 13, 12, 14, 12, 14, 12);
  DEF_UV(BLOCK_WATER_FLOW, 14, 12, 14, 12, 14, 12);
  // Lava (from provided atlas map): row15,col14 => x=13,y=14
  DEF_UV(BLOCK_LAVA_STILL, 13, 14, 13, 14, 13, 14);
  DEF_UV(BLOCK_LAVA_FLOW, 13, 14, 13, 14, 13, 14);
}
