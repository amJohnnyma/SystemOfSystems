#include "player.h"

Player::Player() : Player(PlayerSettings{})
{
    //nop
}

Player::~Player()
{
}
Player::Player(PlayerSettings settings)
    : ps(std::move(settings))
{
    // Health
    currentHealth_ = ps.maxHealth;

    // Movement
    currentMoveSpeed_ = ps.maxMoveSpeed;
    jumpSpeed_ = static_cast<float>(ps.jumpSpeed);

    // Physics
    position_ = ps.spawnPosition;
    velocity_ = Vec2{0.0f, 0.0f};
    size_ = ps.size;

    isGrounded_ = false;

    // Animation
    state_ = "idle";
    animationProgress = 0.0;
    facingRight = true;

    // Initial sprite
    auto it = ps.sprites.find(state_);

    if (it != ps.sprites.end())
    {
        sprite = it->second.filename;
    }
    else
    {
        sprite = "";
    }
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
        velocity_.y = -jumpSpeed_; // Temporary jump strength
        isGrounded_ = false;
    }
}



void Player::tick(const PlayerTickContext& ctx)
{
    const float dt = static_cast<float>(ctx.deltaTime);

    if (dt <= 0.0f)
        return;

    // 1. Build movement state
    MovementState motion;

    motion.position = position_;
    motion.velocity = velocity_;
    motion.size = size_;
    motion.isGrounded = isGrounded_;

    // 2. Apply gravity
    EntityMovementManager::applyGravity(
        motion,
        ps.gravity,
        static_cast<float>(ps.maxFallSpeed),
        dt
    );

    // 3. Build world collision context
    MovementWorld world{
        ctx.world.world_cells,
        ctx.world.world_x,
        ctx.world.world_y,
        ctx.world.threshold
    };

    // 4. Resolve movement
    MovementResult result =
        EntityMovementManager::resolve(
            motion,
            world,
            dt
        );

    // 5. Apply results
    position_ = result.state.position;
    velocity_ = result.state.velocity;
    isGrounded_ = result.state.isGrounded;

    // 6. Animation state
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

    // 7. Animation progression
    auto it = ps.sprites.find(state_);

    if (it != ps.sprites.end())
    {
        const double animation_speed =
            it->second.animationSpeed;

        animationProgress += dt * animation_speed;

        if (animationProgress >= 1.0)
        {
            animationProgress =
                std::fmod(animationProgress, 1.0);
        }
    }
}

// These functions could technically be the same but a distinction is nice
void Player::takeDamage(double damage)
{
}
void Player::takeHealing(double damage)
{
}
