# Minecraft PSP Port — Makefile
# Target: PSP (MIPS-allegrex, >=PSP-2000 cu 64MB RAM)
# Compilator: psp-gcc via PSPSDK

TARGET = MinecraftPSP
PSPSDK = $(shell psp-config --pspsdk-path)

# Metadata EBOOT
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE  = Minecraft PSP
# PSP_EBOOT_ICON   = res/ICON0.PNG

# Surse
SRCS = src/main.cpp \
       src/world/Random.cpp \
       src/world/Mth.cpp \
       src/world/Vec3.cpp \
       src/world/AABB.cpp \
       src/world/Blocks.cpp \
       src/world/NoiseGen.cpp \
       src/world/TreeFeature.cpp \
       src/world/WorldGen.cpp \
       src/world/Chunk.cpp \
       src/world/Level.cpp \
       src/world/Raycast.cpp \
       src/render/PSPRenderer.cpp \
       src/render/ChunkRenderer.cpp \
       src/render/TextureAtlas.cpp \
       src/render/Tesselator.cpp \
       src/render/TileRenderer.cpp \
       src/render/SkyRenderer.cpp \
       src/render/CloudRenderer.cpp \
       src/render/BlockHighlight.cpp \
       src/math/Frustum.cpp \
       src/input/PSPInput.cpp

OBJS = $(SRCS:.cpp=.o)

# Flags
CXXFLAGS = -O2 -G0 -Wall \
           -I$(PSPSDK)/include \
           -Isrc \
           -std=c++11 \
           -fno-exceptions \
           -fno-rtti \
           -DPSP \
           -Wno-misleading-indentation \
           -Wno-unused-function

PSP_FW_VERSION = 600

# Librarii
LIBS = -lstdc++ \
       -lpspgum -lpspgu \
       -lpspaudiolib -lpspaudio \
       -lpsprtc \
       -lpsppower \
       -lm

# Build
include $(PSPSDK)/lib/build.mak
