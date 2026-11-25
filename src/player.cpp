#include "player.h"
#include "item.h"
#include "world_types.h"
#include <cstdint>

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

    if (active_attack_.active)
    {
        active_attack_.elapsed += dt;
        animationProgress = std::min(
                1.0,
                static_cast<double>(active_attack_.elapsed / active_attack_duration_)
                );

        if (animationProgress >= 1.0)
        {
            active_attack_.active = false;
            changeState("idle");
        }
        return;
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

void Player::primaryAttack(const ItemRegistry& registry)
{
    if (active_attack_.active) return;

    const uint32_t id = getEquippedItemId(registry);
    if (id == 0) return;

    const auto item = registry.find(id);
    if(item == registry.end()) return;

    for(size_t i = 0; i < item->second.actions.size(); i ++)
    {
        const ItemAction& action = item->second.actions[i];
        if (action.type != ItemActionType::Attack || action.input != "primary")
        {
            continue;
        }

        const auto anim = item->second.animations.find(action.animation);
        if (anim == item->second.animations.end() ||
                anim->second.source.empty() || anim->second.clip.empty() ||
                anim->second.duration <= 0.f) return;

        active_attack_ = {true, id, static_cast<uint32_t>(i), 0.f, 0};
        active_attack_duration_ = anim->second.duration;
        changeState("primary_attack");
        return;
    }
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




std::vector<AttackHitBoxBounds> Player::getAttackHitboxBounds(const ItemRegistry& registry) const
{
    std::vector<AttackHitBoxBounds> bounds;
    if (!active_attack_.active) return bounds;

    const auto item = registry.find(active_attack_.item_id);
    if (item == registry.end() ||
        active_attack_.action_index >= item->second.actions.size())
        return bounds;

    const ItemAction& action =
        item->second.actions[active_attack_.action_index];
    if (action.hitbox_frame_count == 0) return bounds;

    const int frame = std::clamp(
        static_cast<int>(animationProgress * action.hitbox_frame_count),
        0, static_cast<int>(action.hitbox_frame_count) - 1
    );

    for (const ItemHitboxFrame& box : action.hitbox_frames) {
        if (box.frame != static_cast<uint32_t>(frame)) continue;

        const float left = facingRight
            ? position_.x + box.offset_r.x
            : position_.x + size_.x - box.offset_r.x - box.size.x;
        const float bottom = position_.y - box.offset_r.y + 1.0f;

        const Vec2 center{
            left + box.size.x * 0.5f,
            bottom - box.size.y * 0.5f
        };
        const float degrees = facingRight
            ? box.angle_deg : -box.angle_deg;
        const float radians = degrees * 3.14159265358979323846f / 180.0f;
        const float half_w = box.size.x * 0.5f;
        const float half_h = box.size.y * 0.5f;
        const float extent_x =
            std::abs(std::cos(radians)) * half_w +
            std::abs(std::sin(radians)) * half_h;
        const float extent_y =
            std::abs(std::sin(radians)) * half_w +
            std::abs(std::cos(radians)) * half_h;

        bounds.push_back({
            {center.x - extent_x, center.y + extent_y},
            {center.x + extent_x, center.y - extent_y},
            center, box.size, degrees
        });
    }
    return bounds;
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
    const size_t next = slot == -1
        ? inventory_.hotbar_size
        : (slot >= 0 && static_cast<size_t>(slot) < inventory_.hotbar_size
                ? static_cast<size_t>(slot)
                : inventory_.selected_hotbar_slot
                );
    if (next == inventory_.selected_hotbar_slot) return;
    inventory_.selected_hotbar_slot = next;
    active_attack_.active = false;
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

const ItemAnimation* Player::getActiveAttackAnimation(const ItemRegistry& registry) const
{
    if (!active_attack_.active) return nullptr;

    const auto item = registry.find(active_attack_.item_id);
    if (item == registry.end() || active_attack_.action_index >= item->second.actions.size()) return nullptr;

    const std::string& key = item->second.actions[active_attack_.action_index].animation;

    const auto anim = item->second.animations.find(key);
    return anim == item->second.animations.end() ? nullptr : &anim->second;
}
