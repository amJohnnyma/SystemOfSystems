#include "entity.h"
#include "math_types.h"
#include "world_types.h"
void Entity::on_tick(EntityTickContext etx) 
{
    applyGravity(etx);
    alignPosition();

    moveTo(etx.playerPos.x,etx.playerPos.y,etx);
    alignPosition();
    // we can search these cells using our own functions

    // 1. Calculate target velocity -> Only care about the player if in range
    // 1.1 Run A* on chunks and then on cells
    // 1.2 Determine target vector, and save A* for future use
    // 2. Check (future) AABB around us using this entities width and height
    // 2.1 If collision then "bump" away -> Should allowto keep following A* target line
    // 2.2 No collision -> Update velocity, position, etc.
    // 3. If within some range then perform an attack
    // 3.1 TBD

    // done after all changes
    if (et_.velocity.x == 0.f)
    {
        changeState("idle");
    }
    else
    {

        changeState("walk");
    }

    double prog = 0.0;
    if (state == "idle")
    {
        prog =(etx.deltaTime * etx.gts.idleAnimSpeed);
    }
    else
    {
        prog =(etx.deltaTime * etx.gts.animationSpeed); 
    }
    animationProgress = std::clamp(animationProgress + prog,0.0,1.0);
    if (animationProgress >= 1.0)
    {
        animationProgress = 0.0;
    }
}

void Entity::changeState(std::string new_state) 
{
    if (state != new_state) 
    {
        state = new_state;
        animationProgress = 0.0; // Only reset when transitioning to a different state!
    }
}

void Entity::alignPosition()
{

    if (et_.x_smooth > 0.0f) {
        et_.x_origin = static_cast<int>(std::ceil(et_.x_smooth));
    } else {
        et_.x_origin = static_cast<int>(std::floor(et_.x_smooth));
    }

    if (et_.y_smooth > 0.0f) {
        et_.y_origin = static_cast<int>(std::ceil(et_.y_smooth));
    } else {
        et_.y_origin = static_cast<int>(std::floor(et_.y_smooth));
    }
}
void Entity::applyGravity(EntityTickContext etx)
{
    const float dt = static_cast<float>(etx.deltaTime);
    if (dt <= 0.0f)
        return;

    et_.velocity.y += etx.gts.gravity * dt;

    const float dy = et_.velocity.y * dt;
    const int steps = std::max(
        1, static_cast<int>(std::ceil(std::abs(dy) / 0.25f))
    );

    const float step_y = dy / steps;

    for (int s = 0; s < steps; ++s)
    {
        const float candidate_y = et_.y_smooth + step_y;

        const int origin_x = et_.x_origin;
        const int origin_y = candidate_y > 0.0f
            ? static_cast<int>(std::ceil(candidate_y))
            : static_cast<int>(std::floor(candidate_y));

        bool blocked = false;

        for (int h = 0; h < et_.height && !blocked; ++h)
        {
            const int row = origin_y - h;

            for (int w = 0; w < et_.width; ++w)
            {
                const int col = origin_x + w;

                if (col < 0 || col >= etx.world_x ||
                    row < 0 || row >= etx.world_y)
                {
                    blocked = true;
                    break;
                }

                const size_t index =
                    static_cast<size_t>(row) * etx.world_x + col;

                const Cell& c = etx.world_cells[index];

                if (c.type_id != 0 || c.density >= etx.threshold)
                {
                    blocked = true;
                    break;
                }
            }
        }

        if (blocked)
        {
            et_.velocity.y = 0.0f;
            et_.isGrounded = true;
            return;
        }
        et_.isGrounded = false;

        et_.y_smooth = candidate_y;
    }
}
void Entity::moveTo(float x, float y, EntityTickContext etx)
{
   // (void)y; // Vertical movement is currently handled by gravity.

// TEMP FIX: Stop if there's a solid block directly to the left.
    const int left_col = et_.x_origin - 1;

    const float distance = x - et_.x_smooth;
    const float direction = distance > 0.0f ? 1.0f : -1.0f;
    if (left_col >= 0 && left_col < etx.world_x && direction < 0.f)
    {
        for (int h = 0; h < et_.height; ++h)
        {
            const int row = et_.y_origin - h;

            if (row < 0 || row >= etx.world_y)
                continue;

            const size_t index =
                static_cast<size_t>(row) * etx.world_x + left_col;

            const Cell& c = etx.world_cells[index];

            if (c.type_id != 0 || c.density >= etx.threshold)
            {
                et_.x_smooth = static_cast<float>(et_.x_origin);
                et_.velocity.x = 0.0f;
                if(et_.isGrounded)
                {
                et_.velocity.y = -2.0f * etx.gts.gravity;
                et_.isGrounded = false;
                }
                return;
            }
        }
    }
    const float dt = static_cast<float>(etx.deltaTime);
    if (dt <= 0.0f)
        return;


    if (std::abs(distance) < 0.001f)
    {
        et_.velocity.x = 0.0f;
        return;
    }


    // Desired horizontal velocity.
    et_.velocity.x = direction * et_.move_speed;

    // Don't overshoot the target.
    const float dx = direction *
        std::min(std::abs(distance), et_.move_speed * dt);

    const int steps = std::max(
        1, static_cast<int>(std::ceil(std::abs(dx) / 0.25f))
    );

    const float step_x = dx / steps;

    for (int s = 0; s < steps; ++s)
    {
        const float candidate_x = et_.x_smooth + step_x;

        const int origin_x = candidate_x > 0.0f
            ? static_cast<int>(std::ceil(candidate_x))
            : static_cast<int>(std::floor(candidate_x));

        bool blocked = false;

        for (int h = 0; h < et_.height && !blocked; ++h)
        {
            const int row = et_.y_origin - h;

            for (int w = 0; w < et_.width; ++w)
            {
                const int col = origin_x + w;

                if (col < 0 || col >= etx.world_x ||
                    row < 0 || row >= etx.world_y)
                {
                    blocked = true;
                    break;
                }

                const size_t index =
                    static_cast<size_t>(row) * etx.world_x + col;

                const Cell& c = etx.world_cells[index];

                if (c.type_id != 0 || c.density >= etx.threshold)
                {
                    blocked = true;
                    break;
                }
            }
        }

        if (blocked)
        {
            et_.velocity.x = 0.0f;

            if (et_.isGrounded)
            {

            et_.velocity.y = -2.0f * etx.gts.gravity;

            et_.isGrounded = false;
            }
            return;
        }

        // Commit only after this substep is confirmed clear.
        et_.x_smooth = candidate_x;
        alignPosition();
    }
}

void Entity::do_attack()
{
}
