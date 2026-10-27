#ifndef ENTITY_H
#define ENTITY_H


#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <iostream>
#include <algorithm>
#include "world_types.h"
#include "entity_movement_manager.h"
#include "memory_usage.h"



class Entity
{
    private:
        entity_template et_;
        std::string state = "idle";
        double animationProgress = 0.0;
        float current_health_;
        float current_move_speed_;
        bool facingRight = true;
        float gravity = 8.f;
        float maxFallSpeed = 100.f;
        float jumpStrength = 20.f;



    public:
        Entity(entity_template t) : et_(std::move(t)) 
    {
        current_health_ = et_.max_health;
        current_move_speed_ = et_.move_speed;
        gravity = et_.gravity;
        maxFallSpeed = et_.maxFallSpeed;
        jumpStrength = et_.jumpStrength;

    
    }
        ~Entity() = default;
        void on_tick(EntityTickContext etx);
        void do_attack(); // cycles attack patterns, changes which attack pattern to follow
        const entity_template& getTemplate() const {return et_;}
        void alignPosition();
        std::string getState() const {return state;}
        double getAnimProgress() const {return animationProgress;}
        bool isFacingRight() const {return facingRight;}
        void changeState(std::string state); 
        std::size_t getMemoryUsage() const;


};

#endif
