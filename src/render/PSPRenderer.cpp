// PSPRenderer.cpp - sceGu wrapper

#include "PSPRenderer.h"
#include <malloc.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <string.h>

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

// Display list - must be in uncached RAM
static unsigned int __attribute__((aligned(16))) g_list[262144];

// Frame + depth buffers in VRAM
static void *g_fbp0; // frame buffer 0
static void *g_fbp1; // frame buffer 1
static void *g_zbp;  // depth buffer

// VRAM offset calculation
static void *vrelptr(unsigned int offset) {
  return (void *)(0x44000000 + offset);
}

bool PSPRenderer_Init() {
  // Frame buffer: 512*272*4 bytes = 557056 bytes
  g_fbp0 = vrelptr(0);
  g_fbp1 = vrelptr(512 * 272 * 4);
  g_zbp = vrelptr(2 * 512 * 272 * 4);

  sceGuInit();
  sceGuStart(GU_DIRECT, g_list);

  // Display
  sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_WIDTH);
  sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void *)(512 * 272 * 4), BUF_WIDTH);
  sceGuDepthBuffer((void *)(2 * 512 * 272 * 4), BUF_WIDTH);

  sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
  // Overscan hack: Add 4 pixels to viewport to hide rasterizer gaps at the
  // screen edge
  sceGuViewport(2048, 2048, SCR_WIDTH + 4, SCR_HEIGHT + 4);
  sceGuDepthRange(50000, 10000); 
  
  // ScissorJKSFJKFDJKFDSJKFDJK
  sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
  sceGuEnable(GU_SCISSOR_TEST);

  // Depth test
  sceGuDepthFunc(GU_GEQUAL);
  sceGuEnable(GU_DEPTH_TEST);

  // Textures
  sceGuEnable(GU_TEXTURE_2D);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); // vertex color * texture = final
  sceGuTexFilter(GU_NEAREST, GU_NEAREST);     // pixel-art style

  // Smooth Shading for Ambient Occlusion
  sceGuShadeModel(GU_SMOOTH);

  // Hardware clipping for triangles passing the screen edge
  // Prevents entire cubes from disappearing when close to a corner
  sceGuEnable(GU_CLIP_PLANES);

  // Backface culling
  sceGuFrontFace(GU_CCW);
  sceGuEnable(GU_CULL_FACE);

  // Alpha test (for plants/transparents)
  sceGuAlphaFunc(GU_GREATER, 0x80, 0xFF);
  sceGuEnable(GU_ALPHA_TEST);

  // Fog: Minecraft blue color, distance 40-64 blocks
  sceGuFog(40.0f, 64.0f, 0xFFFFB267); // ABGR: R=0x67,G=0xB2,B=0xFF -> sky blue
  sceGuEnable(GU_FOG);

  sceGuFinish();
  sceGuSync(0, 0);

  sceDisplayWaitVblankStart();
  sceGuDisplay(GU_TRUE);

  return true;
}

void PSPRenderer_BeginFrame(uint32_t skyColor) {
  sceGuStart(GU_DIRECT, g_list);

  // Clear to dynamic sky color (updated per-frame by SkyRenderer)
  sceGuClearColor(skyColor);
  sceGuClearDepth(0);
  sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

  // Update fog to match sky color dynamically
  sceGuFog(40.0f, 64.0f, skyColor);

  // Projection matrix
  sceGumMatrixMode(GU_PROJECTION);
  sceGumLoadIdentity();
  sceGumPerspective(70.0f, 480.0f / 272.0f, 0.21f, 1000.0f);

  sceGumMatrixMode(GU_VIEW);
  sceGumLoadIdentity();

  sceGumMatrixMode(GU_MODEL);
  sceGumLoadIdentity();
}

void PSPRenderer_SetCamera(const ScePspFVector3 *eye,
                           const ScePspFVector3 *center) {
  sceGumMatrixMode(GU_VIEW);
  sceGumLoadIdentity();
  ScePspFVector3 up = {0.0f, 1.0f, 0.0f};
  // sceGumLookAt does not accept const - const_cast required
  sceGumLookAt(const_cast<ScePspFVector3 *>(eye),
               const_cast<ScePspFVector3 *>(center), &up);
  sceGumUpdateMatrix(); // IMPORTANT: This ensures the view matrix is
                        // mathematically updated before we read it!
  sceGumMatrixMode(GU_MODEL);
}

void PSPRenderer_GetViewProjMatrix(ScePspFMatrix4 *outVP) {
  ScePspFMatrix4 proj, view;

  sceGumMatrixMode(GU_PROJECTION);
  sceGumStoreMatrix(&proj);

  sceGumMatrixMode(GU_VIEW);
  sceGumStoreMatrix(&view);

  // P * V
  gumMultMatrix(outVP, &proj, &view);

  // Restore
  sceGumMatrixMode(GU_MODEL);
}

void PSPRenderer_EndFrame() {
  sceGuFinish();
  sceGuSync(0, 0);
  sceDisplayWaitVblankStart();
  sceGuSwapBuffers();
}

void PSPRenderer_Shutdown() { sceGuTerm(); }
