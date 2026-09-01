
#include "godot_world.h"
#include <cstdint>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <map>
#include <array>

using namespace godot;

void GodotSimulation::_bind_methods() {
    ClassDB::bind_method(D_METHOD("createWorld", "world_x", "world_y", "g_size"), &GodotSimulation::createWorld);
    ClassDB::bind_method(D_METHOD("getMeshData"), &GodotSimulation::getMeshData);
    ClassDB::bind_method(D_METHOD("setDensity", "x", "y", "radius", "strength", "type_id"), &GodotSimulation::setDensity);
    ClassDB::bind_method(D_METHOD("updateMeshData"), &GodotSimulation::updateMeshData);
    ClassDB::bind_method(D_METHOD("fillCell", "x", "y", "type_id", "negativeFill"), &GodotSimulation::fillCell);
}

GodotSimulation::GodotSimulation() {}
GodotSimulation::~GodotSimulation() {}

void GodotSimulation::_process(double delta)
{
}

void GodotSimulation::createWorld(int world_x, int world_y, int g_size)
{
    world_grid_ = WorldGrid(); 
    world_grid_.init(world_x, world_y, g_size);
}

// this is slow so lets make sure there is a method for only modifying parts of the mesh
Dictionary GodotSimulation::getMeshData()
{
    Dictionary chunk_map;
    auto all_native_meshes = world_grid_.getMeshData();

    // Helper lambda to format a single MeshData into Godot arrays
    auto build_chunk_dict = [](const MeshData& mesh_data) -> Dictionary {
        PackedVector2Array vertices;
        PackedInt32Array indices;
        PackedInt32Array type_ids;

        vertices.resize(static_cast<int64_t>(mesh_data.vertices.size()));
        indices.resize(static_cast<int64_t>(mesh_data.indices.size()));
        type_ids.resize(static_cast<int64_t>(mesh_data.vertices.size()));

        for (size_t i = 0; i < mesh_data.vertices.size(); ++i) {
            vertices[static_cast<int32_t>(i)] = Vector2(mesh_data.vertices[i].position.x, mesh_data.vertices[i].position.y);
            type_ids[static_cast<int32_t>(i)] = static_cast<int32_t>(mesh_data.vertices[i].type_id);

            indices[static_cast<int32_t>(i)] = static_cast<int32_t>(mesh_data.indices[i]);
        }


        Dictionary dict;
        dict["vertices"] = vertices;
        dict["indices"] = indices;
        dict["type_ids"] = type_ids;
        return dict;
    };

    for (const auto& [coords, mesh_data] : all_native_meshes) {
        Vector2i key(coords[0], coords[1]);
        chunk_map[key] = build_chunk_dict(mesh_data);
    }

    return chunk_map;
}

Array GodotSimulation::updateMeshData()
{
    Array updated_chunks;
    auto updated_native_meshes = world_grid_.updateMesh();

    for (const auto& item : updated_native_meshes) {
        const std::array<int, 2>& coords = item.first;
        const MeshData& mesh_data = item.second;

        PackedVector2Array vertices;
        PackedInt32Array indices;
        PackedInt32Array type_ids;

        vertices.resize(static_cast<int64_t>(mesh_data.vertices.size()));
        indices.resize(static_cast<int64_t>(mesh_data.indices.size()));
        type_ids.resize(static_cast<int64_t>(mesh_data.vertices.size()));

        for (size_t i = 0; i < mesh_data.vertices.size(); ++i) {
            vertices[static_cast<int32_t>(i)] = Vector2(mesh_data.vertices[i].position.x, mesh_data.vertices[i].position.y);
            type_ids[static_cast<int32_t>(i)] = static_cast<int32_t>(mesh_data.vertices[i].type_id);

            indices[static_cast<int32_t>(i)] = static_cast<int32_t>(mesh_data.indices[i]);
        }


        Dictionary chunk_dict;
        chunk_dict["chunk_pos"] = Vector2i(coords[0], coords[1]);
        chunk_dict["vertices"] = vertices;
        chunk_dict["indices"] = indices;
        chunk_dict["type_ids"] = type_ids;

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
