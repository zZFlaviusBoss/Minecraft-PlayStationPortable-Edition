#pragma once

#include "world/Level.h"
#include "world/Raycast.h"
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

    // Hit result and held block state (for HUD and interactions)
    const RayHit& getHitResult() const { return hitResult; }
    uint8_t getHeldBlock() const { return heldBlock; }

private:
    Level* level;
    float x, y, z;
    float yaw, pitch;
    float velY;
    bool onGround;
    bool isFlying;
    float jumpDoubleTapTimer;

    RayHit hitResult;
    uint8_t heldBlock;
    float breakCooldown;

    // Internal physics and interaction
    void updateInputAndPhysics(float dt);
    void updateInteraction(float dt);
};
