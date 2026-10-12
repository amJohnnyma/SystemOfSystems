#ifndef WORLD_TYPES_H
#define WORLD_TYPES_H

#include "math_types.h"
#include <cstdint>
#include <vector>
struct Cell
{
    uint16_t type_id = 0; // material type
    uint8_t density = 0; // 0 - 255
    uint8_t state_flags = 0; // environmental flags
    int entity_on_cell_uID = -1; // there is an entity on this cell -> we store the uniqueID
};

struct Vertex
{
    Vec2 position;
    Vec2 uv;
    uint16_t type_id;
};

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

struct ChunkMeshData
{
    MeshData terrain_mesh;
    MeshData structure_mesh;
};



struct LineSegment {Vertex p1; Vertex p2;};

#endif
