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

struct EntityTickContext
{
    double deltaTime;
    const std::vector<Cell>& world_cells;
    int world_x;
    int world_y;
    float threshold;
    Vec2 playerPos; // will also need the size but for now this is good


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
    const WorldEventContext& world;
};

struct FrameRenderData
{
    float renderWidth = 0.0f;
    float renderHeight = 0.0f;

    float offsetX = 0.0f;
    float offsetY = 0.0f;
};

struct AttackHitboxData
{
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
};
struct PlayerSpriteSettings
{
    std::string filename;
    int columns = 1;
    int rows = 1;
    double animationSpeed = 1.0;
    // default render data for the whole animation
    FrameRenderData defaultRender;

    // optional per-frame overrides
    std::unordered_map<int, FrameRenderData> frameRenderOverrides;

    // optional per-frame attack hitboxes
    std::unordered_map<int, std::vector<AttackHitboxData>> frameHitboxes;
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
    float gravity = 10.f;

    // Physics
    Vec2 size = {2.0f, 3.0f};

    // Rendering
    std::unordered_map<std::string, PlayerSpriteSettings> sprites;
};

// Position is in CELL units, with x at the LEFT of the hitbox and y at
// the BOTTOM OCCUPIED CELL ORIGIN (same convention as the existing Player).
// AABB: [x, x + size.x) x [y - size.y + 1, y + 1).
struct MovementState
{
    Vec2 position = {0.0f, 0.0f};
    Vec2 velocity = {0.0f, 0.0f};
    Vec2 size = {1.0f, 1.0f};
    bool isGrounded = false;
};

// Non-owning, read-only view of the world. Callers must keep cells alive
// throughout the call; no copy of the world or its individual Cells is made.
struct MovementWorld
{
    const std::vector<Cell>& cells;
    int width;
    int height;
    float solidThreshold;
};

struct MovementResult
{
    MovementState state;
    bool hitWall = false;
    bool hitCeiling = false;
    bool landed = false; // Collided while moving downward this frame.
};

enum attack_type
{
    DIRECT, // straight on player
    ARC,
    LINE,
    AEO,
    HEAL
};


struct attack
{
    std::string name = "";
    float damage = 0;
    float range = 0;
    attack_type at = DIRECT;
    float cooldownBeforeNextAttack = 0; // cooldown before any other attack
};

struct attack_pattern
{
    std::string name = "";
    std::vector<attack> attacks;
    
};

struct EntityAnimation
{
    std::string sprite = "";
    int cols = 1;
    int rows = 1;
    double animationSpeed = 1.0;
};
struct entity_template
{
    uint32_t entityTypeID = 0;
    uint32_t instanceID = 0;
    
    std::string name = "";
    std::string sprite = "";
    std::string entity_class = "enemy"; // "player", "enemy", "item", "particle"
    std::string behavior_type = "idle";
 std::unordered_map<std::string, EntityAnimation> animations;

    
    float max_health = 100.0f;
    float move_speed = 5.0f;
    int width = 2;
    int height = 2;

    int x_origin = 0;
    int y_origin = 0;

    float x_smooth = 0;
    float y_smooth = 0;

    bool isGrounded = true;

    Vec2 velocity = Vec2{0,0};

    float gravity = 0;
    float maxFallSpeed = 0;
    float jumpStrength = 0;
    
    
    // Optional item/particle fields
    bool is_pickupable = false;

    std::vector<attack_pattern> attack_patterns; // The various attack patterns for different phases, enemy tactic, etc.

};





struct LineSegment {Vertex p1; Vertex p2;};

#endif
