#ifndef ITEM_H
#define ITEM_H


#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "math_types.h"

// An item can have more than one action.
// For example, a pickaxe might both mine and attack.
enum class ItemActionType : uint8_t
{
    Attack,
    ModifyTerrain,
    PlaceBlock,
    PlaceStructure,
    Consume,
    SpawnProjectile,
    ThrowItem
};

enum class ItemCategory : uint8_t
{
    Material,
    Tool,
    Weapon,
    Consumable,
    Building,
    Ammunition,
    Miscellaneous
};

// An event occurring during an item-use animation.
enum class ItemEventType : uint8_t
{
    ApplyAction,   // Deal damage, mine, place, etc.
    SpawnEffect,   // Request particles or another visual effect
    PlaySound
};

// Resolved atlas reference, shared in JSON by name but stored by value here.
// A 16x16 icon atlas only adds a few bytes of numeric metadata per item.
struct ItemSpriteSheet
{
    std::string path;             // Godot resource path, e.g. res://game/atlas/items.png
    uint32_t frame_width_px = 16; // Grid cell width in source pixels.
    uint32_t frame_height_px = 16;
    uint32_t icon_frame = 0;      // Zero-based, row-major frame used by inventory UI.
};
struct ItemAnimation
{
    // Animation identifier, e.g. "swing" or "shoot".
    std::string name;
  // Inclusive zero-based atlas frame range. For a static animation,
    // set both to the same number (e.g. 3..3).
    uint32_t start_frame = 0;
    uint32_t end_frame = 0;

    // Optional explicit playback order. When nonempty, overrides the range.
    // E.g. [3,3,3,3,9] = four static poses then one effect frame.
    std::vector<uint32_t> frame_sequence;


    // Complete animation length in seconds.
    float duration = 0.25f;

    bool loop = false;

    // Offset relative to the player's hand,
    // expressed in world cells.
    Vec2 hand_offset = {0.0f, 0.0f};
};

struct ItemActionEvent
{
    ItemEventType type = ItemEventType::ApplyAction;

    // 0.0 = animation start, 1.0 = animation end.
    float normalized_time = 0.5f;

    // Used by SpawnEffect and PlaySound.
    // Example: "pickaxe_sparks".
    std::string resource_id;
};

struct ItemAction
{
    ItemActionType type = ItemActionType::Attack;

    // Maximum interaction distance in world cells.
    float range = 3.0f;

    // Delay before this action can be used again.
    float cooldown = 0.25f;

    // Animation selected when using this action.
    std::string animation = "use";

    // Combat

    float damage = 0.0f;

    // Can later support damage types, knockback,
    // armor penetration, critical hits, etc.
    float knockback = 0.0f;

    // Terrain editing

    // Negative = remove density.
    // Positive = add density.
    float density_change = 0.0f;

    // Radius of the terrain modification.
    float radius = 1.0f;

    // Material to add when modifying terrain.
    // May be unused when mining.
    uint16_t terrain_material_id = 0;

    // Placement

    // References existing structure or block registry.
    // It is NOT necessarily the same as the item ID.
    uint32_t place_type_id = 0;

    // Projectiles

    // References a future projectile definition.
    uint32_t projectile_type_id = 0;

    // Inventory

    // Number of items consumed on successful use.
    uint32_t consume_count = 0;

    // Durability lost on successful use.
    uint32_t durability_cost = 0;

    // Animation events, including the moment at which
    // gameplay changes are actually applied.
    std::vector<ItemActionEvent> events;
};

struct ItemDefinition
{
    // Unique registry ID. 0 is reserved for empty slots.
    uint32_t iID = 0;

    std::string name;
    std::string description;

    ItemCategory category = ItemCategory::Miscellaneous;

    // Inventory

    uint32_t max_stack = 99;

    // Whether an item may occupy a hotbar slot.
    bool can_go_hotbar = true;

    // Optional weight, for a future weight-based system.
    float weight = 0.0f;

    // Maximum durability.
    // 0 means this item has no durability.
    uint32_t max_durability = 0;

    // Visuals

    // Icon used for the inventory UI.
    std::string icon;
    // Preferred when icon is empty; contains a resolved atlas path, frame size,
    // and selected idle/inventory frame. No animation is played in the UI.
    ItemSpriteSheet sprite_sheet;

    // Texture used for a dropped item in the world.
    std::string world_sprite;

    // Physical size of the dropped item, in world cells.
    // Does not determine inventory slot usage. // or it could, maybe if one day i decide that a grid based inventory would work
    Vec2 world_size = {1.0f, 1.0f};
    uint32_t structure_id = 0;

    // Functionality

    // An item may define multiple actions.
    // No actions = ordinary inventory material.
    std::vector<ItemAction> actions;

    // Item-specific held animations.
    std::unordered_map<std::string, ItemAnimation> animations;
};
using ItemRegistry = std::unordered_map<uint32_t, ItemDefinition>;
struct ItemUseState
{
    bool active = false;

    uint32_t item_id = 0;
    uint32_t action_index = 0;

    float elapsed = 0.0f;

    // Next event that has not yet been processed.
    // Prevents applying damage repeatedly.
    size_t next_event_index = 0;
};


#endif
