#pragma once

#include "world/Level.h"
#include "world/Raycast.h"
#include "game/CreativeInventory.h"
#include <cstdint>

class Player {
public:
    Player(Level* level);
    ~Player();

    void spawn(float x, float y, float z);
    
    // Updates physics, input, and interactions
    void update(float dt);

    // Getters for camera and state
    float getX() const { return x; }
    float getY() const { return y; }
    float getZ() const { return z; }
    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }
    bool isFlyingCreative() const { return isFlying; }
    bool isSprinting() const { return sprinting; }
    float getWalkDist() const { return walkDist; }
    float getWalkDistO() const { return walkDistO; }
    float getBob() const { return bob; }
    float getOBob() const { return oBob; }
    float getTilt() const { return tilt; }
    float getOTilt() const { return oTilt; }

    // Hit result and held block state (for HUD and interactions)
    const RayHit& getHitResult() const { return hitResult; }
    uint8_t getHeldBlock() const { return heldBlock; }
    bool isInventoryOpen() const { return creativeInv.isOpen(); }
    const CreativeInventory& getCreativeInventory() const { return creativeInv; }

private:
    Level* level;
    float x, y, z;
    float yaw, pitch;
    float velX, velZ;
    float velY;
    bool onGround;
    bool isFlying;
    float jumpDoubleTapTimer;
    bool sprinting;
    float sprintDoubleTapTimer;
    bool prevForwardHeld;
    bool wasInWater;
    bool prevAutoJumpCollision;
    float walkDistO;
    float walkDist;
    float oBob;
    float bob;
    float oTilt;
    float tilt;

    RayHit hitResult;
    uint8_t heldBlock;
    float breakCooldown;
    CreativeInventory creativeInv;

    // Internal physics and interaction
    void updateInputAndPhysics(float dt);
    void updateInteraction(float dt);
    void moveRelative(float xa, float za, float speed);
};
