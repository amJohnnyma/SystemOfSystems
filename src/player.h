#ifndef PLAYER_H
#define PLAYER_H

#include "world_types.h"
#include "math_types.h"

#include <cstdint>
#include <string>
#include <algorithm>
#include "entity_movement_manager.h"
#include "memory_usage.h"
#include "inventory.h"
#include "item.h"

class Player
{
private:
    double currentHealth_ = 100.0;
    double currentMoveSpeed_ = 0.0;

    Vec2 position_ = {0, 0};
    Vec2 velocity_ = {0, 0};
    Vec2 size_ = {2, 3};

    std::string sprite = "";
    std::string state_ = "idle";

    double animationProgress = 0.0;

    bool facingRight = true;
    bool isGrounded_ = false;
    bool canMove_ = true;

    float jumpSpeed_ = 50.0f;

    PlayerSettings ps;
    Inventory inventory_;

    ItemUseState active_attack_;
    float active_attack_duration_ = 0.0f;



    // Future:
    // Equipped item
    // Attack patterns
    // Attack cooldowns

public:
    Player();
    Player(PlayerSettings ps);
    ~Player();

std::size_t getMemoryUsage() const;

    void moveTo(
        Vec2 input,
        bool jump_pressed
    );

    void tick(const PlayerTickContext& ctx);

    void takeDamage(double damage);
    void takeHealing(double healing);
    void primaryAttack(const ItemRegistry& registry);

    // Position and physics
    const Vec2& getPosition() const { return position_; }
    const Vec2& getVelocity() const { return velocity_; }
    const Vec2& getSize() const { return size_; }

    bool isGrounded() const { return isGrounded_; }

    // Health
    double getMaxHealth() const { return ps.maxHealth; }
    double getCurrentHealth() const { return currentHealth_; }

    // Movement speed
    double getMaxMoveSpeed() const { return ps.maxMoveSpeed; }
    double getCurrentMoveSpeed() const { return currentMoveSpeed_; }

    // Rendering and animation
    const std::string& getSprite() const { return sprite; }
    const std::string& getState() const { return state_; }

    double getAnimProgress() const { return animationProgress; }
    bool isFacingRight() const { return facingRight; }



    void changeState(const std::string& new_state)
    {
        if (state_ == new_state)
            return;

        state_ = new_state;
        animationProgress = 0.0;

        auto it = ps.sprites.find(state_);

        if (it != ps.sprites.end())
        {
            sprite = it->second.filename;
        }
    }

    void initializeInventory(const ItemRegistry& registry);
    void selectHotbarSlot(int slot);
    uint32_t getEquippedItemId(const ItemRegistry& registry) const;

    const ItemAnimation* getActiveAttackAnimation(const ItemRegistry& registry) const;

    std::vector<AttackHitBoxBounds> getAttackHitboxBounds(const ItemRegistry& registry) const;
    const bool hasAttackHitboxBounds(const ItemRegistry& registry) const {return !getAttackHitboxBounds(registry).empty();}

    
};

#endif
