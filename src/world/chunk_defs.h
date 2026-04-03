#pragma once
// Common chunk constants - included by both ChunkRenderer and
// WorldGen

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y \
  64 // 64 blocks height - enough for normal hills
#define CHUNK_SIZE_Z 16

#define WORLD_CHUNKS_X 8 // 8x8=64 chunks, vertex bufs ~14MB total
#define WORLD_CHUNKS_Z 8
