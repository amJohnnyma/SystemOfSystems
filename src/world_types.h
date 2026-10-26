#ifndef WORLD_TYPES_H
#define WORLD_TYPES_H

#include "math_types.h"
#include <cstdint>
#include <vector>
#include <iostream>
#include <unordered_map>

struct Cell
{
    uint16_t type_id = 0; // material type
    uint8_t density = 0; // 0 - 255
    uint8_t state_flags = 0; // environmental flags
    int structure_on_cell_uID = -1; // there is an entity on this cell -> we store the uniqueID
    std::vector<int> entities_on_cell_uID = {}; // multiple entities can be on the same cell
};

struct Vertex
{
    Vec2 position;
    Vec2 uv;
    uint16_t type_id; // primary type
    uint16_t other_type_id; // secondary type
    Vec2 weights; //0 = mine/primary, 1 = other/secondary
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

struct global_tick_settings
{
    float gravity = 9.8f;
    double animationSpeed = 1.25;
    double idleAnimSpeed = 1.0;
};

struct EntityTickContext
{
    double deltaTime;
    const std::vector<Cell>& world_cells;
    int world_x;
    int world_y;
    float threshold;
    Vec2 playerPos; // will also need the size but for now this is good
    global_tick_settings gts;


};
struct WorldEventContext
{
    const std::vector<Cell>& world_cells;
    int world_x;
    int world_y;
    float threshold;
};
struct PlayerTickContext
{
    double deltaTime;
    global_tick_settings gts;
    const WorldEventContext& world;
};

struct PlayerSpriteSettings
{
    std::string filename;
    int columns = 1;
    int rows = 1;
    double animationSpeed = 1.0;
};
struct PlayerSettings
{
    // General
    std::string name = "Player";
    Vec2 spawnPosition = {320.0f, 10.0f};

    // Health
    double maxHealth = 100.0;

    // Movement
    double maxMoveSpeed = 25.0;
    double jumpSpeed = 50.0;
    double maxFallSpeed = 100.0;

    // Physics
    Vec2 size = {2.0f, 3.0f};

    // Rendering
    std::unordered_map<std::string, PlayerSpriteSettings> sprites;
};





struct LineSegment {Vertex p1; Vertex p2;};

#endif
