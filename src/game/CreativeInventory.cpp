#include "game/CreativeInventory.h"
#include "world/Blocks.h"

static const uint8_t kInventoryItems[] = {
  // Blocks
  BLOCK_STONE, BLOCK_COBBLESTONE, BLOCK_DIRT, BLOCK_GRASS,
  BLOCK_SAND, BLOCK_GRAVEL, BLOCK_SANDSTONE, BLOCK_WOOD_PLANK,
  BLOCK_LOG, BLOCK_LEAVES, BLOCK_GLASS, BLOCK_WOOL,
  BLOCK_WOOL_ORANGE, BLOCK_WOOL_MAGENTA, BLOCK_WOOL_LIGHT_BLUE, BLOCK_WOOL_YELLOW,
  BLOCK_WOOL_LIME, BLOCK_WOOL_PINK, BLOCK_WOOL_GRAY, BLOCK_WOOL_LIGHT_GRAY,
  BLOCK_WOOL_CYAN, BLOCK_WOOL_PURPLE, BLOCK_WOOL_BLUE, BLOCK_WOOL_BROWN,
  BLOCK_WOOL_GREEN, BLOCK_WOOL_RED, BLOCK_WOOL_BLACK,
  BLOCK_BRICK, BLOCK_BOOKSHELF, BLOCK_MOSSY_COBBLE, BLOCK_OBSIDIAN,
  BLOCK_NETHERRACK, BLOCK_SOULSAND, BLOCK_GLOWSTONE,
  BLOCK_STONE_BRICKS,
  BLOCK_STONE_SLAB, BLOCK_WOOD_SLAB, BLOCK_COBBLE_SLAB, BLOCK_SANDSTONE_SLAB, BLOCK_BRICK_SLAB, BLOCK_STONE_BRICK_SLAB,
  BLOCK_STONE_STAIR, BLOCK_WOOD_STAIR, BLOCK_COBBLE_STAIR, BLOCK_SANDSTONE_STAIR, BLOCK_BRICK_STAIR, BLOCK_STONE_BRICK_STAIR,
  // Ores
  BLOCK_COAL_ORE, BLOCK_IRON_ORE, BLOCK_GOLD_ORE, BLOCK_DIAMOND_ORE, BLOCK_REDSTONE_ORE, BLOCK_LAPIS_ORE, BLOCK_EMERALD_ORE,
  // Plants
  BLOCK_SAPLING, BLOCK_TALLGRASS, BLOCK_FLOWER, BLOCK_ROSE,
  // Liquids
  BLOCK_WATER_STILL, BLOCK_LAVA_STILL,
  // Minerals
  BLOCK_IRON_BLOCK, BLOCK_GOLD_BLOCK, BLOCK_DIAMOND_BLOCK,
  // Utility
  BLOCK_TNT, BLOCK_CHEST, BLOCK_CRAFTING_TABLE, BLOCK_FURNACE,
  // Nature
  BLOCK_PUMPKIN, BLOCK_CACTUS, BLOCK_REEDS, BLOCK_SNOW_BLOCK, BLOCK_ICE, BLOCK_CLAY
};

struct CatRange { int start; int end; const char *name; };
static const CatRange kCatRanges[] = {
  {0, 46, "Blocks"},
  {47, 53, "Ores"},
  {54, 57, "Plants"},
  {58, 59, "Liquids"},
  {60, 62, "Minerals"},
  {63, 66, "Utility"},
  {67, 72, "Nature"},
  {0, 72, "All"}
};

CreativeInventory::CreativeInventory()
  : m_open(false), m_hotbarSel(0), m_cursorX(0), m_cursorY(0), m_creativePage(0),
    m_usingSlider(false), m_cursorHasItem(false), m_cursorItem(BLOCK_AIR), m_category(0) {
  m_hotbar[0] = BLOCK_COBBLESTONE;
  m_hotbar[1] = BLOCK_STONE;
  m_hotbar[2] = BLOCK_DIRT;
  m_hotbar[3] = BLOCK_WOOD_PLANK;
  m_hotbar[4] = BLOCK_GLASS;
  m_hotbar[5] = BLOCK_SAND;
  m_hotbar[6] = BLOCK_LOG;
  m_hotbar[7] = BLOCK_LEAVES;
  m_hotbar[8] = BLOCK_WATER_STILL;
}

void CreativeInventory::open() { m_open = true; m_cursorX = 0; m_cursorY = 0; m_creativePage = 0; }
void CreativeInventory::close() { m_open = false; }
bool CreativeInventory::isOpen() const { return m_open; }

void CreativeInventory::cycleHotbarRight() { m_hotbarSel = (m_hotbarSel + 1) % 9; }
void CreativeInventory::cycleHotbarLeft() { m_hotbarSel = (m_hotbarSel + 8) % 9; }

void CreativeInventory::moveRight() {
  if (m_cursorY == 5) {
    // On hotbar row skip the unused x=9 slot: jump from slot 8 directly to delete slot (x=10).
    if (m_cursorX < 8) m_cursorX++;
    else if (m_cursorX == 8) m_cursorX = 10;
  } else if (m_cursorX < 10) {
    m_cursorX++;
  }
}

void CreativeInventory::moveLeft() {
  if (m_cursorY == 5) {
    // On hotbar row skip the unused x=9 slot in reverse as well.
    if (m_cursorX == 10) m_cursorX = 8;
    else if (m_cursorX > 0 && m_cursorX <= 8) m_cursorX--;
  } else if (m_cursorX > 0) {
    m_cursorX--;
  }
}

void CreativeInventory::moveDown() {
  m_usingSlider = (m_cursorX == 10);
  if (m_usingSlider) {
    if (m_creativePage < maxPage()) m_creativePage++;
  } else if (m_cursorY < 5) {
    m_cursorY++;
  }
}

void CreativeInventory::moveUp() {
  m_usingSlider = (m_cursorX == 10);
  if (m_usingSlider) {
    if (m_creativePage > 0) m_creativePage--;
  } else if (m_cursorY > 0) {
    m_cursorY--;
  }
}

void CreativeInventory::pressCross() {
  if (m_cursorY == 5 && m_cursorX == 10) {
    if (m_cursorHasItem) {
      m_cursorHasItem = false;
      m_cursorItem = BLOCK_AIR;
    }
    return;
  }

  if (m_cursorY == 5) {
    if (m_cursorX > 8) return;
    m_hotbarSel = m_cursorX;
    uint8_t &slot = m_hotbar[m_cursorX];
    if (!m_cursorHasItem) {
      if (slot != BLOCK_AIR) {
        m_cursorItem = slot;
        m_cursorHasItem = true;
        slot = BLOCK_AIR;
      }
    } else {
      uint8_t old = slot;
      slot = m_cursorItem;
      if (old == BLOCK_AIR) {
        m_cursorHasItem = false;
        m_cursorItem = BLOCK_AIR;
      } else {
        m_cursorItem = old;
      }
    }
    return;
  }

  int idx = m_creativePage * 50 + m_cursorY * 10 + m_cursorX;
  if (m_cursorX < 10 && idx >= 0 && idx < categoryItemCount() && !m_cursorHasItem) {
    m_cursorItem = categoryItemAt(idx);
    m_cursorHasItem = true;
  }
}

void CreativeInventory::clearCursorSelection() {
  m_cursorHasItem = false;
  m_cursorItem = BLOCK_AIR;
}

uint8_t CreativeInventory::heldBlock() const { return m_hotbar[m_hotbarSel]; }

int CreativeInventory::hotbarSel() const { return m_hotbarSel; }
void CreativeInventory::setHotbarSel(int sel) { m_hotbarSel = (sel < 0) ? 0 : (sel > 8 ? 8 : sel); }
int CreativeInventory::cursorX() const { return m_cursorX; }
int CreativeInventory::cursorY() const { return m_cursorY; }
int CreativeInventory::creativePage() const { return m_creativePage; }
int CreativeInventory::category() const { return m_category; }
const char *CreativeInventory::categoryName() const { return kCatRanges[m_category].name; }
void CreativeInventory::prevCategory() {
  m_category = (m_category + 7) % 8;
  m_creativePage = 0;
}
void CreativeInventory::nextCategory() {
  m_category = (m_category + 1) % 8;
  m_creativePage = 0;
}
bool CreativeInventory::usingSlider() const { return m_usingSlider; }
bool CreativeInventory::cursorHasItem() const { return m_cursorHasItem; }
uint8_t CreativeInventory::cursorItem() const { return m_cursorItem; }

uint8_t CreativeInventory::hotbarAt(int idx) const { return (idx >= 0 && idx < 9) ? m_hotbar[idx] : BLOCK_AIR; }
void CreativeInventory::setHotbarAt(int idx, uint8_t id) { if (idx >= 0 && idx < 9) m_hotbar[idx] = id; }
int CreativeInventory::visibleItemCount() const { return categoryItemCount(); }
uint8_t CreativeInventory::visibleItemAt(int idx) const { return categoryItemAt(idx); }

int CreativeInventory::inventoryItemCount() { return (int)(sizeof(kInventoryItems) / sizeof(kInventoryItems[0])); }
uint8_t CreativeInventory::inventoryItemAt(int idx) {
  if (idx < 0 || idx >= inventoryItemCount()) return BLOCK_AIR;
  return kInventoryItems[idx];
}

int CreativeInventory::maxPage() const { return categoryItemCount() / 50; }
int CreativeInventory::categoryItemCount() const {
  const CatRange &r = kCatRanges[m_category];
  return r.end - r.start + 1;
}
uint8_t CreativeInventory::categoryItemAt(int idx) const {
  const CatRange &r = kCatRanges[m_category];
  if (idx < 0 || idx >= categoryItemCount()) return BLOCK_AIR;
  return kInventoryItems[r.start + idx];
}
