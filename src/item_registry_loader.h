
#ifndef ITEM_REGISTRY_LOADER_H
#define ITEM_REGISTRY_LOADER_H

#include "item.h"
#include "structure.h"
#include "json_loader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Startup-only helpers. No inventory logic, no Godot dependency, no exceptions.
namespace ItemRegistryLoader {

using Json = nlohmann::json;
constexpr uint32_t STRUCTURE_ITEM_BASE = 100000;

inline const Json* field(const Json& obj, const char* key) {
    if (!obj.is_object()) return nullptr;
    auto it = obj.find(key);
    return it == obj.end() ? nullptr : &(*it);
}

inline std::string stringValue(const Json& obj, const char* key, const std::string& fallback = "") {
    const Json* v = field(obj, key);
    return v && v->is_string() ? v->get<std::string>() : fallback;
}

inline bool boolValue(const Json& obj, const char* key, bool fallback) {
    const Json* v = field(obj, key);
    return v && v->is_boolean() ? v->get<bool>() : fallback;
}

inline uint32_t uintValue(const Json& obj, const char* key, uint32_t fallback) {
    const Json* v = field(obj, key);
    if (!v || !v->is_number_integer()) return fallback;
    if (v->is_number_unsigned()) {
        const auto n = v->get<uint64_t>();
        return n <= std::numeric_limits<uint32_t>::max() ? static_cast<uint32_t>(n) : fallback;
    }
    const auto n = v->get<int64_t>();
    return n >= 0 && static_cast<uint64_t>(n) <= std::numeric_limits<uint32_t>::max()
        ? static_cast<uint32_t>(n) : fallback;
}

inline float floatValue(const Json& obj, const char* key, float fallback) {
    const Json* v = field(obj, key);
    if (!v || !v->is_number()) return fallback;
    const float n = v->get<float>();
    return std::isfinite(n) ? n : fallback;
}

inline Vec2 vec2Value(const Json& obj, const char* key, Vec2 fallback) {
    const Json* v = field(obj, key);
    if (!v || !v->is_array() || v->size() != 2 || !(*v)[0].is_number() || !(*v)[1].is_number())
        return fallback;
    const float x = (*v)[0].get<float>();
    const float y = (*v)[1].get<float>();
    return std::isfinite(x) && std::isfinite(y) ? Vec2{x, y} : fallback;
}

inline ItemCategory parseCategory(const std::string& name) {
    if (name == "Material") return ItemCategory::Material;
    if (name == "Tool") return ItemCategory::Tool;
    if (name == "Weapon") return ItemCategory::Weapon;
    if (name == "Consumable") return ItemCategory::Consumable;
    if (name == "Building") return ItemCategory::Building;
    if (name == "Ammunition") return ItemCategory::Ammunition;
    return ItemCategory::Miscellaneous;
}

inline bool parseActionType(const std::string& name, ItemActionType& type) {
    if (name == "Attack") type = ItemActionType::Attack;
    else if (name == "ModifyTerrain") type = ItemActionType::ModifyTerrain;
    else if (name == "PlaceBlock") type = ItemActionType::PlaceBlock;
    else if (name == "PlaceStructure") type = ItemActionType::PlaceStructure;
    else if (name == "Consume") type = ItemActionType::Consume;
    else if (name == "SpawnProjectile") type = ItemActionType::SpawnProjectile;
    else if (name == "ThrowItem") type = ItemActionType::ThrowItem;
    else return false;
    return true;
}

inline bool parseEventType(const std::string& name, ItemEventType& type) {
    if (name == "ApplyAction") type = ItemEventType::ApplyAction;
    else if (name == "SpawnEffect") type = ItemEventType::SpawnEffect;
    else if (name == "PlaySound") type = ItemEventType::PlaySound;
    else return false;
    return true;
}

// Resolve named atlas definitions from the root of items.json.
// Frame indexes are zero-based and row-major. All numeric values are validated
// before get<T>() because the GDExtension is compiled with -fno-exceptions.
inline std::unordered_map<std::string, ItemSpriteSheet> parseSpriteSheets(const Json& doc) {
    std::unordered_map<std::string, ItemSpriteSheet> sheets;
    const Json* definitions = field(doc, "sprite_sheets");
    if (!definitions || !definitions->is_object()) return sheets;

    for (auto it = definitions->begin(); it != definitions->end(); ++it) {
        if (!it.value().is_object()) continue;
        const Json& src = it.value();
        ItemSpriteSheet sheet;
        sheet.path = stringValue(src, "path");
        const Vec2 frame = vec2Value(src, "frame_size", {16.0f, 16.0f});
        sheet.frame_width_px = static_cast<uint32_t>(std::clamp(frame.x, 1.0f, 4096.0f));
        sheet.frame_height_px = static_cast<uint32_t>(std::clamp(frame.y, 1.0f, 4096.0f));
        if (!sheet.path.empty()) sheets.emplace(it.key(), std::move(sheet));
    }
    return sheets;
}

// Parse only data. Actual use/animation/particle execution is a later phase.
inline void parseAnimations(const Json& obj, ItemDefinition& item) {
    const Json* animations = field(obj, "animations");
    if (!animations || !animations->is_object()) return;
    for (auto it = animations->begin(); it != animations->end(); ++it) {
        if (!it.value().is_object()) continue;
        const Json& src = it.value();
        ItemAnimation anim;
        anim.name = it.key();
        anim.start_frame = uintValue(src, "start_frame", item.sprite_sheet.icon_frame);
        anim.end_frame = uintValue(src, "end_frame", anim.start_frame);
        if (anim.end_frame < anim.start_frame) {
            std::cerr << "[Items] Invalid frame range for item " << item.iID
                      << " animation '" << anim.name << "'; using one frame\n";
            anim.end_frame = anim.start_frame;
        }

        // Optional repeated/noncontiguous frames, e.g. [2,2,2,2,7].
        const Json* sequence = field(src, "frame_sequence");
        if (sequence && sequence->is_array()) {
            for (const Json& frame : *sequence) {
                if (!frame.is_number_integer()) continue;
                uint64_t n = 0;
                if (frame.is_number_unsigned()) n = frame.get<uint64_t>();
                else {
                    const int64_t signed_n = frame.get<int64_t>();
                    if (signed_n < 0) continue;
                    n = static_cast<uint64_t>(signed_n);
                }
                if (n <= std::numeric_limits<uint32_t>::max())
                    anim.frame_sequence.push_back(static_cast<uint32_t>(n));
            }
        }
        anim.duration = std::max(0.001f, floatValue(src, "duration", 0.25f));
        anim.loop = boolValue(src, "loop", false);
        anim.hand_offset = vec2Value(src, "hand_offset", {0.0f, 0.0f});
        item.animations.emplace(anim.name, std::move(anim));
    }
}

inline void parseActions(const Json& obj, ItemDefinition& item) {
    const Json* actions = field(obj, "actions");
    if (!actions || !actions->is_array()) return;
    for (const Json& src : *actions) {
        if (!src.is_object()) continue;
        ItemAction a;
        const std::string type_name = stringValue(src, "type");
        if (!parseActionType(type_name, a.type)) {
            std::cerr << "[Items] Skipping unknown action '" << type_name
                      << "' for item " << item.iID << "\n";
            continue;
        }
        a.range = std::max(0.0f, floatValue(src, "range", a.range));
        a.cooldown = std::max(0.0f, floatValue(src, "cooldown", a.cooldown));
        a.animation = stringValue(src, "animation", a.animation);
        a.damage = floatValue(src, "damage", a.damage);
        a.knockback = floatValue(src, "knockback", a.knockback);
        a.density_change = floatValue(src, "density_change", a.density_change);
        a.radius = std::max(0.0f, floatValue(src, "radius", a.radius));
        a.terrain_material_id = static_cast<uint16_t>(
            std::min(uintValue(src, "terrain_material_id", a.terrain_material_id), 65535u));
        a.place_type_id = uintValue(src, "place_type_id", a.place_type_id);
        a.projectile_type_id = uintValue(src, "projectile_type_id", a.projectile_type_id);
        a.consume_count = uintValue(src, "consume_count", a.consume_count);
        a.durability_cost = uintValue(src, "durability_cost", a.durability_cost);
        const Json* events = field(src, "events");
        if (events && events->is_array()) {
            for (const Json& event_src : *events) {
                if (!event_src.is_object()) continue;
                ItemActionEvent e;
                if (!parseEventType(stringValue(event_src, "type"), e.type)) continue;
                e.normalized_time = std::clamp(floatValue(event_src, "normalized_time", 0.5f), 0.0f, 1.0f);
                e.resource_id = stringValue(event_src, "resource_id");
                a.events.push_back(std::move(e));
            }
            std::stable_sort(a.events.begin(), a.events.end(), [](const auto& l, const auto& r) {
                return l.normalized_time < r.normalized_time;
            });
        }
        item.actions.push_back(std::move(a));
    }
}

inline size_t loadCustomItems(ItemRegistry& registry, const Json& document) {
    // New format: { "sprite_sheets": { ... }, "items": [ ... ] }
    // Legacy plain-array documents are still accepted, without named sheets.
    const Json* item_list = document.is_array() ? &document : field(document, "items");
    if (!item_list || !item_list->is_array()) {
        std::cerr << "[Items] game/items.json requires an items array\n";
        return 0;
    }
    const auto sheets = parseSpriteSheets(document);
    size_t added = 0;
    for (const Json& src : *item_list) {
        if (!src.is_object()) continue;
        const uint32_t id = uintValue(src, "iID", 0);
        if (id == 0 || id >= STRUCTURE_ITEM_BASE) {
            std::cerr << "[Items] Invalid/reserved item ID: " << id << "\n";
            continue;
        }
        ItemDefinition item;
        item.iID = id;
        item.name = stringValue(src, "name", "Item " + std::to_string(id));
        item.description = stringValue(src, "description");
        item.category = parseCategory(stringValue(src, "category", "Miscellaneous"));
        item.max_stack = std::max(1u, uintValue(src, "max_stack", item.max_stack));
        item.can_go_hotbar = boolValue(src, "can_go_hotbar", item.can_go_hotbar);
        item.max_durability = uintValue(src, "max_durability", item.max_durability);
        item.weight = std::max(0.0f, floatValue(src, "weight", item.weight));
        item.icon = stringValue(src, "icon");
        const std::string sheet_name = stringValue(src, "sprite_sheet");
        if (!sheet_name.empty()) {
            const auto found = sheets.find(sheet_name);
            if (found != sheets.end()) item.sprite_sheet = found->second;
            else std::cerr << "[Items] Unknown sprite sheet '" << sheet_name
                           << "' for item " << id << "\n";
        }
        item.sprite_sheet.icon_frame = uintValue(src, "icon_frame", 0);
        item.world_sprite = stringValue(src, "world_sprite");
        item.world_size = vec2Value(src, "world_size", item.world_size);
        if (item.world_size.x <= 0 || item.world_size.y <= 0) item.world_size = {1, 1};
        // Structure-linked items come from structures.json, not items.json.
        item.structure_id = 0;
        parseAnimations(src, item);
        parseActions(src, item);
        if (registry.emplace(id, std::move(item)).second) ++added;
        else std::cerr << "[Items] Duplicate item ID: " << id << "\n";
    }
    return added;
}

// Each rule maps a contiguous blockType range to one atlas file.
// The frame index is LOCAL to that texture, not a global block ID.
struct BlockAtlasRule
{
    std::string name;
    uint32_t first_block_type = 0; // inclusive
    uint32_t last_block_type = 0;  // inclusive
    uint32_t first_frame = 0;      // zero-based index inside the PNG
    ItemSpriteSheet sheet;
};

// Reads game/structure_atlases.json. No Godot dependency and no exceptions.
// Overlapping ranges are rejected to avoid ambiguous material selection.
inline std::vector<BlockAtlasRule> parseBlockAtlases(const Json& document)
{
    std::vector<BlockAtlasRule> rules;
    const Json* entries = field(document, "block_atlases");
    if (!entries || !entries->is_array()) {
        std::cerr << "[Items] structure_atlases.json requires a block_atlases array\n";
        return rules;
    }

    for (const Json& src : *entries) {
        if (!src.is_object()) continue;
        BlockAtlasRule rule;
        rule.name = stringValue(src, "name", "unnamed");
        rule.first_block_type = uintValue(src, "first_block_type", 0);
        rule.last_block_type = uintValue(src, "last_block_type", 0);
        rule.first_frame = uintValue(src, "first_frame", 0);
        rule.sheet.path = stringValue(src, "path");

        const Vec2 size = vec2Value(src, "frame_size", {0.0f, 0.0f});
        if (size.x <= 0.0f || size.y <= 0.0f ||
            size.x > 4096.0f || size.y > 4096.0f ||
            std::floor(size.x) != size.x || std::floor(size.y) != size.y ||
            rule.first_block_type == 0 ||
            rule.last_block_type < rule.first_block_type ||
            rule.sheet.path.rfind("res://", 0) != 0) {
            std::cerr << "[Items] Skipping invalid block atlas '" << rule.name << "'\n";
            continue;
        }
        rule.sheet.frame_width_px = static_cast<uint32_t>(size.x);
        rule.sheet.frame_height_px = static_cast<uint32_t>(size.y);

        // A range of N block types needs N consecutive frame indices.
        if (static_cast<uint64_t>(rule.first_frame) +
                (rule.last_block_type - rule.first_block_type) >
                std::numeric_limits<uint32_t>::max()) {
            std::cerr << "[Items] Atlas frame index overflow in '" << rule.name << "'\n";
            continue;
        }

        bool overlaps = false;
        for (const BlockAtlasRule& previous : rules) {
            if (rule.first_block_type <= previous.last_block_type &&
                rule.last_block_type >= previous.first_block_type) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) {
            std::cerr << "[Items] Overlapping block atlas range: '" << rule.name << "'\n";
            continue;
        }
        rules.push_back(std::move(rule));
    }
    return rules;
}

inline size_t registerStructureItems(
        ItemRegistry& registry,
        const std::unordered_map<int, Structure>& structures,
        const Json& block_atlas_document) {
    const auto block_atlases = parseBlockAtlases(block_atlas_document);
    size_t unmapped_blocks = 0;
    size_t added = 0;
    for (const auto& [id, structure] : structures) {
        const auto& st = structure.getTemplate();
        if (st.structureID <= 0 ||
            st.structureID > std::numeric_limits<uint32_t>::max() - STRUCTURE_ITEM_BASE ||
            st.x_width <= 0 || st.y_width <= 0) continue;

        ItemDefinition item;
        item.iID = STRUCTURE_ITEM_BASE + st.structureID;
        item.structure_id = st.structureID;
        // A copy of the structure dimensions is intentional: dropped items need size.
        item.world_size = {static_cast<float>(st.x_width), static_cast<float>(st.y_width)};
        item.category = st.isBuilding ? ItemCategory::Building : ItemCategory::Material;
        item.name = (st.isBuilding ? "Building " : "Block ") + std::to_string(st.structureID);
        item.description = "Automatically generated from structures.json";
        item.max_stack = st.isBuilding ? 99u : 999u;
        item.can_go_hotbar = true;
        // Structures carry their own visual source. A building gets its own
        // sprite; an ordinary block uses its material tile in terrain.png.
        if (st.isBuilding) {
            item.icon = st.sprite.empty() ? "" :
                (st.sprite.rfind("res://", 0) == 0 ? st.sprite
                 : "res://game/sprite/" + st.sprite);
            item.world_sprite = item.icon;
        } else {
            // Use the underlying block type, which need not equal structureID.
            const uint32_t block_type = st.blockType > 0
                ? static_cast<uint32_t>(st.blockType)
                : static_cast<uint32_t>(st.structureID);

            bool found_atlas = false;
            for (const BlockAtlasRule& rule : block_atlases) {
                if (block_type < rule.first_block_type ||
                    block_type > rule.last_block_type) continue;

                item.sprite_sheet = rule.sheet;
                item.sprite_sheet.icon_frame =
                    rule.first_frame + (block_type - rule.first_block_type);
                found_atlas = true;
                break;
            }
            if (!found_atlas) ++unmapped_blocks;
        }

        ItemAction a;
        a.type = st.isBuilding ? ItemActionType::PlaceStructure : ItemActionType::PlaceBlock;
        a.place_type_id = st.structureID;
        a.consume_count = 1;
        a.animation = "place";
        item.actions.push_back(std::move(a));

        if (registry.emplace(item.iID, std::move(item)).second) ++added;
        else std::cerr << "[Items] Generated ID collision for structure " << id << "\n";
    }
    if (unmapped_blocks > 0) {
        std::cerr << "[Items] No atlas mapping for " << unmapped_blocks
                  << " block items (their inventory icons will be missing)\n";
    }
    return added;
}

} // namespace ItemRegistryLoader
#endif // ITEM_REGISTRY_LOADER_H
