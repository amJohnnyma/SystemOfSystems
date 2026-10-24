
#include "godot_world.h"
#include "world_grid.h"
#include <charconv>
#include <cstdint>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

using namespace godot;

void GodotSimulation::_bind_methods() {
    ClassDB::bind_method(D_METHOD("createWorld", "world_x", "world_y", "g_size", "threshold"), &GodotSimulation::createWorld);
    ClassDB::bind_method(D_METHOD("getMeshData"), &GodotSimulation::getMeshData);
    ClassDB::bind_method(D_METHOD("setDensity", "x", "y", "radius", "strength", "type_id"), &GodotSimulation::setDensity);
    ClassDB::bind_method(D_METHOD("updateMeshData"), &GodotSimulation::updateMeshData);
    ClassDB::bind_method(D_METHOD("fillCell", "x", "y", "type_id", "negativeFill"), &GodotSimulation::fillCell);
    ClassDB::bind_method(D_METHOD("getStructureRegistry"), &GodotSimulation::getStructureRegistry);
    ClassDB::bind_method(D_METHOD("getEntityRegistry"), &GodotSimulation::getEntityRegistry);
    ClassDB::bind_method(D_METHOD("placeStructure", "x", "y", "structureID"), &GodotSimulation::placeStructure);
    ClassDB::bind_method(D_METHOD("placeEntity", "x", "y", "entityID"), &GodotSimulation::placeEntity);
    ClassDB::bind_method(D_METHOD("selectStructure", "x", "y"), &GodotSimulation::selectStructure);



    ClassDB::bind_method(D_METHOD("trigger_test_signal", "message"), &GodotSimulation::trigger_test_signal);
    ADD_SIGNAL(MethodInfo("test_signal_event", PropertyInfo(Variant::STRING, "message")));


    ClassDB::bind_method(D_METHOD("update_godot_mesh_signal"), &GodotSimulation::trigger_test_signal);
    ADD_SIGNAL(MethodInfo("update_godot_mesh_event"));

ClassDB::bind_method(D_METHOD("updatePlayerPosition", "x", "y"), &GodotSimulation::updatePlayerPosition);
    ClassDB::bind_method(D_METHOD("resolveCollision", "current_pos", "velocity", "width", "height", "delta"), &GodotSimulation::resolveCollision);
ADD_SIGNAL(MethodInfo("update_entities_event", PropertyInfo(Variant::DICTIONARY, "batch_data")));

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


    if (flags & TickFlags::ENTITY_UPDATE)
    {
        emit_signal("update_entities_event", getEntityTransformsForRendering());
    }
}
/*
godot::Dictionary GodotSimulation::getEntityTransformsForRendering() const {
    godot::Dictionary batch_data;
    
    // Call down to your world grid or entity registry to gather active entities
    auto const& entities = world_grid_.getEntities(); // Or however you store them
    
    for (auto const& [uID, entity] : entities) {
        auto const& et = entity.getTemplate();

        godot::Dictionary pos_dict;
        pos_dict["x_origin"] = et.x_origin;
        pos_dict["y_origin"] = et.y_origin;
        pos_dict["x_smooth"] = et.x_smooth;
        pos_dict["y_smooth"] = et.y_smooth;
        pos_dict["width"] = et.width;
        pos_dict["height"] = et.height;
        pos_dict["entityTypeID"] = et.entityTypeID;

        batch_data[uID] = pos_dict;
    }
    return batch_data;
}
*/
godot::Dictionary GodotSimulation::getEntityTransformsForRendering() const {
    godot::Dictionary batch_data;
    auto const& entities = world_grid_.getEntities(); 
    
    // 1. Copy into a vector so we can sort by uID for stable indices
    std::vector<std::pair<uint32_t, const Entity*>> sorted_entities;
    sorted_entities.reserve(entities.size());
    for (auto const& [uID, entity] : entities) {
        sorted_entities.push_back({uID, &entity});
    }
    
    // 2. Sort deterministically by instance ID (uID)
    std::sort(sorted_entities.begin(), sorted_entities.end(), [](auto const& a, auto const& b) {
        return a.first < b.first;
    });

    // 3. Populate dictionary in stable order
    for (auto const& [uID, entity_ptr] : sorted_entities) {
        auto const& et = entity_ptr->getTemplate();

        godot::Dictionary pos_dict;
        pos_dict["x_smooth"] = et.x_smooth;
        pos_dict["y_smooth"] = et.y_smooth;
        pos_dict["x_origin"] = et.x_origin;
        pos_dict["y_origin"] = et.y_origin;
        pos_dict["entityTypeID"] = et.entityTypeID;
        pos_dict["width"] = et.width;
        pos_dict["height"] = et.height;

        pos_dict["state"] = entity_ptr->getState().c_str();             // Current state string (matches keys in entity.json animations)
        pos_dict["animPhase"] = entity_ptr->getAnimProgress();            // Progress from 0.0 to 1.0 across the animation frames
        pos_dict["facingRight"] = entity_ptr->isFacingRight();

        batch_data[uID] = pos_dict;
    }
    return batch_data;
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

void GodotSimulation::createWorld(int world_x, int world_y, int g_size, float threshold)
{

    world_grid_ = WorldGrid(); 
    world_grid_.init(world_x, world_y, g_size,threshold );

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
godot::Dictionary GodotSimulation::getEntityRegistry() const {
    godot::Dictionary registry_dict;
    auto const& er = world_grid_.getEntityRegistry(); // Or entity_registry_, depending on where it's stored

    for (auto it = er.begin(); it != er.end(); ++it) {
        auto const& et = it->second.getTemplate();
        
        godot::Dictionary et_dict;
        et_dict["entityTypeID"] = et.entityTypeID;
        et_dict["name"] = godot::String(et.name.c_str());
        et_dict["sprite"] = godot::String(et.sprite.c_str());
        et_dict["entity_class"] = godot::String(et.entity_class.c_str());
        et_dict["behavior_type"] = godot::String(et.behavior_type.c_str());
        et_dict["max_health"] = et.max_health;
        et_dict["move_speed"] = et.move_speed;
        et_dict["is_pickupable"] = et.is_pickupable;
        et_dict["width"] = et.width;
        et_dict["height"] = et.height;

        // Serialize attack patterns
        godot::Array patterns_arr;
        for (const auto& ap : et.attack_patterns) {
            godot::Dictionary ap_dict;
            ap_dict["name"] = godot::String(ap.name.c_str());

            godot::Array attacks_arr;
            for (const auto& atk : ap.attacks) {
                godot::Dictionary atk_dict;
                atk_dict["name"] = godot::String(atk.name.c_str());
                atk_dict["damage"] = atk.damage;
                atk_dict["range"] = atk.range;
                
                // Convert attack_type enum to string for Godot readability
                const char* at_str = "DIRECT";
                switch (atk.at) {
                    case DIRECT: at_str = "DIRECT"; break;
                    case ARC:    at_str = "ARC"; break;
                    case LINE:   at_str = "LINE"; break;
                    case AEO:    at_str = "AEO"; break;
                    case HEAL:   at_str = "HEAL"; break;
                }
                atk_dict["attack_type"] = godot::String(at_str);
                atk_dict["cooldownBeforeNextAttack"] = atk.cooldownBeforeNextAttack;

                attacks_arr.append(atk_dict);
            }
            ap_dict["attacks"] = attacks_arr;
            patterns_arr.append(ap_dict);
        }
        et_dict["attack_patterns"] = patterns_arr;

        // Use the entityTypeID as the dictionary key
        registry_dict[et.entityTypeID] = et_dict;
    }

    return registry_dict;
}

auto build_mesh_dict = [](const MeshData& mesh_data) -> Dictionary {
    PackedVector2Array vertices;
    PackedVector2Array uvs;
    PackedInt32Array indices;
    PackedInt32Array type_ids;
    PackedColorArray colors;

    vertices.resize(static_cast<int64_t>(mesh_data.vertices.size()));
    uvs.resize(static_cast<int64_t>(mesh_data.vertices.size()));
    indices.resize(static_cast<int64_t>(mesh_data.indices.size()));
    type_ids.resize(static_cast<int64_t>(mesh_data.vertices.size()));
    colors.resize(static_cast<int64_t>(mesh_data.vertices.size()));

for (size_t i = 0; i < mesh_data.vertices.size(); ++i) {
    const auto& vertex = mesh_data.vertices[i];

    vertices[static_cast<int32_t>(i)] =
        Vector2(vertex.position.x, vertex.position.y);

    uvs[static_cast<int32_t>(i)] =
        Vector2(vertex.uv.x, vertex.uv.y);

    type_ids[static_cast<int32_t>(i)] =
        static_cast<int32_t>(vertex.type_id);

    colors[static_cast<int32_t>(i)] = Color(
        vertex.weights.x,
        vertex.weights.y,
        static_cast<float>(vertex.type_id),
        static_cast<float>(vertex.other_type_id)
    );
}

for (size_t i = 0; i < mesh_data.indices.size(); ++i) {
    indices[static_cast<int32_t>(i)] =
        static_cast<int32_t>(mesh_data.indices[i]);
}

    Dictionary dict;
    dict["vertices"] = vertices;
    dict["colors"] = colors;
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


void GodotSimulation::placeEntity(int x, int y, int entityID)
{
    world_grid_.placeEntity(x,y, entityID);
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
