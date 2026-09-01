
// exposes world to ClassDB bindings to GDScript
// translate raw cpp into godot resources

#ifndef GODOT_WORLD_H
#define GODOT_WORLD_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <vector>
#include "world_grid.h" // Includes pure C++ header

namespace godot {

class GodotSimulation : public Node {
    GDCLASS(GodotSimulation, Node)

private:
    WorldGrid world_grid_; // Holds your pure C++ object

protected:
    static void _bind_methods();

public:
    GodotSimulation();
    ~GodotSimulation();

    void _process(double delta) override;
    
    // Exposed wrapper methods
    void createWorld(int world_x, int world_y, int g_size);
    Dictionary getMeshData();
    Array updateMeshData();
    void setDensity(int x, int y, float radius, float strength, uint16_t type_id);
    void fillCell(int x,int y, uint16_t type_id, bool negativeFill =false);
};

} // namespace godot

#endif
