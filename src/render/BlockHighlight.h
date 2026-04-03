#pragma once
#include <stdint.h>

// Draws a wireframe cube around the targeted block, using its specific bounding box
void BlockHighlight_Draw(int bx, int by, int bz, uint8_t blockId);
