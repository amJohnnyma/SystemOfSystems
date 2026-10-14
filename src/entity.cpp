#include "entity.h"
#include "math_types.h"
#include "world_types.h"

void Entity::on_tick(EntityTickContext etx) 
{
    applyGravity(etx);
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
    alignPosition();
}
void Entity::applyGravity(EntityTickContext etx)
{
    float dy = etx.gts.gravity * static_cast<float>(etx.deltaTime);
    float target_y = et_.y_smooth + dy;

    int current_row = static_cast<int>(std::ceil(et_.y_smooth));
    int target_row = static_cast<int>(std::ceil(target_y));

    if (et_.x_origin < 0 ||
        et_.x_origin + et_.width > etx.world_x) {
        et_.velocity.y = 0.f;
        return;
    }

    // Check every row entered during this movement.
    for (int row = current_row + 1; row <= target_row; ++row)
    {
        if (row < 0 || row >= etx.world_y) {
            et_.velocity.y = 0.f;
            return;
        }

        size_t index =
            static_cast<size_t>(row) * etx.world_x + et_.x_origin;

        bool blocked = false;

        for (int w = 0; w < et_.width; ++w)
        {
            const Cell& c = etx.world_cells[index + w];

            if (c.type_id != 0 || c.density >= etx.threshold) {
                blocked = true;
                break;
            }
        }

        if (blocked)
        {
            et_.velocity.y = 0.f;
            et_.y_smooth = static_cast<float>(row - 1) - 0.0001f;
            return;
        }
    }

    et_.y_smooth = target_y;
}

void Entity::alignPosition()
{
    et_.x_origin = static_cast<int>(std::floor(et_.x_smooth));

    if (et_.y_smooth > 0.0f) {
        et_.y_origin = static_cast<int>(std::ceil(et_.y_smooth));
    } else {
        et_.y_origin = static_cast<int>(std::floor(et_.y_smooth));
    }
}

void Entity::do_attack()
{
}
