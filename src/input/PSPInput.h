#pragma once
#include <pspctrl.h>

// PSP Input via sceCtrl

void PSPInput_Update();

// Analog sticks: idx=0 left, idx=1 right (simulated with buttons on PSP without right stick)
float PSPInput_StickX(int idx);
float PSPInput_StickY(int idx);

// Buttons
bool PSPInput_IsHeld(unsigned int button);
bool PSPInput_JustPressed(unsigned int button);
bool PSPInput_JustReleased(unsigned int button);
