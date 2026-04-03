#include "Texture.h"
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "../stb_image.h"

Texture::Texture() : vramPtr(nullptr), width(0), height(0),
                     origWidth(0), origHeight(0), isBound(false) {}

Texture::~Texture() { free(); }

static unsigned char *read_file(const char *path, int *out_size) {
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) return nullptr;
    int size = (int)sceIoLseek(fd, 0, PSP_SEEK_END);
    sceIoLseek(fd, 0, PSP_SEEK_SET);
    if (size <= 0 || size > 4 * 1024 * 1024) { sceIoClose(fd); return nullptr; }
    unsigned char *buf = (unsigned char *)malloc(size);
    if (!buf) { sceIoClose(fd); return nullptr; }
    sceIoRead(fd, buf, (SceSize)size);
    sceIoClose(fd);
    if (out_size) *out_size = size;
    return buf;
}

static int nextPow2(int v) {
    v--; v |= v>>1; v |= v>>2; v |= v>>4; v |= v>>8; v |= v>>16; v++;
    return v;
}

bool Texture::load(const char *path) {
    free();
    int fileSize = 0;
    unsigned char *fileData = read_file(path, &fileSize);
    if (!fileData) return false;

    int imgW, imgH, channels;
    unsigned char *pixels = stbi_load_from_memory(fileData, fileSize, &imgW, &imgH, &channels, 4);
    ::free(fileData);
    if (!pixels) return false;

    origWidth  = imgW;
    origHeight = imgH;

    int p2w = nextPow2(imgW);
    int p2h = nextPow2(imgH);

    vramPtr = memalign(16, p2w * p2h * 4);
    if (!vramPtr) { stbi_image_free(pixels); return false; }

    memset(vramPtr, 0, p2w * p2h * 4);
    for (int y = 0; y < imgH; y++)
        memcpy((unsigned char*)vramPtr + y * p2w * 4, pixels + y * imgW * 4, imgW * 4);

    stbi_image_free(pixels);
    width  = p2w;
    height = p2h;
    sceKernelDcacheWritebackAll();
    return true;
}

void Texture::bind() {
    if (!vramPtr) return;
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, width, height, width, vramPtr);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    isBound = true;
}

void Texture::free() {
    if (vramPtr) { ::free(vramPtr); vramPtr = nullptr; }
    width = height = origWidth = origHeight = 0;
    isBound = false;
}
