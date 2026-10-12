
#include "godot_world.h"
#include <charconv>
#include <cstdint>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

using namespace godot;

void GodotSimulation::_bind_methods() {
    ClassDB::bind_method(D_METHOD("createWorld", "world_x", "world_y", "g_size"), &GodotSimulation::createWorld);
    ClassDB::bind_method(D_METHOD("getMeshData"), &GodotSimulation::getMeshData);
    ClassDB::bind_method(D_METHOD("setDensity", "x", "y", "radius", "strength", "type_id"), &GodotSimulation::setDensity);
    ClassDB::bind_method(D_METHOD("updateMeshData"), &GodotSimulation::updateMeshData);
    ClassDB::bind_method(D_METHOD("fillCell", "x", "y", "type_id", "negativeFill"), &GodotSimulation::fillCell);
    ClassDB::bind_method(D_METHOD("getStructureRegistry"), &GodotSimulation::getStructureRegistry);
    ClassDB::bind_method(D_METHOD("placeStructure", "x", "y", "structureID"), &GodotSimulation::placeStructure);
    ClassDB::bind_method(D_METHOD("selectStructure", "x", "y"), &GodotSimulation::selectStructure);



    ClassDB::bind_method(D_METHOD("trigger_test_signal", "message"), &GodotSimulation::trigger_test_signal);
    ADD_SIGNAL(MethodInfo("test_signal_event", PropertyInfo(Variant::STRING, "message")));


    ClassDB::bind_method(D_METHOD("update_godot_mesh_signal"), &GodotSimulation::trigger_test_signal);
    ADD_SIGNAL(MethodInfo("update_godot_mesh_event"));

ClassDB::bind_method(D_METHOD("updatePlayerPosition", "x", "y"), &GodotSimulation::updatePlayerPosition);
    ClassDB::bind_method(D_METHOD("resolveCollision", "current_pos", "velocity", "width", "height", "delta"), &GodotSimulation::resolveCollision);

}

GodotSimulation::GodotSimulation() {}
GodotSimulation::~GodotSimulation() {}

void GodotSimulation::trigger_test_signal(const godot::String &msg)
{
    emit_signal("test_signal_event", msg);
}

void GodotSimulation::update_godot_mesh_signal()
{
    emit_signal("update_godot_mesh_event");
}

void GodotSimulation::_process(double delta)
{
    uint32_t flags = world_grid_.tick(delta);
    if (flags & TickFlags::MESH_UPDATE)
    {
        update_godot_mesh_signal();
    }

    if (flags & TickFlags::UI_UPDATE)
    {
        // emit_signal("update_ui_event");
    }
}
void GodotSimulation::updatePlayerPosition(float x, float y) {
    world_grid_.updatePlayerPosition(x, y);
}

Vector2 GodotSimulation::resolveCollision(Vector2 current_pos, Vector2 velocity, float width, float height, double delta) {
    Vec2 cp = Vec2(current_pos.x, current_pos.y);
    Vec2 v = Vec2(velocity.x,velocity.y);
    Vec2 r = world_grid_.resolveCollision(cp,v, width, height, delta);
    Vector2 rr = Vector2(r.x, r.y);
 
    return rr; 
}

void GodotSimulation::createWorld(int world_x, int world_y, int g_size)
{

    world_grid_ = WorldGrid(); 
    world_grid_.init(world_x, world_y, g_size);

    auto const& sr = world_grid_.getStructureRegistry();
    godot::String dump = "--- Registry Dump ---\n";

    
    char buffer[256];
    for (auto it = sr.begin(); it != sr.end(); ++it) {
        auto const& st = it->second.getTemplate();
        
        snprintf(buffer, sizeof(buffer), "ID: %u | Size: %dx%d | Tick: %s | Time: %.1f | Type: %s\n",
                 st.structureID, st.x_width, st.y_width, 
                 (st.canTick ? "yes" : "no"), st.processing_time,
                 (st.isBuilding ? "Building": "Block"));
                 
        dump += godot::String(buffer);
    }

    trigger_test_signal(dump);

}

godot::Dictionary GodotSimulation::getStructureRegistry() const {
    godot::Dictionary registry_dict;
    auto const& sr = world_grid_.getStructureRegistry();

    for (auto it = sr.begin(); it != sr.end(); ++it) {
        auto const& st = it->second.getTemplate();
        
        godot::Dictionary st_dict;
        st_dict["id"] = st.structureID;
        st_dict["width"] = st.x_width;
        st_dict["height"] = st.y_width;
        st_dict["can_tick"] = st.canTick;
        st_dict["processing_time"] = st.processing_time;
        st_dict["isBuilding"] = st.isBuilding;
        st_dict["blockType"] = st.blockType;
        st_dict["sprite"] = godot::String(st.sprite.c_str());
        godot::Array input_arr;
        godot::Array output_arr;
        for (const auto& ri : st.resource_input) {
            godot::Dictionary req;
            req["objectID"] = ri.rID;
            req["count"] = ri.count;
            input_arr.append(req);
        }
        st_dict["resource_input"] = input_arr;
        for (const auto& ro : st.resource_output) {
            godot::Dictionary req;
            req["objectID"] = ro.rID;
            req["count"] = ro.count;
            output_arr.append(req);
        }
        st_dict["resource_output"] = output_arr;

        // Use the structure ID as the dictionary key
        registry_dict[st.structureID] = st_dict;
    }

    return registry_dict;
}

auto build_mesh_dict = [](const MeshData& mesh_data) -> Dictionary {
    PackedVector2Array vertices;
    PackedVector2Array uvs;
    PackedInt32Array indices;
    PackedInt32Array type_ids;

    vertices.resize(static_cast<int64_t>(mesh_data.vertices.size()));
    uvs.resize(static_cast<int64_t>(mesh_data.vertices.size()));
    indices.resize(static_cast<int64_t>(mesh_data.indices.size()));
    type_ids.resize(static_cast<int64_t>(mesh_data.vertices.size()));

    for (size_t i = 0; i < mesh_data.vertices.size(); ++i) {
        vertices[static_cast<int32_t>(i)] = Vector2(mesh_data.vertices[i].position.x, mesh_data.vertices[i].position.y);
        uvs[static_cast<int32_t>(i)] = Vector2(mesh_data.vertices[i].uv.x, mesh_data.vertices[i].uv.y);
        type_ids[static_cast<int32_t>(i)] = static_cast<int32_t>(mesh_data.vertices[i].type_id);
        indices[static_cast<int32_t>(i)] = static_cast<int32_t>(mesh_data.indices[i]);
    }

    Dictionary dict;
    dict["vertices"] = vertices;
    dict["uvs"] = uvs;
    dict["indices"] = indices;
    dict["type_ids"] = type_ids;
    return dict;
};

Dictionary GodotSimulation::getMeshData()
{
    Dictionary chunk_map;
    const auto& all_native_meshes = world_grid_.getMeshData();
    int num_chunk_x = world_grid_.get_num_chunk_x();

    for (size_t chunk_idx = 0; chunk_idx < all_native_meshes.size(); chunk_idx++) {
        Vector2i key(static_cast<int>(chunk_idx) % num_chunk_x, static_cast<int>(chunk_idx) / num_chunk_x);
        
        const ChunkMeshData& chunk_data = all_native_meshes[chunk_idx];
        Dictionary chunk_dict;
        chunk_dict["terrain"] = build_mesh_dict(chunk_data.terrain_mesh);
        chunk_dict["structure"] = build_mesh_dict(chunk_data.structure_mesh);

        chunk_map[key] = chunk_dict;
    }

    return chunk_map;
}

Array GodotSimulation::updateMeshData()
{
    Array updated_chunks;
    auto updated_native_meshes = world_grid_.updateMesh();

    for (const auto& item : updated_native_meshes) {
        const std::array<int, 2>& coords = item.first;
        const ChunkMeshData& chunk_data = item.second;

        Dictionary chunk_dict;
        chunk_dict["chunk_pos"] = Vector2i(coords[0], coords[1]);
        chunk_dict["terrain"] = build_mesh_dict(chunk_data.terrain_mesh);
        chunk_dict["structure"] = build_mesh_dict(chunk_data.structure_mesh);

        updated_chunks.append(chunk_dict);
    }

    return updated_chunks;
}

void GodotSimulation::setDensity(int x, int y, float radius, float strength, uint16_t type_id)
{
    world_grid_.set_density(x,y,radius, strength, type_id);
}

void GodotSimulation::fillCell(int x, int y, uint16_t type_id, bool negativeFill)
{
    world_grid_.fill_cell(x,y, type_id, negativeFill);
}


void GodotSimulation::placeStructure(int x, int y, int structureID)
{
    world_grid_.placeStructure(x,y,structureID);
}

void GodotSimulation::selectStructure(int x, int y)
{
    struct_template st = world_grid_.selectStructure(x,y);

    godot::String dump = "--- Selected Structure Dump ---\n";

    char buffer[256];

    snprintf(buffer, sizeof(buffer), "ID: %u | Size: %dx%d | Tick: %s | Time: %.1f | Type: %s\n",
             st.structureID, st.x_width, st.y_width, 
             (st.canTick ? "yes" : "no"), st.processing_time,
             (st.isBuilding ? "Building": "Block"));

    dump += godot::String(buffer);
    trigger_test_signal(dump);

} 
