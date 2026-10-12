#ifndef ENTITY_H
#define ENTITY_H


#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <iostream>

struct entity_template
{
    uint32_t entityTypeID = 0;
    std::string name = "";
    std::string sprite = "";
    std::string entity_class = "enemy"; // "player", "enemy", "item", "particle"
    std::string behavior_type = "idle";
    
    float max_health = 100.0f;
    float move_speed = 5.0f;
    
    // Optional item/particle fields
    bool is_pickupable = false;

};

#endif
