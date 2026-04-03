#pragma once
#include "../world/Blocks.h"
#include "../world/chunk_defs.h"
#include "TextureAtlas.h"
#include <stdint.h>

class Random;



#include "../world/Chunk.h"
#include "../world/Level.h"

#include "Tesselator.h"

class ChunkRenderer {
public:
  ChunkRenderer(TextureAtlas *atlas);
  ~ChunkRenderer();

  void setLevel(Level *level);
  void render(float camX, float camY, float camZ);
  // Immediately compile a single subchunk (all 5 steps in one frame).
  // Use this for player-triggered block changes to avoid visible holes.
  void rebuildChunkNow(int cx, int cz, int sy);

private:
  Level *m_level;
  TextureAtlas *m_atlas;

  // Asynchronous Compilation State Machine
  void processCompileQueue(float camX, float camY, float camZ);
  int m_compileStep;
  Chunk *m_compileChunk;
  int m_compileSy;
  
  Tesselator m_opaqueTess;
  Tesselator m_transTess;
  Tesselator m_transFancyTess;
  Tesselator m_emitTess;  // Block-lit (torch) faces, always full brightness
};
