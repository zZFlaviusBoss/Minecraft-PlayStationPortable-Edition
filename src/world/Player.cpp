#include "Player.h"
#include "input/PSPInput.h"
#include "world/AABB.h"
#include "world/Blocks.h"
#include "world/Mth.h"
#include <math.h>

static inline bool isBottomSlabId(uint8_t id) {
    return id == BLOCK_STONE_SLAB || id == BLOCK_WOOD_SLAB || id == BLOCK_COBBLE_SLAB ||
           id == BLOCK_SANDSTONE_SLAB || id == BLOCK_BRICK_SLAB || id == BLOCK_STONE_BRICK_SLAB;
}

static inline bool isTopSlabId(uint8_t id) {
    return id == BLOCK_STONE_SLAB_TOP || id == BLOCK_WOOD_SLAB_TOP || id == BLOCK_COBBLE_SLAB_TOP ||
           id == BLOCK_SANDSTONE_SLAB_TOP || id == BLOCK_BRICK_SLAB_TOP || id == BLOCK_STONE_BRICK_SLAB_TOP;
}

static inline uint8_t toTopSlabId(uint8_t id) {
    switch (id) {
        case BLOCK_STONE_SLAB: return BLOCK_STONE_SLAB_TOP;
        case BLOCK_WOOD_SLAB: return BLOCK_WOOD_SLAB_TOP;
        case BLOCK_COBBLE_SLAB: return BLOCK_COBBLE_SLAB_TOP;
        case BLOCK_SANDSTONE_SLAB: return BLOCK_SANDSTONE_SLAB_TOP;
        case BLOCK_BRICK_SLAB: return BLOCK_BRICK_SLAB_TOP;
        case BLOCK_STONE_BRICK_SLAB: return BLOCK_STONE_BRICK_SLAB_TOP;
        default: return id;
    }
}

static inline uint8_t slabToFullBlock(uint8_t id) {
    switch (id) {
        case BLOCK_STONE_SLAB:
        case BLOCK_STONE_SLAB_TOP: return BLOCK_STONE;
        case BLOCK_WOOD_SLAB:
        case BLOCK_WOOD_SLAB_TOP: return BLOCK_WOOD_PLANK;
        case BLOCK_COBBLE_SLAB:
        case BLOCK_COBBLE_SLAB_TOP: return BLOCK_COBBLESTONE;
        case BLOCK_SANDSTONE_SLAB:
        case BLOCK_SANDSTONE_SLAB_TOP: return BLOCK_SANDSTONE;
        case BLOCK_BRICK_SLAB:
        case BLOCK_BRICK_SLAB_TOP: return BLOCK_BRICK;
        case BLOCK_STONE_BRICK_SLAB:
        case BLOCK_STONE_BRICK_SLAB_TOP: return BLOCK_STONE_BRICKS;
        default: return BLOCK_AIR;
    }
}

Player::Player(Level* level)
    : level(level), x(0.0f), y(0.0f), z(0.0f), yaw(0.0f), pitch(0.0f), velX(0.0f), velZ(0.0f), velY(0.0f),
      onGround(false), isFlying(false), jumpDoubleTapTimer(0.0f),
      sprinting(false), sprintDoubleTapTimer(0.0f), prevForwardHeld(false), wasInWater(false),
      prevAutoJumpCollision(false),
      heldBlock(BLOCK_COBBLESTONE), breakCooldown(0.0f) {
}

Player::~Player() {}

void Player::spawn(float startX, float startY, float startZ) {
    x = startX;
    y = startY;
    z = startZ;
    yaw = 0.0f;
    pitch = 0.0f;
    velX = 0.0f;
    velZ = 0.0f;
    velY = 0.0f;
    onGround = false;
    isFlying = false;
    jumpDoubleTapTimer = 0.0f;
    sprinting = false;
    sprintDoubleTapTimer = 0.0f;
    prevForwardHeld = false;
    wasInWater = false;
    prevAutoJumpCollision = false;
    breakCooldown = 0.0f;
}

void Player::moveRelative(float xa, float za, float speed) {
    float dist = sqrtf(xa * xa + za * za);
    if (dist < 0.01f) return;
    if (dist < 1.0f) dist = 1.0f;
    float inv = speed / dist;
    xa *= inv;
    za *= inv;

    float yawRad = yaw * Mth::DEGRAD;
    float s = Mth::sin(yawRad);
    float c = Mth::cos(yawRad);

    // Match this project's world/camera convention:
    // forward input must follow lookDir = (sin(yaw), cos(yaw)) on X/Z.
    velX += xa * c + za * s;
    velZ += -xa * s + za * c;
}

void Player::update(float dt) {
    updateInputAndPhysics(dt);
    updateInteraction(dt);
}

void Player::updateInputAndPhysics(float dt) {
    // Open creative inventory with L+R combo.
    bool lHeld = PSPInput_IsHeld(PSP_CTRL_LTRIGGER);
    bool rHeld = PSPInput_IsHeld(PSP_CTRL_RTRIGGER);
    if (!creativeInv.isOpen() && lHeld && rHeld &&
        (PSPInput_JustPressed(PSP_CTRL_LTRIGGER) || PSPInput_JustPressed(PSP_CTRL_RTRIGGER))) {
        creativeInv.open();
    }

    if (creativeInv.isOpen()) {
        if (PSPInput_JustPressed(PSP_CTRL_CIRCLE)) {
            if (creativeInv.cursorHasItem()) creativeInv.clearCursorSelection();
            else creativeInv.close();
        }
        if (PSPInput_JustPressed(PSP_CTRL_LTRIGGER) && !rHeld) creativeInv.prevCategory();
        if (PSPInput_JustPressed(PSP_CTRL_RTRIGGER) && !lHeld) creativeInv.nextCategory();
        if (PSPInput_JustPressed(PSP_CTRL_RIGHT)) creativeInv.moveRight();
        if (PSPInput_JustPressed(PSP_CTRL_LEFT)) creativeInv.moveLeft();
        if (PSPInput_JustPressed(PSP_CTRL_UP)) creativeInv.moveUp();
        if (PSPInput_JustPressed(PSP_CTRL_DOWN)) creativeInv.moveDown();
        if (PSPInput_JustPressed(PSP_CTRL_CROSS)) creativeInv.pressCross();
        heldBlock = creativeInv.heldBlock();
        return;
    }

    if (PSPInput_JustPressed(PSP_CTRL_RIGHT)) creativeInv.cycleHotbarRight();
    if (PSPInput_JustPressed(PSP_CTRL_LEFT)) creativeInv.cycleHotbarLeft();
    heldBlock = creativeInv.heldBlock();

    float lookSpeed = 120.0f * dt;

    // Rotation with right stick (Face Buttons)
    float lx = PSPInput_StickX(1);
    float ly = PSPInput_StickY(1);
    yaw += lx * lookSpeed;
    pitch += ly * lookSpeed;
    pitch = Mth::clamp(pitch, -89.0f, 89.0f);

    // Movement with left stick (Analog)
    float xa = -PSPInput_StickX(0);
    float ya = -PSPInput_StickY(0);
    if (fabsf(xa) < 0.10f) xa = 0.0f;
    if (fabsf(ya) < 0.10f) ya = 0.0f;

    // MCPE-like sprint via quick forward double-tap.
    // 7 ticks ~= 0.35s at 20 TPS.
    const float SPRINT_TAP_WINDOW = 0.35f;
    bool forwardHeld = (ya > 0.75f);
    if (forwardHeld && !prevForwardHeld) {
        if (sprintDoubleTapTimer > 0.0f) sprinting = true;
        else sprintDoubleTapTimer = SPRINT_TAP_WINDOW;
    }
    if (!forwardHeld) sprinting = false;
    prevForwardHeld = forwardHeld;
    if (sprintDoubleTapTimer > 0.0f) sprintDoubleTapTimer -= dt;
    if (isFlying) sprinting = false;

    const float R = 0.3f;   // 4J: setSize(0.6, 1.8)
    const float H = 1.8f;   // 4J: player bounding box height

    // Check if player is in water
    uint8_t feetBlock = level->getBlock((int)floorf(x), (int)floorf(y), (int)floorf(z));
    uint8_t headBlock = level->getBlock((int)floorf(x), (int)floorf(y + 1.6f), (int)floorf(z));
    bool inWater = g_blockProps[feetBlock].isLiquid() || g_blockProps[headBlock].isLiquid();
    AABB waterCheckBox(x - R, y, z - R, x + R, y + H, z + R);
    inWater = level->applyWaterCurrent(waterCheckBox, velX, velY, velZ) || inWater;
    if (!inWater && wasInWater && velY > 0.42f) {
        velY = 0.42f; // prevent launch when crossing water surface
    }

    // Controls: Jump/Fly toggle (double tap jump button)
    static const float DOUBLE_TAP_WINDOW = 0.35f;
    if (jumpDoubleTapTimer > 0.0f) jumpDoubleTapTimer -= dt;

    if (PSPInput_JustPressed(PSP_CTRL_SELECT)) {
        if (jumpDoubleTapTimer > 0.0f) {
            isFlying = !isFlying;
            velX = velZ = velY = 0.0f;
            sprinting = false;
            jumpDoubleTapTimer = 0.0f;
        } else {
            if (!isFlying && !inWater && onGround) {
                velY = 0.42f; // MCPE/MC jump impulse
                onGround = false;
            }
            jumpDoubleTapTimer = DOUBLE_TAP_WINDOW;
        }
    }

    if (isFlying) {
        prevAutoJumpCollision = false;
        float flySpeed = 10.0f * dt * (sprinting ? 1.3f : 1.0f);
        float yawRad = yaw * Mth::DEGRAD;
        float dx = (xa * Mth::cos(yawRad) + ya * Mth::sin(yawRad)) * flySpeed;
        float dz = (-xa * Mth::sin(yawRad) + ya * Mth::cos(yawRad)) * flySpeed;
        float dy = 0.0f;
        if (PSPInput_IsHeld(PSP_CTRL_SELECT))
            dy = flySpeed;  // Ascend
        if (PSPInput_IsHeld(PSP_CTRL_DOWN))
            dy = -flySpeed;  // Descend
        AABB player_aabb(x - R, y, z - R, x + R, y + H, z + R);
        AABB* expanded = player_aabb.expand(dx, dy, dz);
        std::vector<AABB> cubes = level->getCubes(*expanded);
        delete expanded;

        float dyOrg = dy;
        for (auto& cube : cubes) dy = cube.clipYCollide(&player_aabb, dy);
        player_aabb.move(0, dy, 0);
        for (auto& cube : cubes) dx = cube.clipXCollide(&player_aabb, dx);
        player_aabb.move(dx, 0, 0);
        for (auto& cube : cubes) dz = cube.clipZCollide(&player_aabb, dz);
        player_aabb.move(0, 0, dz);

        onGround = (dyOrg != dy && dyOrg < 0.0f);
        x = (player_aabb.x0 + player_aabb.x1) * 0.5f;
        y = player_aabb.y0;
        z = (player_aabb.z0 + player_aabb.z1) * 0.5f;
    } else {
        float ticksLeft = dt * 20.0f; // MCPE physics is tuned for 20 TPS
        if (ticksLeft < 0.0f) ticksLeft = 0.0f;
        while (ticksLeft > 0.0f) {
            float step = (ticksLeft > 1.0f) ? 1.0f : ticksLeft;
            ticksLeft -= step;

            float sprintMul = sprinting ? 1.3f : 1.0f;
            if (inWater) {
                if (PSPInput_IsHeld(PSP_CTRL_SELECT)) {
                    velY += 0.04f * step; // hold jump to keep swimming up
                    if (velY > 0.3f) velY = 0.3f;
                }
                moveRelative(xa, ya, 0.02f * sprintMul * step);
            } else {
                float friction = onGround ? (0.6f * 0.91f) : 0.91f;
                float friction2 = (0.6f * 0.6f * 0.91f * 0.91f * 0.6f * 0.91f) /
                                  (friction * friction * friction);
                float accel = (onGround ? (0.1f * friction2) : 0.02f) * sprintMul * step;
                moveRelative(xa, ya, accel);
            }

            AABB stepWaterCheckBox(x - R, y, z - R, x + R, y + H, z + R);
            inWater = level->applyWaterCurrent(stepWaterCheckBox, velX, velY, velZ) || inWater;

            float dx = velX * step;
            float dy = velY * step;
            float dz = velZ * step;
            const float expectedDx = dx;
            const float expectedDy = dy;
            const float expectedDz = dz;

            AABB player_aabb(x - R, y, z - R, x + R, y + H, z + R);
            AABB original_aabb = player_aabb;
            AABB* expanded = player_aabb.expand(dx, dy, dz);
            std::vector<AABB> cubes = level->getCubes(*expanded);
            delete expanded;

            float dyOrg = dy;
            for (auto& cube : cubes) dy = cube.clipYCollide(&player_aabb, dy);
            player_aabb.move(0, dy, 0);
            for (auto& cube : cubes) dx = cube.clipXCollide(&player_aabb, dx);
            player_aabb.move(dx, 0, 0);
            for (auto& cube : cubes) dz = cube.clipZCollide(&player_aabb, dz);
            player_aabb.move(0, 0, dz);

            // Step-up assistance (MCPE-like): walk up half-block obstacles smoothly.
            bool horizontalCollision = (dx != expectedDx) || (dz != expectedDz);
            if (horizontalCollision && onGround && expectedDy <= 0.0f) {
                const float stepHeight = 0.5f;
                float sdx = expectedDx;
                float sdy = stepHeight;
                float sdz = expectedDz;
                AABB stepAabb = original_aabb;
                AABB* stepExpanded = stepAabb.expand(sdx, sdy, sdz);
                std::vector<AABB> stepCubes = level->getCubes(*stepExpanded);
                delete stepExpanded;

                for (auto& cube : stepCubes) sdy = cube.clipYCollide(&stepAabb, sdy);
                stepAabb.move(0, sdy, 0);
                for (auto& cube : stepCubes) sdx = cube.clipXCollide(&stepAabb, sdx);
                stepAabb.move(sdx, 0, 0);
                for (auto& cube : stepCubes) sdz = cube.clipZCollide(&stepAabb, sdz);
                stepAabb.move(0, 0, sdz);

                float flatMoved = dx * dx + dz * dz;
                float stepMoved = sdx * sdx + sdz * sdz;
                if (stepMoved > flatMoved + 0.0001f) {
                    player_aabb = stepAabb;
                    dx = sdx;
                    dy = sdy;
                    dz = sdz;
                    horizontalCollision = false;
                }
            }

            onGround = (dyOrg != dy && dyOrg < 0.0f);
            bool didAutoJump = false;
            if (dx != expectedDx) velX = 0.0f;
            if (dz != expectedDz) velZ = 0.0f;
            if (dy != expectedDy) velY = 0.0f;
            bool autoJumpForward = (ya > 0.75f) && (fabsf(xa) < 0.4f);
            bool autoJumpEdge = horizontalCollision && !prevAutoJumpCollision;
            if (autoJumpEdge && onGround && expectedDy <= 0.0f && autoJumpForward) {
                // MCPE 0.6.1-like autojump: trigger once on a new forward collision.
                velY = 0.42f;
                onGround = false;
                didAutoJump = true;
            }
            prevAutoJumpCollision = horizontalCollision;

            x = (player_aabb.x0 + player_aabb.x1) * 0.5f;
            y = player_aabb.y0;
            z = (player_aabb.z0 + player_aabb.z1) * 0.5f;

            if (inWater) {
                velX *= powf(0.80f, step);
                velY *= powf(0.80f, step);
                velZ *= powf(0.80f, step);
                velY -= 0.02f * step;
            } else {
                float friction = onGround ? (0.6f * 0.91f) : 0.91f;
                if (!didAutoJump) {
                    velY -= 0.08f * step;
                    velY *= powf(0.98f, step);
                }
                velX *= powf(friction, step);
                velZ *= powf(friction, step);
            }

            // Update medium sample during long frames/sub-steps
            feetBlock = level->getBlock((int)floorf(x), (int)floorf(y), (int)floorf(z));
            headBlock = level->getBlock((int)floorf(x), (int)floorf(y + 1.6f), (int)floorf(z));
            inWater = g_blockProps[feetBlock].isLiquid() || g_blockProps[headBlock].isLiquid();
        }
    }
    wasInWater = inWater;

    // Enforce world bounds natively
    const float WORLD_MAX_X = (float)(WORLD_CHUNKS_X * CHUNK_SIZE_X - 1);
    const float WORLD_MAX_Z = (float)(WORLD_CHUNKS_Z * CHUNK_SIZE_Z - 1);
    if (x < 0.5f) x = 0.5f;
    if (x > WORLD_MAX_X) x = WORLD_MAX_X;
    if (z < 0.5f) z = 0.5f;
    if (z > WORLD_MAX_Z) z = WORLD_MAX_Z;

}

void Player::updateInteraction(float dt) {
    if (creativeInv.isOpen()) return;

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

        }
    }

    // Place block
    if (PSPInput_JustPressed(PSP_CTRL_LTRIGGER) && hitResult.hit) {
        int px = hitResult.nx;
        int py = hitResult.ny;
        int pz = hitResult.nz;

        uint8_t hitId = level->getBlock(hitResult.x, hitResult.y, hitResult.z);
        if (hitId != BLOCK_AIR && !g_blockProps[hitId].isSolid()) {
            px = hitResult.x;
            py = hitResult.y;
            pz = hitResult.z;
        }

        bool canPlace = true;
        uint8_t blockToPlace = heldBlock;
        if (heldBlock == BLOCK_SAPLING || heldBlock == BLOCK_TALLGRASS || heldBlock == BLOCK_FLOWER || 
            heldBlock == BLOCK_ROSE || heldBlock == BLOCK_MUSHROOM_BROWN || heldBlock == BLOCK_MUSHROOM_RED) {
            uint8_t floorId = level->getBlock(px, py - 1, pz);
            if (floorId != BLOCK_GRASS && floorId != BLOCK_DIRT && floorId != BLOCK_FARMLAND) {
                canPlace = false;
            }
        }
        if (heldBlock == BLOCK_CACTUS) {
            uint8_t floorId = level->getBlock(px, py - 1, pz);
            if (floorId != BLOCK_SAND && floorId != BLOCK_CACTUS) {
                canPlace = false;
            }
            if (g_blockProps[level->getBlock(px - 1, py, pz)].isSolid() ||
                g_blockProps[level->getBlock(px + 1, py, pz)].isSolid() ||
                g_blockProps[level->getBlock(px, py, pz - 1)].isSolid() ||
                g_blockProps[level->getBlock(px, py, pz + 1)].isSolid()) {
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
        bool canReplaceTarget = (targetBlock == BLOCK_AIR || !g_blockProps[targetBlock].isSolid());

        if (isBottomSlabId(heldBlock)) {
            uint8_t hitId2 = level->getBlock(hitResult.x, hitResult.y, hitResult.z);
            uint8_t hitIdBase = slabToFullBlock(hitId2);
            uint8_t heldBase = slabToFullBlock(heldBlock);
            bool hitIsMatchingSlab = ((isBottomSlabId(hitId2) || isTopSlabId(hitId2)) && hitIdBase == heldBase);
            if (hitIsMatchingSlab) {
                // Merge into a full block only when clicking the internal joining face:
                // - lower slab -> click top face
                // - upper slab -> click bottom face
                bool hitLower = isBottomSlabId(hitId2);
                bool hitUpper = isTopSlabId(hitId2);
                bool wantsMerge = (hitLower && hitResult.face == 1) || (hitUpper && hitResult.face == 0);
                if (wantsMerge) {
                    level->setBlock(hitResult.x, hitResult.y, hitResult.z, heldBase);
                    level->markDirty(hitResult.x, hitResult.y, hitResult.z, 20);
                    level->markDirty(hitResult.x - 1, hitResult.y, hitResult.z, ((hitResult.x & 0xF) == 0) ? 10 : 0, false);
                    level->markDirty(hitResult.x + 1, hitResult.y, hitResult.z, ((hitResult.x & 0xF) == 15) ? 10 : 0, false);
                    level->markDirty(hitResult.x, hitResult.y - 1, hitResult.z, ((hitResult.y & 0xF) == 0) ? 10 : 0, false);
                    level->markDirty(hitResult.x, hitResult.y + 1, hitResult.z, ((hitResult.y & 0xF) == 15) ? 10 : 0, false);
                    level->markDirty(hitResult.x, hitResult.y, hitResult.z - 1, ((hitResult.z & 0xF) == 0) ? 10 : 0, false);
                    level->markDirty(hitResult.x, hitResult.y, hitResult.z + 1, ((hitResult.z & 0xF) == 15) ? 10 : 0, false);
                    return;
                }
            }

            // Place upper slab when attaching to a block's bottom face.
            if (hitResult.face == 0) {
                blockToPlace = toTopSlabId(heldBlock);
            } else if (hitResult.face >= 2 && hitResult.face <= 5) {
                // Side placement:
                // - if clicking a slab side, preserve that slab orientation
                // - otherwise use upper/lower split by hit height.
                if (isBottomSlabId(hitId2)) {
                    blockToPlace = heldBlock;
                } else if (isTopSlabId(hitId2)) {
                    blockToPlace = toTopSlabId(heldBlock);
                } else if (hitResult.hitYLocal >= 0.5f) {
                    blockToPlace = toTopSlabId(heldBlock);
                }
            }
        }

        if (isStairId(blockToPlace)) {
            float yawRad = yaw * Mth::DEGRAD;
            float dirX = Mth::sin(yawRad);
            float dirZ = Mth::cos(yawRad);
            bool upsideDown = false;
            if (hitResult.face == 0) {
                upsideDown = true;
            } else if (hitResult.face >= 2 && hitResult.face <= 5) {
                upsideDown = (hitResult.hitYLocal >= 0.5f);
            }
            int lookDir;
            if (fabsf(dirX) > fabsf(dirZ)) {
                lookDir = (dirX >= 0.0f) ? 1 : 3; // east/west
            } else {
                lookDir = (dirZ >= 0.0f) ? 2 : 0; // south/north
            }
            int frontDir = (lookDir + 2) & 3; // face player
            blockToPlace = toOrientedStairId(stairBaseId(blockToPlace), frontDir, upsideDown);
        }

        if (canPlace && !overlaps && canReplaceTarget) {
            level->setBlock(px, py, pz, blockToPlace);
            
            level->markDirty(px, py, pz, 20); // Priority 20 ensures main block builds BEFORE neighbors
            level->markDirty(px - 1, py, pz, ((px & 0xF) == 0) ? 10 : 0, false);
            level->markDirty(px + 1, py, pz, ((px & 0xF) == 15) ? 10 : 0, false);
            level->markDirty(px, py - 1, pz, ((py & 0xF) == 0) ? 10 : 0, false);
            level->markDirty(px, py + 1, pz, ((py & 0xF) == 15) ? 10 : 0, false);
            level->markDirty(px, py, pz - 1, ((pz & 0xF) == 0) ? 10 : 0, false);
            level->markDirty(px, py, pz + 1, ((pz & 0xF) == 15) ? 10 : 0, false);
        }
    }

}
