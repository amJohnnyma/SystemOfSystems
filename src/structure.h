#ifndef STRUCTURE_H
#define STRUCTURE_H

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <iostream>

struct resource_requirement
{
    uint32_t rID = 0;
    int count = 0;
};

struct struct_template
{
    int x_width = 2;
    int y_width = 2;
    int x_origin = 0;
    int y_origin = 0;
    bool canTick = false; // if a structure needs to perform updates each tick
    bool isBuilding = false; // else its just a "block"
    int blockType = 0;
    std::string sprite = "";
    
    uint32_t structureID = 0;
    uint32_t instanceID;

    float processing_time = 1.f; // in seconds
    std::vector<resource_requirement> resource_input = {}; 
    std::vector<resource_requirement> resource_output = {};                                      

};



// a structure is something like a building, or a blocky terrain block
class Structure
{
    private:
        struct_template struct_;
    public:
        Structure(struct_template t) : struct_(std::move(t)) {}
        ~Structure() = default;
        void on_tick(double deltaTime) {} // might want to return something
        const struct_template& getTemplate() const {return struct_;}
        std::size_t getMemoryUsage() const;
};


#endif
