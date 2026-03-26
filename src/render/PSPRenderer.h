#pragma once
#include <pspgu.h>
#include <pspmath.h>

// Basic sceGu wrapper

bool PSPRenderer_Init();
void PSPRenderer_BeginFrame(uint32_t skyColor, float fogNear, float fogFar, uint32_t fogColor, float fov);
void PSPRenderer_SetCamera(const ScePspFVector3 *eye,
                           const ScePspFVector3 *center);

void PSPRenderer_GetViewProjMatrix(ScePspFMatrix4 *outVP);

void PSPRenderer_EndFrame();
void PSPRenderer_Shutdown();
