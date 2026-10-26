#include "player.h"

Player::Player()
{
    maxHealth_ = 100.0;
    currentHealth_ = maxHealth_;

    maxMoveSpeed_ = 25.0;
    currentMoveSpeed_ = maxMoveSpeed_;

    position_ = Vec2{320.0f, 10.0f};
    velocity_ = Vec2{0.0f, 0.0f};

    size_ = Vec2{2.0f, 3.0f};

    sprite = "Knight_idle.png";
}

Player::Player(PlayerSettings ps)
{
}
Player::~Player()
{
}
void Player::moveTo(
    Vec2 input,
    bool jump_pressed
)
{

    const float horizontal = std::clamp(
        input.x, -1.0f, 1.0f
    );

    velocity_.x =
        horizontal * static_cast<float>(currentMoveSpeed_);

    if (horizontal > 0.0f)
        facingRight = true;
    else if (horizontal < 0.0f)
        facingRight = false;

    if (jump_pressed && isGrounded_)
    {
        velocity_.y = -50.0f; // Temporary jump strength
        isGrounded_ = false;
    }
}

namespace
{
    constexpr float COLLISION_STEP = 0.25f;
    constexpr float EPSILON = 0.0001f;

    struct MovementResult
    {
        Vec2 position;
        Vec2 velocity;

        bool isGrounded = false;
        bool hitWall = false;
        bool hitCeiling = false;
    };

    bool isCellSolid(
        int x,
        int y,
        const PlayerTickContext& ctx
    )
    {
        if (x < 0 || x >= ctx.world.world_x ||
            y < 0 || y >= ctx.world.world_y)
        {
            return true;
        }

        const size_t index =
            static_cast<size_t>(y) * ctx.world.world_x + x;

        const Cell& cell = ctx.world.world_cells[index];

        return cell.type_id != 0 &&
               cell.density > ctx.world.threshold;
    }

    bool checkCollision(
        const Vec2& pos,
        const Vec2& size,
        const PlayerTickContext& ctx
    )
    {
        // Same coordinate convention as your entity renderer:
        // X = left edge
        // Y = bottom occupied cell origin

        const int left = static_cast<int>(
            std::floor(pos.x + EPSILON)
        );

        const int right = static_cast<int>(
            std::floor(pos.x + size.x - EPSILON)
        );

        const int top = static_cast<int>(
            std::floor(pos.y - size.y + 1.0f + EPSILON)
        );

        const int bottom = static_cast<int>(
            std::floor(pos.y + 1.0f - EPSILON)
        );

        for (int y = top; y <= bottom; ++y)
        {
            for (int x = left; x <= right; ++x)
            {
                if (isCellSolid(x, y, ctx))
                    return true;
            }
        }

        return false;
    }

    MovementResult resolveMovement(
        const Vec2& position,
        const Vec2& velocity,
        const Vec2& size,
        const PlayerTickContext& ctx
    )
    {
        MovementResult result;
        result.position = position;
        result.velocity = velocity;

        const float dt = static_cast<float>(ctx.deltaTime);

        // Horizontal movement
        const float dx = velocity.x * dt;

        const int stepsX = std::max(
            1,
            static_cast<int>(
                std::ceil(std::abs(dx) / COLLISION_STEP)
            )
        );

        const float stepX = dx / stepsX;

        for (int i = 0; i < stepsX; ++i)
        {
            Vec2 candidate = result.position;
            candidate.x += stepX;

            if (checkCollision(candidate, size, ctx))
            {
                result.velocity.x = 0.0f;
                result.hitWall = true;
                break;
            }

            result.position = candidate;
        }

        // Vertical movement
        const float dy = velocity.y * dt;

        const int stepsY = std::max(
            1,
            static_cast<int>(
                std::ceil(std::abs(dy) / COLLISION_STEP)
            )
        );

        const float stepY = dy / stepsY;

        for (int i = 0; i < stepsY; ++i)
        {
            Vec2 candidate = result.position;
            candidate.y += stepY;

            if (checkCollision(candidate, size, ctx))
            {
                if (stepY > 0.0f)
                    result.isGrounded = true;
                else if (stepY < 0.0f)
                    result.hitCeiling = true;

                result.velocity.y = 0.0f;
                break;
            }

            result.position = candidate;
        }

        // Check whether standing on the ground,
        // including when vertical velocity is zero.
        //
        // The small probe compensates for stopping at
        // the last valid movement substep.
        Vec2 groundProbe = result.position;
        groundProbe.y += COLLISION_STEP + EPSILON;

        if (checkCollision(groundProbe, size, ctx))
            result.isGrounded = true;

        return result;
    }
}
void Player::tick(const PlayerTickContext& ctx)
{
    const float dt = static_cast<float>(ctx.deltaTime);

    if (dt <= 0.0f)
        return;

    // Apply gravity
    velocity_.y += ctx.gts.gravity * dt;

    // Temporary collision processing
    MovementResult result = resolveMovement(
        position_,
        velocity_,
        size_,
        ctx
    );

    // Update player state
    position_ = result.position;
    velocity_ = result.velocity;
    isGrounded_ = result.isGrounded;

    // Animation state
    if (!isGrounded_)
    {
        changeState("jump");
    }
    else if (std::abs(velocity_.x) > 0.001f)
    {
        changeState("walk");
    }
    else
    {
        changeState("idle");
    }

    // Animation progress
    const double animation_speed =
        state_ == "idle"
        ? ctx.gts.idleAnimSpeed
        : ctx.gts.animationSpeed;

    animationProgress += dt * animation_speed;

    if (animationProgress >= 1.0)
    {
        animationProgress = std::fmod(
            animationProgress, 1.0
        );
    }
}

// These functions could technically be the same but a distinction is nice
void Player::takeDamage(double damage)
{
}
void Player::takeHealing(double damage)
{
}
