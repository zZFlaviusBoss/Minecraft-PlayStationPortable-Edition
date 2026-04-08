#pragma once
#include <stdint.h>

// Block IDs

enum BlockID : uint16_t {
  BLOCK_AIR = 0,
  BLOCK_STONE = 1,
  BLOCK_GRASS = 2,
  BLOCK_DIRT = 3,
  BLOCK_COBBLESTONE = 4,
  BLOCK_WOOD_PLANK = 5,
  BLOCK_SAPLING = 6,
  BLOCK_TALLGRASS = 31,
  BLOCK_BEDROCK = 7,
  BLOCK_WATER_FLOW = 8,
  BLOCK_WATER_STILL = 9,
  BLOCK_LAVA_FLOW = 10,
  BLOCK_LAVA_STILL = 11,
  BLOCK_SAND = 12,
  BLOCK_GRAVEL = 13,
  BLOCK_GOLD_ORE = 14,
  BLOCK_IRON_ORE = 15,
  BLOCK_COAL_ORE = 16,
  BLOCK_LOG = 17,
  BLOCK_LEAVES = 18,
  BLOCK_GLASS = 20,
  BLOCK_LAPIS_ORE = 21,
  BLOCK_SANDSTONE = 24,
  BLOCK_WOOL = 35,
  BLOCK_FLOWER = 37,
  BLOCK_ROSE = 38,
  BLOCK_MUSHROOM_BROWN = 39,
  BLOCK_MUSHROOM_RED = 40,
  BLOCK_GOLD_BLOCK = 41,
  BLOCK_IRON_BLOCK = 42,
  BLOCK_STONE_SLAB = 44,
  BLOCK_BRICK = 45,
  BLOCK_TNT = 46,
  BLOCK_BOOKSHELF = 47,
  BLOCK_MOSSY_COBBLE = 48,
  BLOCK_OBSIDIAN = 49,
  BLOCK_TORCH = 50,
  BLOCK_FIRE = 51,
  BLOCK_CHEST = 54,
  BLOCK_DIAMOND_ORE = 56,
  BLOCK_DIAMOND_BLOCK = 57,
  BLOCK_CRAFTING_TABLE = 58,
  BLOCK_CROPS = 59,
  BLOCK_FARMLAND = 60,
  BLOCK_FURNACE = 61,
  BLOCK_SIGN = 63,
  BLOCK_DOOR_WOOD = 64,
  BLOCK_LADDER = 65,
  BLOCK_RAIL = 66,
  BLOCK_COBBLE_STAIR = 67,
  BLOCK_LEVER = 69,
  BLOCK_STONE_PLATE = 70,
  BLOCK_DOOR_IRON = 71,
  BLOCK_WOOD_PLATE = 72,
  BLOCK_REDSTONE_ORE = 73,
  BLOCK_TORCH_REDSTONE = 75,
  BLOCK_SNOW = 78,
  BLOCK_ICE = 79,
  BLOCK_SNOW_BLOCK = 80,
  BLOCK_CACTUS = 81,
  BLOCK_CLAY = 82,
  BLOCK_REEDS = 83,
  BLOCK_PUMPKIN = 86,
  BLOCK_NETHERRACK = 87,
  BLOCK_SOULSAND = 88,
  BLOCK_GLOWSTONE = 89,
  BLOCK_EMERALD_ORE = 129,
  BLOCK_STONE_BRICKS = 98,
  BLOCK_FENCE = 85,
  BLOCK_WOOD_STAIR = 53,

  // Custom IDs for colored wool variants (metadata-less representation).
  BLOCK_WOOL_ORANGE = 200,
  BLOCK_WOOL_MAGENTA = 201,
  BLOCK_WOOL_LIGHT_BLUE = 202,
  BLOCK_WOOL_YELLOW = 203,
  BLOCK_WOOL_LIME = 204,
  BLOCK_WOOL_PINK = 205,
  BLOCK_WOOL_GRAY = 206,
  BLOCK_WOOL_LIGHT_GRAY = 207,
  BLOCK_WOOL_CYAN = 208,
  BLOCK_WOOL_PURPLE = 209,
  BLOCK_WOOL_BLUE = 210,
  BLOCK_WOOL_BROWN = 211,
  BLOCK_WOOL_GREEN = 212,
  BLOCK_WOOL_RED = 213,
  BLOCK_WOOL_BLACK = 214,
  BLOCK_WOOD_SLAB = 215,
  BLOCK_COBBLE_SLAB = 216,
  BLOCK_SANDSTONE_SLAB = 217,
  BLOCK_BRICK_SLAB = 218,
  BLOCK_STONE_BRICK_SLAB = 219,
  BLOCK_STONE_SLAB_TOP = 220,
  BLOCK_WOOD_SLAB_TOP = 221,
  BLOCK_COBBLE_SLAB_TOP = 222,
  BLOCK_SANDSTONE_SLAB_TOP = 223,
  BLOCK_BRICK_SLAB_TOP = 224,
  BLOCK_STONE_BRICK_SLAB_TOP = 225,
  BLOCK_STONE_STAIR = 108,
  BLOCK_BRICK_STAIR = 109,
  BLOCK_SANDSTONE_STAIR = 128,
  BLOCK_STONE_BRICK_STAIR = 130,

  BLOCK_COUNT = 256
};

// Block properties
struct BlockProps {
  uint8_t hardness_x10; // hardness * 10 (0 = indestructible)
  uint8_t light_emit;   // emitted light [0..15]
  uint8_t light_block;  // blocked light [0..15]
  uint8_t flags;        // bit 0: solid, bit 1: transparent, bit 2: liquid

  // Bounding box for block highlight (0.0 to 1.0)
  float minX, minY, minZ;
  float maxX, maxY, maxZ;

  bool isSolid() const { return (flags & 1) != 0; }
  bool isTransparent() const { return (flags & 2) != 0; }
  bool isLiquid() const { return (flags & 4) != 0; }
  bool isOpaque() const { return isSolid() && !isTransparent(); }
};

extern BlockProps g_blockProps[BLOCK_COUNT];

// Texture coordinate mappings
struct BlockUV {
  uint8_t top_x, top_y;   // top face
  uint8_t side_x, side_y; // side faces
  uint8_t bot_x, bot_y;   // bottom face
};

extern BlockUV g_blockUV[BLOCK_COUNT];

// Initialize tables - called at startup
void Blocks_Init();
