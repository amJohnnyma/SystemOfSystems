#ifndef STRUCTURE_H
#define STRUCTURE_H

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

struct resource_requirement
{
    uint32_t rID = 0;
    int count = 0;
};

struct struct_template
{
    int x_width = 2;
    int y_width = 2;
    std::pair<int, int> origin = std::make_pair(0, 0); // local origin (bottom left)
    bool canTick = false; // if a structure needs to perform updates each tick
    
    uint32_t structureID = 0;
    uint32_t instanceID;

    float processing_time = 1.f; // in seconds
    std::vector<resource_requirement> resource_input = {}; 
    std::vector<resource_requirement> resource_output = {};                                      

};



class Structure
{
    private:
        struct_template struct_;
    public:
        Structure(struct_template t) : struct_(std::move(t)) {}
        ~Structure() = default;
        void on_tick(double deltaTime) {} // might want to return something
        const struct_template& getTemplate() const {return struct_;}
};


#endif
