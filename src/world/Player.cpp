#include "Player.h"
#include "input/PSPInput.h"
#include "world/AABB.h"
#include "world/Blocks.h"
#include "world/Mth.h"
#include <math.h>

Player::Player(Level* level)
    : level(level), x(0.0f), y(0.0f), z(0.0f), yaw(0.0f), pitch(0.0f), velY(0.0f),
      onGround(false), isFlying(false), jumpDoubleTapTimer(0.0f),
      heldBlock(BLOCK_COBBLESTONE), breakCooldown(0.0f) {
}

Player::~Player() {}

void Player::spawn(float startX, float startY, float startZ) {
    x = startX;
    y = startY;
    z = startZ;
    yaw = 0.0f;
    pitch = 0.0f;
    velY = 0.0f;
    onGround = false;
    isFlying = false;
    jumpDoubleTapTimer = 0.0f;
    breakCooldown = 0.0f;
}

void Player::update(float dt) {
    updateInputAndPhysics(dt);
    updateInteraction(dt);
}

void Player::updateInputAndPhysics(float dt) {
    // Check if player is in water
    uint8_t feetBlock = level->getBlock((int)floorf(x), (int)floorf(y), (int)floorf(z));
    uint8_t headBlock = level->getBlock((int)floorf(x), (int)floorf(y + 1.6f), (int)floorf(z));
    bool inWater = g_blockProps[feetBlock].isLiquid() || g_blockProps[headBlock].isLiquid();

    float moveSpeed = (isFlying ? 10.0f : 5.0f) * dt;
    if (inWater && !isFlying) moveSpeed *= 0.6f;

    float lookSpeed = 120.0f * dt;

    // Rotation with right stick (Face Buttons)
    float lx = PSPInput_StickX(1);
    float ly = PSPInput_StickY(1);
    yaw += lx * lookSpeed;
    pitch += ly * lookSpeed;
    pitch = Mth::clamp(pitch, -89.0f, 89.0f);

    // Movement with left stick (Analog)
    float fx = -PSPInput_StickX(0);
    float fz = -PSPInput_StickY(0);

    float yawRad = yaw * Mth::DEGRAD;

    float dx = (fx * Mth::cos(yawRad) + fz * Mth::sin(yawRad)) * moveSpeed;
    float dz = (-fx * Mth::sin(yawRad) + fz * Mth::cos(yawRad)) * moveSpeed;

    const float R = 0.3f;   // 4J: setSize(0.6, 1.8)
    const float H = 1.8f;   // 4J: player bounding box height

    // Vertical movement
    float dy = 0.0f;
    if (isFlying) {
        float flySpeed = 10.0f * dt;
        if (PSPInput_IsHeld(PSP_CTRL_SELECT))
            dy = flySpeed;  // Ascend
        if (PSPInput_IsHeld(PSP_CTRL_DOWN))
            dy = -flySpeed;  // Descend
        velY = 0.0f;
    } else if (inWater) {
        // Sink/Float physics
        if (PSPInput_IsHeld(PSP_CTRL_SELECT)) {
            velY += 10.0f * dt; // Float up
            if (velY > 3.0f) velY = 3.0f;
        } else {
            velY -= 5.0f * dt; // Sink slowly
            if (velY < -2.0f) velY = -2.0f;
        }
        dy = velY * dt;
    } else {
        velY -= 20.0f * dt;
        dy = velY * dt;
    }

    // Collision
    AABB player_aabb(x - R, y, z - R, x + R, y + H, z + R);

    AABB* expanded = player_aabb.expand(dx, dy, dz);
    std::vector<AABB> cubes = level->getCubes(*expanded);
    delete expanded;

    float dy_org = dy;
    for (auto& cube : cubes) dy = cube.clipYCollide(&player_aabb, dy);
    player_aabb.move(0, dy, 0);

    for (auto& cube : cubes) dx = cube.clipXCollide(&player_aabb, dx);
    player_aabb.move(dx, 0, 0);

    for (auto& cube : cubes) dz = cube.clipZCollide(&player_aabb, dz);
    player_aabb.move(0, 0, dz);

    onGround = (dy_org != dy && dy_org < 0.0f);
    if (onGround || dy_org != dy) {
        velY = 0.0f;
    }

    x = (player_aabb.x0 + player_aabb.x1) / 2.0f;
    y = player_aabb.y0;
    z = (player_aabb.z0 + player_aabb.z1) / 2.0f;

    // Enforce world bounds natively
    const float WORLD_MAX_X = (float)(WORLD_CHUNKS_X * CHUNK_SIZE_X - 1);
    const float WORLD_MAX_Z = (float)(WORLD_CHUNKS_Z * CHUNK_SIZE_Z - 1);
    if (x < 0.5f) x = 0.5f;
    if (x > WORLD_MAX_X) x = WORLD_MAX_X;
    if (z < 0.5f) z = 0.5f;
    if (z > WORLD_MAX_Z) z = WORLD_MAX_Z;

    // Controls: Jump/Fly
    static const float DOUBLE_TAP_WINDOW = 0.35f;
    if (jumpDoubleTapTimer > 0.0f)
        jumpDoubleTapTimer -= dt;

    if (PSPInput_JustPressed(PSP_CTRL_SELECT)) {
        if (jumpDoubleTapTimer > 0.0f) {
            isFlying = !isFlying;
            velY = 0.0f;
            jumpDoubleTapTimer = 0.0f;
        } else {
            if (!isFlying && !inWater && onGround) {
                velY = 6.7f;
                onGround = false;
            }
            jumpDoubleTapTimer = DOUBLE_TAP_WINDOW;
        }
    }
}

void Player::updateInteraction(float dt) {
    float yawRad = yaw * Mth::DEGRAD;

    // Raycast block target
    {
        float eyeX = x;
        float eyeY = y + 1.6f;
        float eyeZ = z;
        float pitchRad = pitch * Mth::DEGRAD;
        float dirX = Mth::sin(yawRad) * Mth::cos(pitchRad);
        float dirY = Mth::sin(pitchRad);
        float dirZ = Mth::cos(yawRad) * Mth::cos(pitchRad);
        hitResult = raycast(level, eyeX, eyeY, eyeZ, dirX, dirY, dirZ, 5.0f);
    }

    // Block action cooldown
    if (breakCooldown > 0.0f) breakCooldown -= dt;

    // block break timer
    bool doBreak = false;

    // regular mc hold delay (5 ticks = 250ms)
    if (PSPInput_IsHeld(PSP_CTRL_RTRIGGER) && breakCooldown <= 0.0f && hitResult.hit) {
        doBreak = true;
        breakCooldown = 0.25f; 
    }

    if (doBreak) {
        uint8_t oldBlock = level->getBlock(hitResult.x, hitResult.y, hitResult.z);
        if (oldBlock != BLOCK_BEDROCK) {
            int bx = hitResult.x, by = hitResult.y, bz = hitResult.z;
            level->setBlock(bx, by, bz, BLOCK_AIR);
            
            // mark chunks dirty async so we don't lag
            // BREAKING: Neighbors build FIRST to draw their newly exposed faces. Main block builds AFTER.
            level->markDirty(bx, by, bz, 10); 
            level->markDirty(bx - 1, by, bz, ((bx & 0xF) == 0) ? 20 : 0, false);
            level->markDirty(bx + 1, by, bz, ((bx & 0xF) == 15) ? 20 : 0, false);
            level->markDirty(bx, by - 1, bz, ((by & 0xF) == 0) ? 20 : 0, false);
            level->markDirty(bx, by + 1, bz, ((by & 0xF) == 15) ? 20 : 0, false);
            level->markDirty(bx, by, bz - 1, ((bz & 0xF) == 0) ? 20 : 0, false);
            level->markDirty(bx, by, bz + 1, ((bz & 0xF) == 15) ? 20 : 0, false);

            // Cascading plant break
            uint8_t topId = level->getBlock(bx, by + 1, bz);
            if (topId != BLOCK_AIR && !g_blockProps[topId].isSolid() && !g_blockProps[topId].isLiquid()) {
                level->setBlock(bx, by + 1, bz, BLOCK_AIR);
                level->markDirty(bx, by + 1, bz);
            }
        }
    }

    // Place block
    if (PSPInput_JustPressed(PSP_CTRL_LTRIGGER) && hitResult.hit) {
        int px = hitResult.nx;
        int py = hitResult.ny;
        int pz = hitResult.nz;

        uint8_t hitId = level->getBlock(hitResult.x, hitResult.y, hitResult.z);
        if (hitId != BLOCK_AIR && !g_blockProps[hitId].isSolid() && !g_blockProps[hitId].isLiquid()) {
            px = hitResult.x;
            py = hitResult.y;
            pz = hitResult.z;
        }

        bool canPlace = true;
        if (heldBlock == BLOCK_SAPLING || heldBlock == BLOCK_TALLGRASS || heldBlock == BLOCK_FLOWER || 
            heldBlock == BLOCK_ROSE || heldBlock == BLOCK_MUSHROOM_BROWN || heldBlock == BLOCK_MUSHROOM_RED) {
            uint8_t floorId = level->getBlock(px, py - 1, pz);
            if (floorId != BLOCK_GRASS && floorId != BLOCK_DIRT && floorId != BLOCK_FARMLAND) {
                canPlace = false;
            }
        }

        const float R = 0.3f;
        const float H = 1.8f;
        int playerMinX = (int)floorf(x - R);
        int playerMaxX = (int)floorf(x + R);
        int playerMinY = (int)floorf(y);
        int playerMaxY = (int)floorf(y + H);
        int playerMinZ = (int)floorf(z - R);
        int playerMaxZ = (int)floorf(z + R);

        bool overlaps = (px >= playerMinX && px <= playerMaxX &&
                         py >= playerMinY && py <= playerMaxY &&
                         pz >= playerMinZ && pz <= playerMaxZ);

        uint8_t targetBlock = level->getBlock(px, py, pz);
        bool canReplaceTarget = (targetBlock == BLOCK_AIR || (!g_blockProps[targetBlock].isSolid() && !g_blockProps[targetBlock].isLiquid()));

        if (canPlace && !overlaps && canReplaceTarget) {
            level->setBlock(px, py, pz, heldBlock);
            
            level->markDirty(px, py, pz, 20); // Priority 20 ensures main block builds BEFORE neighbors
            level->markDirty(px - 1, py, pz, ((px & 0xF) == 0) ? 10 : 0, false);
            level->markDirty(px + 1, py, pz, ((px & 0xF) == 15) ? 10 : 0, false);
            level->markDirty(px, py - 1, pz, ((py & 0xF) == 0) ? 10 : 0, false);
            level->markDirty(px, py + 1, pz, ((py & 0xF) == 15) ? 10 : 0, false);
            level->markDirty(px, py, pz - 1, ((pz & 0xF) == 0) ? 10 : 0, false);
            level->markDirty(px, py, pz + 1, ((pz & 0xF) == 15) ? 10 : 0, false);
        }
    }

    // Cycle hotbar
    static const uint8_t PLACEABLE[] = {
        BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT, BLOCK_COBBLESTONE,
        BLOCK_WOOD_PLANK, BLOCK_SAND, BLOCK_GRAVEL, BLOCK_LOG,
        BLOCK_LEAVES, BLOCK_GLASS, BLOCK_SANDSTONE, BLOCK_WOOL,
        BLOCK_GOLD_BLOCK, BLOCK_IRON_BLOCK, BLOCK_BRICK,
        BLOCK_BOOKSHELF, BLOCK_MOSSY_COBBLE, BLOCK_OBSIDIAN,
        BLOCK_GLOWSTONE, BLOCK_PUMPKIN,
        BLOCK_FLOWER, BLOCK_ROSE, BLOCK_SAPLING, BLOCK_TALLGRASS
    };
    static const int NUM_PLACEABLE = sizeof(PLACEABLE) / sizeof(PLACEABLE[0]);
    
    // Find index of held block
    int placeIdx = 3;
    for (int i = 0; i < NUM_PLACEABLE; i++) {
        if (PLACEABLE[i] == heldBlock) {
            placeIdx = i;
            break;
        }
    }

    if (PSPInput_JustPressed(PSP_CTRL_RIGHT)) {
        placeIdx = (placeIdx + 1) % NUM_PLACEABLE;
        heldBlock = PLACEABLE[placeIdx];
    }
    if (PSPInput_JustPressed(PSP_CTRL_LEFT)) {
        placeIdx = (placeIdx - 1 + NUM_PLACEABLE) % NUM_PLACEABLE;
        heldBlock = PLACEABLE[placeIdx];
    }
}
