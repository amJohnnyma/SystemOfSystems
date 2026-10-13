/*
 Startup Preloading vs. Lazy Loading in Loop
 // Explicitly preload at startup if desired
JsonLoader::Instance().LoadFile("materials.json");

// In Marching Squares loop:
for (int i = 0; i < world_size; ++i) {
    // "assets.json" will automatically load ONCE on the first iteration, then use memory cache
    std::string tex = JsonLoader::Instance().GetEntry<std::string>("assets.json", "textures", 0, "path");
}
. Direct Indexing & Property Fetching
// Option A: Direct nlohmann::json lookup via Get()
auto& materials_json = JsonLoader::Instance().Get("materials.json");
if (materials_json["materials"][0]["id"] == 2) {
    // Do something
}

// Option B: Variadic type-safe helper via GetEntry<T>()
uint8_t priority = JsonLoader::Instance().GetEntry<uint8_t>("materials.json", "materials", 1, "priority");
 */
#ifndef JSON_LOADER_H
#define JSON_LOADER_H

#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include "json.hpp"
#include "attack_type.h"


struct jResourceRequirement {
    int objectID{0};
    int count{0};

    // Enables direct conversion from nlohmann::json to ResourceRequirement
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(jResourceRequirement, objectID, count)
};

struct jStructure {
    int structureID{0};
    int x_width{0};
    int y_width{0};
    bool cantick{false};
    bool isBuilding{false};
    float processing_time{0.0f};
    int blockType{0};
    std::string sprite{""};
    std::vector<jResourceRequirement> resource_input;
    std::vector<jResourceRequirement> resource_output;

};

inline void from_json(const nlohmann::json& j, jStructure& s) {
    j.at("structureID").get_to(s.structureID);
    j.at("x_width").get_to(s.x_width);
    j.at("y_width").get_to(s.y_width);
    s.cantick = j.value("cantick", false);
    s.isBuilding = j.value("isBuilding", false);
    s.processing_time = j.value("processing_time", 1.0f);
    s.blockType = j.value("blockType", 0);
    s.sprite = j.value("sprite", "");
    s.resource_input = j.value("resource_input", std::vector<jResourceRequirement>{});
    s.resource_output = j.value("resource_output", std::vector<jResourceRequirement>{});

}
// Enum string conversion helper for attack_type
inline void from_json(const nlohmann::json& j, attack_type& at) {
    std::string s = j.get<std::string>();
    if (s == "DIRECT") at = DIRECT;
    else if (s == "ARC") at = ARC;
    else if (s == "LINE") at = LINE;
    else if (s == "AEO") at = AEO;
    else if (s == "HEAL") at = HEAL;
    else at = DIRECT;
}

struct jAttack {
    std::string name{""};
    float damage{0.0f};
    float range{0.0f};
    attack_type at{DIRECT};
    float cooldownBeforeNextAttack{0.0f};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(jAttack, name, damage, range, at, cooldownBeforeNextAttack)
};

struct jAttackReference {
    std::string name{""};
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(jAttackReference, name)
};

struct jAttackPattern {
    std::string name{""};
    std::vector<jAttackReference> attacks;
};

inline void from_json(const nlohmann::json& j, jAttackPattern& ap) {
    ap.name = j.value("name", "");
    ap.attacks = j.value("attacks", std::vector<jAttackReference>{});
}

struct jEntityAttackPatternRef {
    std::string name{""};
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(jEntityAttackPatternRef, name)
};

struct jEntityTemplate {
    uint32_t entityTypeID{0};
    std::string name{""};
    std::string sprite{""};
    std::string entity_class{"enemy"};
    std::string behavior_type{"idle"};
    float max_health{100.0f};
    float move_speed{5.0f};
    int width{2};
    int height{2};
    bool is_pickupable{false};
    std::vector<jEntityAttackPatternRef> attack_patterns;
};

inline void from_json(const nlohmann::json& j, jEntityTemplate& et) {
    et.entityTypeID = j.value("entityTypeID", 0);
    et.name = j.value("name", "");
    et.sprite = j.value("sprite", "");
    et.entity_class = j.value("entity_class", "enemy");
    et.behavior_type = j.value("behavior_type", "idle");
    et.max_health = j.value("max_health", 100.0f);
    et.move_speed = j.value("move_speed", 5.0f);
    et.is_pickupable = j.value("is_pickupable", false);
    et.width= j.value("width",2);
    et.height= j.value("height",2);
    et.attack_patterns = j.value("attack_patterns", std::vector<jEntityAttackPatternRef>{});
}

class JsonLoader {
public:
    static JsonLoader& Instance() {
        static JsonLoader instance;
        return instance;
    }

    // Load file safely without throwing exceptions
    bool LoadFile(const std::string& filepath) {
        if (cache_.find(filepath) != cache_.end()) {
            return true; // Already loaded
        }

        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[JsonLoader] Error: Could not open file " << filepath << std::endl;
            return false;
        }

        // Pass 'false' as the 3rd argument to disable exceptions in nlohmann::json
        nlohmann::json parsed_json = nlohmann::json::parse(file, nullptr, false);
        
        if (parsed_json.is_discarded()) {
            std::cerr << "[JsonLoader] Parse error in " << filepath << std::endl;
            return false;
        }

        cache_[filepath] = std::move(parsed_json);
        return true;
    }

    nlohmann::json* GetPtr(const std::string& filepath) {
        auto it = cache_.find(filepath);
        if (it == cache_.end()) {
            if (!LoadFile(filepath)) return nullptr;
            return &cache_[filepath];
        }
        return &(it->second);
    }

    nlohmann::json& Get(const std::string& filepath) {
        nlohmann::json* ptr = GetPtr(filepath);
        static nlohmann::json null_json = nullptr;
        if (!ptr) return null_json;
        return *ptr;
    }


    template <typename T, typename... Args>
    T GetEntry(const std::string& filepath, Args&&... keys_or_indices) {
        nlohmann::json* current = GetPtr(filepath);
        if (!current || current->is_null()) return T{};

        bool success = AccessJsonPathPtr(current, std::forward<Args>(keys_or_indices)...);
        if (success && current && !current->is_null())
        {
            return current->template get<T>();
        }
        
        return T{};
    }

    bool IsLoaded(const std::string& filepath) const {
        return cache_.find(filepath) != cache_.end();
    }

    void Clear() {
        cache_.clear();
    }

private:
    JsonLoader() = default;
    ~JsonLoader() = default;
    JsonLoader(const JsonLoader&) = delete;
    JsonLoader& operator=(const JsonLoader&) = delete;

    std::unordered_map<std::string, nlohmann::json> cache_;

    // Safe pointer-based path traversal (no exceptions thrown)
    template <typename CurrentKey>
    bool AccessJsonPathPtr(nlohmann::json*& current, CurrentKey&& key) {
        if (!current) return false;
        
        if (current->is_object() && current->contains(key))
        {
            current = &((*current)[std::forward<CurrentKey>(key)]);
            return true;
        }

        if (current->is_array())
        {
            if constexpr (std::is_integral_v<std::decay_t<CurrentKey>>) {
                size_t idx = static_cast<size_t>(key);
                if (idx < current->size())
                {
                    current = &((*current)[idx]);
                    return true;
                }
            }
        }
        return false;
    }

    template <typename CurrentKey, typename... RemainingKeys>
    bool AccessJsonPathPtr(nlohmann::json*& current, CurrentKey&& key, RemainingKeys&&... rest) {
        if (!AccessJsonPathPtr(current, std::forward<CurrentKey>(key))) return false;
        return AccessJsonPathPtr(current, std::forward<RemainingKeys>(rest)...);
    }
};

#endif // JSON_LOADER_H
