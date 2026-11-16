#include "entity_movement_manager.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
    constexpr float EDGE_EPSILON = 0.00001f;
    constexpr float GROUND_PROBE = 0.001f;
    constexpr float TARGET_EPSILON = 0.001f;

    // Low and high are inclusive cell indices of a half-open extent.
    int lowCell(float edge)  { return static_cast<int>(std::floor(edge + EDGE_EPSILON)); }
    int highCell(float edge) { return static_cast<int>(std::floor(edge - EDGE_EPSILON)); }
}

void EntityMovementManager::applyGravity(MovementState& state,
        float gravity,
        float maxFallSpeed,
        float dt)
{
    if (dt <= 0.0f)
        return;

    // In Godot 2D / this grid, +Y is DOWN.
    state.velocity.y = std::min(state.velocity.y + gravity * dt,
            std::max(0.0f, maxFallSpeed));
}

void EntityMovementManager::moveTowards(MovementState& state,
        const Vec2& target,
        float maxMoveSpeed,
        float dt)
{
    if (dt <= 0.0f)
        return;

    const float distance = target.x - state.position.x;
    if (std::abs(distance) <= TARGET_EPSILON)
    {
        state.velocity.x = 0.0f;
        return;
    }
    /*
    // AI has calculated its next waypoint
    Vec2 waypoint = path.front();

    // Horizontal steering
    EntityMovementManager::moveTowards(
    motion, waypoint, moveSpeed, dt
    );

    // AI separately decides whether to jump
    if (shouldJump && motion.isGrounded)
    {
    motion.velocity.y = -jumpSpeed;
    motion.isGrounded = false;
    }
    */

    // Move no faster than needed to reach the target in THIS frame.
    const float speed = std::min(std::max(0.0f, maxMoveSpeed),
            std::abs(distance) / dt);
    state.velocity.x = std::copysign(speed, distance);
}

float EntityMovementManager::tryStepUp(
        const MovementState& state,
        const MovementWorld& world,
        float dx
        )
{
    if (!state.isGrounded ||
            state.velocity.y < 0.0f ||
            state.maxStepHeight <= 0.0f ||
            std::abs(dx) <= EDGE_EPSILON)
    {
        return 0.0f;
    }

    constexpr float STEP_INCREMENT = 0.1f;
    constexpr float SWEEP_INCREMENT = 0.25f;

    const Vec2 original = state.position;

    const int stepCount = static_cast<int>(
            std::ceil(state.maxStepHeight / STEP_INCREMENT)
            );

    for (int step = 1; step <= stepCount; ++step)
    {
        const float height = std::min(
                step * STEP_INCREMENT,
                state.maxStepHeight
                );

        Vec2 candidate = original;
        bool clear = true;

        // 1. Check the entire upward path.
        const int verticalSamples = static_cast<int>(
                std::ceil(height / SWEEP_INCREMENT)
                );

        for (int i = 1; i <= verticalSamples; ++i)
        {
            const float fraction =
                static_cast<float>(i) / verticalSamples;

            candidate.y = original.y - height * fraction;

            if (intersectsSolid(candidate, state.size, world))
            {
                clear = false;
                break;
            }
        }

        if (!clear)
            continue;

        // 2. Check the horizontal path at the elevated position.
        candidate.y = original.y - height;

        const int horizontalSamples = static_cast<int>(
                std::ceil(std::abs(dx) / SWEEP_INCREMENT)
                );

        for (int i = 1; i <= horizontalSamples; ++i)
        {
            const float fraction =
                static_cast<float>(i) / horizontalSamples;

            candidate.x = original.x + dx * fraction;

            if (intersectsSolid(candidate, state.size, world))
            {
                clear = false;
                break;
            }
        }

        if (!clear)
            continue;

        // 3. Verify that there's ground beneath the destination.
        candidate.x = original.x + dx;
        candidate.y = original.y - height + GROUND_PROBE;

        if (!intersectsSolid(candidate, state.size, world))
            continue;

        return height;
    }

    return 0.0f;
}

bool EntityMovementManager::isCellSolid(int x, int y,
        const MovementWorld& world)
{
    // Treat outside world (or missing cell data) as solid.
    if (x < 0 || x >= world.width || y < 0 || y >= world.height)
        return true;

    const std::size_t index = static_cast<std::size_t>(y) *
        static_cast<std::size_t>(world.width) +
        static_cast<std::size_t>(x);
    if (index >= world.cells.size())
        return true;

    const Cell& cell = world.cells[index];
    return cell.type_id != 0 && cell.density >= world.solidThreshold;
}

bool EntityMovementManager::intersectsSolid(const Vec2& position,
        const Vec2& size,
        const MovementWorld& world)
{
    const int left   = lowCell(position.x);
    const int right  = highCell(position.x + size.x);
    const int top    = lowCell(position.y - size.y + 1.0f);
    const int bottom = highCell(position.y + 1.0f);

    for (int row = top; row <= bottom; ++row)
        for (int col = left; col <= right; ++col)
            if (isCellSolid(col, row, world))
                return true;
    return false;
}

MovementResult EntityMovementManager::resolve(const MovementState& initial,
        const MovementWorld& world,
        float dt)
{
    MovementResult result;
    result.state = initial;

    if (dt <= 0.0f || initial.size.x <= 0.0f || initial.size.y <= 0.0f)
        return result;

    // AXIS 1: sweep X. Only the columns newly entered by the leading edge
    // can produce a NEW collision if the starting AABB is collision-free.
    const float dx = result.state.velocity.x * dt;
    const int top = lowCell(result.state.position.y - result.state.size.y + 1.0f);
    const int bottom = highCell(result.state.position.y + 1.0f);
    bool blockedX = false;


    if (dx > 0.0f)
    {
        const float right =
            result.state.position.x + result.state.size.x;

        const float next_right = right + dx;

        const int first = static_cast<int>(
                std::floor(right)
                );

        const int last = static_cast<int>(
                std::ceil(next_right)
                ) - 1;

        for (int col = first; col <= last; ++col)
        {
            if (next_right <= static_cast<float>(col))
                continue;

            bool blocked = false;

            for (int row = top; row <= bottom; ++row)
            {
                if (isCellSolid(col, row, world))
                {
                    blocked = true;
                    break;
                }
            }

            if (blocked)
            {
                const float resolved_x =
                    static_cast<float>(col) -
                    result.state.size.x;

                result.state.position.x = std::min(
                        result.state.position.x,
                        resolved_x
                        );

                blockedX = true;
                break;
            }
        }
    }
    else if (dx < 0.0f)
    {
        const float left = result.state.position.x;
        const int first = lowCell(left) - 1;
        const int last = lowCell(left + dx);
        for (int col = first; col >= last && !blockedX; --col)
        {
            for (int row = top; row <= bottom; ++row)
            {
                if (isCellSolid(col, row, world))
                {
                    result.state.position.x = static_cast<float>(col + 1);
                    blockedX = true;
                    break;
                }
            }
        }
    }
if (blockedX)
{
    const float stepHeight = tryStepUp(
        initial,
        world,
        dx
    );

    if (stepHeight > 0.0f)
    {
        // Move onto the obstacle.
        result.state.position.x = initial.position.x + dx;
        result.state.position.y = initial.position.y - stepHeight;

        result.stepHeight = stepHeight;

        // Preserve horizontal velocity.
        blockedX = false;
    }
    else
    {
        // Normal wall collision.
        result.state.velocity.x = 0.0f;
        result.hitWall = true;
    }
}
else
{
    result.state.position.x += dx;
}


    // AXIS 2: sweep Y using the X-resolved AABB.
    const float dy = result.state.velocity.y * dt;
    const int left = lowCell(result.state.position.x);
    const int right = highCell(result.state.position.x + result.state.size.x);
    bool blockedY = false;

    if (dy > 0.0f)
    {
        const float foot = result.state.position.y + 1.0f;
        const int first = static_cast<int>(std::floor(foot));
        const int last = static_cast<int>(
                std::floor(foot + dy - EDGE_EPSILON)
                );
        for (int row = first; row <= last && !blockedY; ++row)
        {
            for (int col = left; col <= right; ++col)
            {
                if (isCellSolid(col, row, world))
                {
                    // The bottom edge now rests exactly on row's top edge.
                    result.state.position.y = static_cast<float>(row) - 1.0f;
                    blockedY = true;
                    result.landed = true;
                    break;
                }
            }
        }
    }

    else if (dy < 0.0f)
    {
        const float head = result.state.position.y - result.state.size.y + 1.0f;
        const int first = lowCell(head) - 1;
        const int last = lowCell(head + dy);
        for (int row = first; row >= last && !blockedY; --row)
        {
            for (int col = left; col <= right; ++col)
            {
                if (isCellSolid(col, row, world))
                {
                    // Head rests just below the hit cell's bottom edge.
                    result.state.position.y = static_cast<float>(row) + result.state.size.y;
                    blockedY = true;
                    result.hitCeiling = true;
                    break;
                }
            }
        }
    }

    if (blockedY)
        result.state.velocity.y = 0.0f;
    else
        result.state.position.y += dy;

    // Small ground probe catches standing still or walking off edges.
    // Do not consider an upward-moving character grounded.
    Vec2 probe = result.state.position;
    probe.y += GROUND_PROBE;
    result.state.isGrounded = result.state.velocity.y >= 0.0f &&
        intersectsSolid(probe, result.state.size, world);

    return result;
}
