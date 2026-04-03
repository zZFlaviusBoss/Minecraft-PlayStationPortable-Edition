// PSPInput.cpp - controller input with sceCtrl

#include "PSPInput.h"
#include <pspctrl.h>
#include <string.h>

static SceCtrlData g_pad;
static SceCtrlData g_padPrev;

// Analog deadzone
static const float DEADZONE = 0.15f;

void PSPInput_Update() {
  g_padPrev = g_pad;
  sceCtrlSetSamplingCycle(0);
  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  sceCtrlReadBufferPositive(&g_pad, 1);
}

static float normalize_axis(unsigned char raw) {
  // sceCtrl gives values 0-255, center ~128
  float v = ((float)raw - 128.0f) / 128.0f;
  if (v > -DEADZONE && v < DEADZONE)
    return 0.0f;
  return v;
}

float PSPInput_StickX(int idx) {
  if (idx == 0)
    return normalize_axis(g_pad.Lx);
  // Right stick X simulated with Square/Circle Face buttons
  float v = 0.0f;
  if (g_pad.Buttons & PSP_CTRL_CIRCLE)
    v -= 1.0f; // Look Right
  if (g_pad.Buttons & PSP_CTRL_SQUARE)
    v += 1.0f; // Look Left
  return v;
}

float PSPInput_StickY(int idx) {
  if (idx == 0)
    return normalize_axis(g_pad.Ly);
  // Right stick Y simulated with Triangle/Cross Face buttons
  float v = 0.0f;
  if (g_pad.Buttons & PSP_CTRL_TRIANGLE)
    v += 1.0f; // Look Up
  if (g_pad.Buttons & PSP_CTRL_CROSS)
    v -= 1.0f; // Look Down
  return v;
}

bool PSPInput_IsHeld(unsigned int button) {
  return (g_pad.Buttons & button) != 0;
}

bool PSPInput_JustPressed(unsigned int button) {
  return (g_pad.Buttons & button) != 0 && (g_padPrev.Buttons & button) == 0;
}

bool PSPInput_JustReleased(unsigned int button) {
  return (g_pad.Buttons & button) == 0 && (g_padPrev.Buttons & button) != 0;
}
