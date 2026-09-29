#ifndef STRUCTURE_H
#define STRUCTURE_H

#include <cstdint>
#include <memory>
#include <utility>

struct struct_template
{
    int x_width = 2;
    int y_width = 2;
    std::pair<int, int> origin = std::make_pair(0, 0); // local origin (bottom left)
    bool canTick = false; // if a structure needs to perform updates each tick
    
    uint32_t structureID = 0;
    uint32_t instanceID;

    float processing_time = 1.f; // in seconds
    float resource_input = 10.f; //data structure TBD
    float resource_output = 20.f; // data structure TBD

};



class Structure
{
    private:
        struct_template struct_;
    public:
        Structure(struct_template t);
        ~Structure();
        void on_tick(double deltaTime); // might want to return something
};


#endif
