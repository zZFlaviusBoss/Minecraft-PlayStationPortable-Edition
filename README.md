# Minecraft PSP Edition

## Overview
**Minecraft PSP Edition** is an ambitious project aiming to bring the authentic *Minecraft Console Edition* experience to the PlayStation Portable (PSP). Unlike other homebrew versions, this port is structurally inspired by the **4J Studios** codebase, ensuring highly optimized rendering, smooth day/night transitions, and a genuine console feel.

We are pushing the PSP hardware to its absolute limits to achieve a stable **60 FPS** while maintaining faithful visuals, including dynamic hardware lighting, LOD leaf culling, and smooth sun/moon cycles.

## Features (Currently Implemented)
- **4J Studios Architecture**: Rebuilt chunk rendering pipeline inspired by the Xbox 360/PS3 Console Editions.
- **Dynamic Hardware Lighting**: Seamless day/night cycle utilizing the PSP's `sceGuAmbient` engine for instantaneous global illumination.
- **Stable 60 FPS Performance**: Heavy optimizations including cubic 3D frustum culling, transparent leaf LOD drops, and custom mem-aligned VBOs.
- **Accurate Skybox & Environment**: Fully working procedural starfield, sun/moon phases, and horizon fog color blending.

## How to Play
To play this on real hardware, you will need:
- A PlayStation Portable (PSP 1000, 2000, 3000, or Go) running **Custom Firmware (CFW)**.
- For Emulators: You can use [PPSSPP](https://www.ppsspp.org/) on PC, Android, or iOS.

*Instructions on compiling the `EBOOT.PBP` are provided below.*

## Building from Source
If you want to compile the game yourself, you need the official PSP SDK set up on your machine.
1. Clone the repository.
2. Ensure `psp-gcc` and the PSPSDK environment variables are correctly configured.
3. Ensure you have `CMake` greater than `3.16` and correctly configured.
4. Simply run:
```bash
make
```
5. Copy the resulting `build/EBOOT.PBP` alongside the `build/res/` folder to your PSP (`ms0:/PSP/GAME/MinecraftPSP/`).

## Contribute
**We need your help!** 
Are you a C++ developer? Do you know MIPS, PSP GU hardware programming, or just want to help with math, physics, or rendering? 
This is a massive passion project and **anyone who wants to contribute is welcome!** Feel free to fork the repository, submit Pull Requests, or open Issues with ideas and bug reports.

**Note on AI Tools:** Working with LLMs (ChatGPT, Claude, etc.) to write code is completely acceptable and welcome, as long as it's not the biggest "slop" possible. Please review, test, and understand the code you submit before opening a Pull Request.

## Contributors
- [Gabriel Araujo (dotrujos)](https://github.com/dotrujos) - Docker devcontainer support

## Disclaimer
This project is made for educational, technical, and preservation purposes only. All rights, trademarks, and copyright to *Minecraft* belong to **Mojang Studios** and **Microsoft**. 
