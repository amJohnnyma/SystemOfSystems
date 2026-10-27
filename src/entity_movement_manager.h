#ifndef ENTITY_MOVEMENT_MANAGER_H
#define ENTITY_MOVEMENT_MANAGER_H

#include "math_types.h"
#include "world_types.h"

#include <vector>


class EntityMovementManager final
{
public:
    // Adjust velocity only. Does NOT change position or resolve collision.
    static void applyGravity(MovementState& state,
                             float gravity,
                             float maxFallSpeed,
                             float dt);

    // Horizontal steering toward a point (platformer behavior). Vertical
    // velocity is deliberately unchanged so gravity/jumping stays independent.
    // Caps speed AND avoids passing target.x within one frame.
    static void moveTowards(MovementState& state,
                            const Vec2& target,
                            float maxMoveSpeed,
                            float dt);

    // Advances BOTH axes once using the velocities computed above.
    // Sweeps crossed grid cells instead of taking tiny fixed substeps.
    // Returns updated position, velocity, ground state, and collision flags.
    static MovementResult resolve(const MovementState& state,
                                  const MovementWorld& world,
                                  float dt);

private:
    static bool isCellSolid(int x, int y, const MovementWorld& world);
    static bool intersectsSolid(const Vec2& position,
                                const Vec2& size,
                                const MovementWorld& world);
};

#endif
