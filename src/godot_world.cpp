

#include "godot_world.h"
#include "world_grid.h"
#include <charconv>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <cstdint>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
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
    ClassDB::bind_method(D_METHOD("movePlayer", "input", "jump_pressed", "deltaTime"), &GodotSimulation::movePlayer);
    ClassDB::bind_method(D_METHOD("getPlayerData"), &GodotSimulation::getPlayerData);
    ClassDB::bind_method(D_METHOD("createPlayer", "data"), &GodotSimulation::createPlayer);
    ADD_SIGNAL(MethodInfo("update_entities_event", PropertyInfo(Variant::DICTIONARY, "batch_data")));
godot::ClassDB::bind_method(
    godot::D_METHOD("set_entity_render_configs", "configs"),
    &GodotSimulation::setEntityRenderConfigs
);

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
/*
   void GodotSimulation::_process(double delta)
   {
   uint32_t flags =  world_grid_.tick(delta);
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
*/

void GodotSimulation::setEntityRenderConfigs(
    const godot::Dictionary& configs)
{
    entity_render_configs_ = configs.duplicate(true);
}

// Writes one 2D instance:
// 8 transform floats + 4 custom-data floats.
//
// Requires:
// MultiMesh.TRANSFORM_2D
// use_colors = false
// use_custom_data = true
static void writeEntityInstance(
    float* dst,
    const Entity& entity,
    const godot::Dictionary& config)
{
    const auto& et = entity.getTemplate();

    const float frame_w =
        static_cast<float>(config.get("frame_width_px", 1.0));

    const float frame_h =
        static_cast<float>(config.get("frame_height_px", 1.0));

    const float pixels_per_cell = std::max(
        0.0001f,
        static_cast<float>(
            config.get("visual_pixels_per_cell", 30.0)
        )
    );

    const godot::Vector2 pivot =
        config.get("visual_pivot", godot::Vector2(0.5f, 0.0f));

    const int cols = std::max(
        1, static_cast<int>(config.get("cols", 1))
    );

    const int rows = std::max(
        1, static_cast<int>(config.get("rows", 1))
    );

    // Sprite dimensions in world cells.
    const float visual_w = frame_w / pixels_per_cell;
    const float visual_h = frame_h / pixels_per_cell;

    const float physical_w = static_cast<float>(et.width);

    const float grid_x = static_cast<float>(et.x_smooth);
    const float grid_y = static_cast<float>(et.y_smooth);

    const bool facing_right = entity.isFacingRight();

    // Physical horizontal center plus visual pivot adjustment.
    const float direction = facing_right ? 1.0f : -1.0f;

    const float center_x =
        grid_x + physical_w * 0.5f +
        direction * (0.5f - pivot.x) * visual_w;

    // Physical bottom is grid_y + 1.
    // Negative Y is upward in the existing entity convention.
    const float center_y =
        grid_y + 1.0f -
        (0.5f - pivot.y) * visual_h;

    const float scale_x = direction * visual_w;

    const float scale_y = -visual_h;

    // Transform2D packed in Godot's required row-major layout.
    dst[0] = scale_x;
    dst[1] = 0.0f;
    dst[2] = 0.0f; // padding
    dst[3] = center_x;

    dst[4] = 0.0f;
    dst[5] = scale_y;
    dst[6] = 0.0f; // padding
    dst[7] = center_y;

    // Animation frame selection.
    const int total_frames = cols * rows;

    const float phase =
        static_cast<float>(entity.getAnimProgress());

    int frame = static_cast<int>(
        std::floor(phase * total_frames)
    );

    frame = ((frame % total_frames) + total_frames)
        % total_frames;

    const int frame_col = frame % cols;
    const int frame_row = frame / cols;

    // INSTANCE_CUSTOM = (col, row, cols, rows)
    dst[8]  = static_cast<float>(frame_col);
    dst[9]  = static_cast<float>(frame_row);
    dst[10] = static_cast<float>(cols);
    dst[11] = static_cast<float>(rows);
}


godot::Dictionary
GodotSimulation::getEntityRenderBuffers() const
{
    godot::Dictionary result;

    const auto& entities = world_grid_.getEntities();

    std::unordered_map<
        std::string,
        std::vector<const Entity*>
    > groups;

    // Group entities by type and animation state.
    for (const auto& [uid, entity] : entities)
    {
        const auto& et = entity.getTemplate();

        const std::string key =
            std::to_string(et.entityTypeID) +
            "_" + entity.getState();

        groups[key].push_back(&entity);
    }

    for (const auto& [key, group] : groups)
    {
        const godot::String godot_key(key.c_str());

        if (!entity_render_configs_.has(godot_key))
            continue;

        const godot::Dictionary config =
            entity_render_configs_[godot_key];

        const int64_t count =
            static_cast<int64_t>(group.size());

        godot::PackedFloat32Array buffer;
        buffer.resize(count * 12);

        float* dst = buffer.ptrw();

        float min_x = std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();

        for (int64_t i = 0; i < count; ++i)
        {
            float* instance = dst + i * 12;

            writeEntityInstance(
                instance,
                *group[i],
                config
            );

            // Packed Transform2D:
            // [0] = scale X
            // [5] = scale Y
            // [3] = translation X
            // [7] = translation Y

            // Assumes the mesh is a centered 1x1 quad.
            const float half_w =
                std::abs(instance[0]) * 0.5f;

            const float half_h =
                std::abs(instance[5]) * 0.5f;

            const float x = instance[3];
            const float y = instance[7];

            min_x = std::min(min_x, x - half_w);
            max_x = std::max(max_x, x + half_w);

            min_y = std::min(min_y, y - half_h);
            max_y = std::max(max_y, y + half_h);
        }

        // Small visibility margin in local coordinates.
        constexpr float MARGIN = 2.0f;

        godot::AABB bounds(
            godot::Vector3(
                min_x - MARGIN,
                min_y - MARGIN,
                -1.0f
            ),
            godot::Vector3(
                max_x - min_x + MARGIN * 2.0f,
                max_y - min_y + MARGIN * 2.0f,
                2.0f
            )
        );

        godot::Dictionary group_data;
        group_data["buffer"] = buffer;
        group_data["aabb"] = bounds;

        result[godot_key] = group_data;
    }

    return result;
}
#include <chrono>
#include <iostream>

void GodotSimulation::_process(double delta)
{
    using Clock = std::chrono::steady_clock;

    static double simulation_total = 0.0;
    static double dictionary_total = 0.0;
    static double signal_total = 0.0;
    static int samples = 0;

    auto t0 = Clock::now();

    uint32_t flags = world_grid_.tick(delta);

    auto t1 = Clock::now();

    if (flags & TickFlags::MESH_UPDATE)
    {
        update_godot_mesh_signal();
    }

    auto t2 = Clock::now();

    if (flags & TickFlags::ENTITY_UPDATE)
    {
        godot::Dictionary data =
            getEntityRenderBuffers();

        auto t3 = Clock::now();

        emit_signal("update_entities_event", data);

        auto t4 = Clock::now();

        auto ms = [](auto a, auto b)
        {
            return std::chrono::duration<double, std::milli>(
                    b - a
                    ).count();
        };

        simulation_total += ms(t0, t1);
        dictionary_total += ms(t2, t3);
        signal_total += ms(t3, t4);

        ++samples;

        if (samples >= 300)
        {
        emit_signal(
            "test_signal_event",
            godot::vformat(
                "--- Entity Performance ---\n"
                "WorldGrid tick: %.3f ms\n"
                "Render buffer creation: %.3f ms\n"
                "Signal + GDScript: %.3f ms",
                simulation_total / samples,
                dictionary_total / samples,
                signal_total / samples
            )
        );

            simulation_total = 0.0;
            dictionary_total = 0.0;
            signal_total = 0.0;
            samples = 0;
        }
    }
}
/*
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

*/
godot::Dictionary
GodotSimulation::getEntityTransformsForRendering() const
{
    godot::Dictionary batch_data;

    const auto& entities = world_grid_.getEntities();

    for (const auto& [uID, entity] : entities)
    {
        const auto& et = entity.getTemplate();

        godot::Dictionary pos_dict;

        pos_dict["x_smooth"] = et.x_smooth;
        pos_dict["y_smooth"] = et.y_smooth;
        pos_dict["x_origin"] = et.x_origin;
        pos_dict["y_origin"] = et.y_origin;

        pos_dict["entityTypeID"] = et.entityTypeID;

        pos_dict["width"] = et.width;
        pos_dict["height"] = et.height;

        pos_dict["state"] =
            entity.getState().c_str();

        pos_dict["animPhase"] =
            entity.getAnimProgress();

        pos_dict["facingRight"] =
            entity.isFacingRight();

        batch_data[uID] = pos_dict;
    }

    return batch_data;
}

namespace
{
    std::string toStdString(const godot::String& str)
    {
        return std::string(str.utf8().get_data());
    }

    double readNumber(
            const godot::Variant& value,
            double fallback
            )
    {
        if (value.get_type() == godot::Variant::INT ||
                value.get_type() == godot::Variant::FLOAT)
        {
            return static_cast<double>(value);
        }

        return fallback;
    }

    double readNumber(
            const godot::Dictionary& dict,
            const char* key,
            double fallback
            )
    {
        return readNumber(dict.get(key, fallback), fallback);
    }

    Vec2 readVec2(
            const godot::Variant& value,
            Vec2 fallback
            )
    {
        // Godot Vector2
        if (value.get_type() == godot::Variant::VECTOR2)
        {
            godot::Vector2 v = value;
            return Vec2{v.x, v.y};
        }

        // JSON object: {"x": 320, "y": 10}
        if (value.get_type() == godot::Variant::DICTIONARY)
        {
            godot::Dictionary d = value;

            return Vec2{
                static_cast<float>(
                        readNumber(d, "x", fallback.x)
                        ),
                    static_cast<float>(
                            readNumber(d, "y", fallback.y)
                            )
            };
        }

        // JSON array: [320, 10]
        if (value.get_type() == godot::Variant::ARRAY)
        {
            godot::Array a = value;

            if (a.size() >= 2)
            {
                return Vec2{
                    static_cast<float>(
                            readNumber(a[0], fallback.x)
                            ),
                        static_cast<float>(
                                readNumber(a[1], fallback.y)
                                )
                };
            }
        }

        return fallback;
    }
}
void GodotSimulation::createPlayer(godot::Dictionary data)
{
    PlayerSettings settings;

    // General
    if (data.has("name"))
    {
        godot::String name = data["name"];
        settings.name = toStdString(name);
    }

    settings.spawnPosition = readVec2(
            data.get("spawnPosition", godot::Variant()),
            settings.spawnPosition
            );
    // Health
    settings.maxHealth = readNumber(
            data, "maxHealth", settings.maxHealth
            );

    // Movement
    settings.maxMoveSpeed = readNumber(
            data, "maxMoveSpeed", settings.maxMoveSpeed
            );

    settings.jumpSpeed = readNumber(
            data, "jumpSpeed", settings.jumpSpeed
            );

    settings.maxFallSpeed = readNumber(
            data, "maxFallSpeed", settings.maxFallSpeed
            );

    // Physics
    settings.size = readVec2(
            data.get("size", godot::Variant()),
            settings.size
            );

    settings.gravity = readNumber(
            data, "gravity", settings.gravity
            );



    // Sprite settings
    godot::Variant sprites_value = data.get(
            "sprites", godot::Dictionary()
            );

    if (sprites_value.get_type() == godot::Variant::DICTIONARY)
    {
        godot::Dictionary sprites = sprites_value;
        godot::Array states = sprites.keys();

        for (int i = 0; i < states.size(); ++i)
        {
            godot::Variant key = states[i];

            if (key.get_type() != godot::Variant::STRING)
                continue;

            godot::String state_name = key;

            godot::Variant sprite_value = sprites[key];

            if (sprite_value.get_type() != godot::Variant::DICTIONARY)
                continue;

            godot::Dictionary sprite_data = sprite_value;

            PlayerSpriteSettings sprite;

            godot::Variant filename_value = sprite_data.get(
                    "filename", godot::String("")
                    );

            if (filename_value.get_type() != godot::Variant::STRING)
                continue;

            godot::String filename = filename_value;
            sprite.filename = toStdString(filename);

            sprite.columns = std::max(
                    1,
                    static_cast<int>(
                        readNumber(sprite_data, "columns", 1)
                        )
                    );

            sprite.rows = std::max(
                    1,
                    static_cast<int>(
                        readNumber(sprite_data, "rows", 1)
                        )
                    );

            sprite.animationSpeed = readNumber(
                    sprite_data, "animationSpeed", 1.0
                    );

            settings.sprites.emplace(
                    toStdString(state_name),
                    std::move(sprite)
                    );
        }
    }

    // Create player
    world_grid_.createPlayer(std::move(settings));
}
void GodotSimulation::updatePlayerPosition(float x, float y) {
    world_grid_.updatePlayerPosition(x, y);
}
void GodotSimulation::movePlayer(Vector2 input, bool jump_pressed, double deltaTime) 
{
    world_grid_.movePlayer(Vec2{input.x, input.y}, jump_pressed, deltaTime);
}
godot::Dictionary GodotSimulation::getPlayerData() const
{
    godot::Dictionary data;

    const auto& player = world_grid_.getPlayer();

    const Vec2& pos = player.getPosition();
    const Vec2& vel = player.getVelocity();
    const Vec2& size = player.getSize();

    // Position
    data["x_smooth"] = pos.x;
    data["y_smooth"] = pos.y;

    // Dimensions
    data["width"] = size.x;
    data["height"] = size.y;

    // Velocity
    data["velocity_x"] = vel.x;
    data["velocity_y"] = vel.y;

    // Physics
    data["isGrounded"] = player.isGrounded();

    // Health
    data["currentHealth"] = player.getCurrentHealth();
    data["maxHealth"] = player.getMaxHealth();

    // Movement
    data["currentMoveSpeed"] = player.getCurrentMoveSpeed();
    data["maxMoveSpeed"] = player.getMaxMoveSpeed();

    // Rendering
    data["sprite"] = player.getSprite().c_str();
    data["state"] = player.getState().c_str();
    data["animPhase"] = player.getAnimProgress();
    data["facingRight"] = player.isFacingRight();

    return data;
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
