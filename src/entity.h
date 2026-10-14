#ifndef ENTITY_H
#define ENTITY_H


#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <iostream>
#include "attack_type.h"
#include "world_types.h"



struct attack
{
    std::string name = "";
    float damage = 0;
    float range = 0;
    attack_type at = DIRECT;
    float cooldownBeforeNextAttack = 0; // cooldown before any other attack
};

struct attack_pattern
{
    std::string name = "";
    std::vector<attack> attacks;
    
};

struct entity_template
{
    uint32_t entityTypeID = 0;
    uint32_t instanceID = 0;
    
    std::string name = "";
    std::string sprite = "";
    std::string entity_class = "enemy"; // "player", "enemy", "item", "particle"
    std::string behavior_type = "idle";
    
    float max_health = 100.0f;
    float move_speed = 5.0f;
    int width = 2;
    int height = 2;

    int x_origin = 0;
    int y_origin = 0;

    float x_smooth = 0;
    float y_smooth = 0;

    bool isGrounded = true;

    Vec2 velocity = Vec2{0,0};
    
    
    // Optional item/particle fields
    bool is_pickupable = false;

    std::vector<attack_pattern> attack_patterns; // The various attack patterns for different phases, enemy tactic, etc.

};

class Entity
{
    private:
        entity_template et_;
        float current_health_;
        float current_move_speed_;



    public:
        Entity(entity_template t) : et_(std::move(t)) 
    {
        current_health_ = t.max_health;
        current_move_speed_ = t.move_speed;

    
    }
        ~Entity() = default;
        void on_tick(EntityTickContext etx);
        void do_attack(); // cycles attack patterns, changes which attack pattern to follow
        const entity_template& getTemplate() const {return et_;}
        void applyGravity(EntityTickContext etx);
        void alignPosition();
        void moveTo(float x, float y, EntityTickContext etx);

};

#endif
