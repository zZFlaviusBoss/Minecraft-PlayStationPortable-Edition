#pragma once
#include <stdint.h>

class Texture {
public:
    Texture();
    ~Texture();

    bool load(const char *path);
    void bind();
    void free();

    void *vramPtr;
    int width;      // power-of-2 padded width  (for sceGuTexImage)
    int height;     // power-of-2 padded height
    int origWidth;  // actual image width  in pixels
    int origHeight; // actual image height in pixels
    bool isBound;
};
