#include "entity.h"
#include "math_types.h"
#include "world_types.h"

void Entity::on_tick(EntityTickContext etx)
{
    const float dt = static_cast<float>(etx.deltaTime);

    if (dt <= 0.0f)
        return;

    // 1. Construct movement state
    MovementState motion;

    motion.position = {
        et_.x_smooth,
        et_.y_smooth
    };

    motion.velocity = et_.velocity;

    motion.size = {
        static_cast<float>(et_.width),
        static_cast<float>(et_.height)
    };

    motion.isGrounded = et_.isGrounded;

    // 2. Set movement target (temporary: player)
    Vec2 target = etx.playerPos;

    // Determine desired horizontal velocity
    EntityMovementManager::moveTowards(
        motion,
        target,
        current_move_speed_,
        dt
    );

    // 3. Apply gravity
    EntityMovementManager::applyGravity(
        motion,        
        gravity,
        maxFallSpeed, // Temporary max fall speed
        dt
    );

    // 4. Build world context
    MovementWorld world{
        etx.world_cells,
        etx.world_x,
        etx.world_y,
        etx.threshold
    };

    // 5. Resolve collision and movement
    MovementResult result =
        EntityMovementManager::resolve(
            motion,
            world,
            dt
        );

    // 6. Apply movement result
    et_.x_smooth = result.state.position.x;
    et_.y_smooth = result.state.position.y;

    et_.velocity = result.state.velocity;
    et_.isGrounded = result.state.isGrounded;

    // Update integer origins for rendering / other systems
    alignPosition();

    // 7. Handle collision behavior
    // Preserve your previous automatic wall-jump behavior.
    if (result.hitWall && et_.isGrounded)
    {
        et_.velocity.y = -jumpStrength;
        et_.isGrounded = false;
    }

    // 8. Update facing direction
    if (et_.velocity.x > 0.001f)
    {
        facingRight = true;
    }
    else if (et_.velocity.x < -0.001f)
    {
        facingRight = false;
    }

    // 9. Update animation state
    if (std::abs(et_.velocity.x) > 0.001f)
    {
        changeState("walk");
    }
    else
    {
        changeState("idle");
    }

    // 10. Animation progression
    auto it = et_.animations.find(state);

    if (it != et_.animations.end())
    {
        const double animation_speed =
            it->second.animationSpeed;

        animationProgress +=
            etx.deltaTime * animation_speed;

        if (animationProgress >= 1.0)
        {
            animationProgress =
                std::fmod(animationProgress, 1.0);
        }
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

void Entity::do_attack()
{
}
