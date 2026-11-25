#include "player.h"
#include "world_types.h"

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

    if (state_ == "primary_attack")
    {
        return;
    }

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
    motion.maxStepHeight = 1.f;

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
    if (state_ != "primary_attack")
    {

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
            if (state_ == "primary_attack")
            {
                changeState("idle");
                return;
            }
            else
            {
                animationProgress =
                    std::fmod(animationProgress, 1.0);
            }

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

void Player::primaryAttack()
{
    if (state_ == "primary_attack")
    {
        return;
    }
    changeState("primary_attack");
}

std::size_t Player::getMemoryUsage() const
{
    std::size_t bytes = sizeof(*this);

    // Player configuration
    bytes += MemoryUsage::dynamic(ps);

    // Current sprite and animation state
    bytes += MemoryUsage::heap(sprite);
    bytes += MemoryUsage::heap(state_);

    return bytes;
}

const std::vector<AttackHitboxData>* Player::currentHitboxDefinitions() const
{
    if (state_ != "primary_attack") return nullptr;

    const auto state = ps.sprites.find(state_);

    if (state == ps.sprites.end()) return nullptr;

    const int count = std::max(
            1, state->second.columns * state->second.rows
            );
    const int frame = std::clamp(
            static_cast<int>(animationProgress * count), 0, count - 1
            );
    const auto boxes = state->second.frameHitboxes.find(frame);

    return boxes == state->second.frameHitboxes.end()
        ? nullptr : &boxes->second;

}

std::vector<AttackHitBoxBounds> Player::getAttackHitboxBounds() const
{
    std::vector<AttackHitBoxBounds> bounds;
    const auto* boxes = currentHitboxDefinitions();
    if(boxes == nullptr) return bounds;
    bounds.reserve(boxes->size());

    for (const AttackHitboxData& hitbox : *boxes)
    {
        const float left = facingRight
            ? position_.x + hitbox.offsetX
            : position_.x + size_.x - hitbox.offsetX - hitbox.width;

        const float bottom = position_.y - hitbox.offsetY + 1.f;

        bounds.push_back(
                {
                Vec2{left, bottom},
                Vec2{left + hitbox.width, bottom - hitbox.height}
                }
                );
    }
    return bounds;
}

bool Player::isAttackHitboxActive() const
{
    const auto* boxes = currentHitboxDefinitions();
    return boxes != nullptr && !boxes->empty();
}

void Player::initializeInventory(const ItemRegistry& registry)
{
    const size_t count = std::min(inventory_.hotbar_size, ps.startingHotbar.size()); 
    for (size_t i = 0; i < count; ++i) {
        auto found = registry.find(ps.startingHotbar[i]); 
        if (found == registry.end() || !found->second.can_go_hotbar) continue;
        ItemStack& stack = inventory_.slots[i];
        stack.item_id = found->first;
        stack.count = 1;   
        stack.durability = found->second.max_durability;
    }  
}  
void Player::selectHotbarSlot(int slot)  
{        
    if (slot < 0 || static_cast<size_t>(slot) >= inventory_.hotbar_size ||  
            inventory_.selected_hotbar_slot == static_cast<size_t>(slot)) return; 
    inventory_.selected_hotbar_slot = static_cast<size_t>(slot);      
    if (state_ == "primary_attack") changeState("idle");
} 
uint32_t Player::getEquippedItemId(const ItemRegistry& registry) const
{ 
    const ItemStack* stack = inventory_.selectedItem(); 
    if (stack == nullptr || stack->empty()) return 0;
    auto found = registry.find(stack->item_id); 
    return found != registry.end() && found->second.weapon.has_value()
        ? stack->item_id : 0;
}
