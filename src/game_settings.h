
#ifndef GAME_SETTINGS_H
#define GAME_SETTINGS_H

#include "json_loader.h"
#include "terrain_generator.h"
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <string>
#include <thread>
#include <utility>
#include <vector>

struct GameSettings {
    struct Performance {
        std::size_t entity_parallel_threshold = 250;
        std::size_t entity_min_batch_size = 125;
        bool profiling_enabled = true;
        int profiling_sample_count = 300;
        unsigned int thread_pool_workers = 0; // 0 = hardware concurrency
    } performance;

    struct World {
        int default_width = 640;
        int default_height = 180;
        int default_chunk_size = 64;
        float default_density_threshold = 128.0f;
    } world;

    TerrainConfig terrain;

    struct Rendering {
        float entity_culling_margin = 2.0f;
    } rendering;

    struct Simulation {
        double structure_spawn_interval = 1.0;
    } simulation;

    static const GameSettings& Instance() {
        static const GameSettings result = Load();
        return result;
    }

private:
    static GameSettings Load() {
        GameSettings s;
        const nlohmann::json& root = JsonLoader::Instance().Get("game/game_settings.json");
        if (!root.is_object()) {
            std::cerr << "[GameSettings] Missing/invalid game/game_settings.json; using defaults\n";
            return s;
        }
        auto section = [&root](const char* key) -> nlohmann::json {
            if (!root.contains(key) || !root[key].is_object()) return nlohmann::json::object();
            return root[key];
        };
        const auto p = section("performance");
        const auto w = section("world");
        const auto t = section("terrain_generation");
        const auto r = section("rendering");
        const auto sim = section("simulation");
        // JSON lookup without try/catch: compatible with -fno-exceptions.
        // Reject mismatched types and out-of-range integer values.
        auto read = [](const nlohmann::json& j, const char* key, auto fallback) {
            using T = decltype(fallback);
            auto it = j.find(key);
            if (it == j.end()) return fallback;

            if constexpr (std::is_same_v<T, bool>) {
                return it->is_boolean() ? it->get<bool>() : fallback;
            } else if constexpr (std::is_integral_v<T>) {
                if (!it->is_number_integer()) return fallback;

                if (it->is_number_unsigned()) {
                    const auto value = it->get<std::uint64_t>();
                    if (value > static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
                        return fallback;
                    return static_cast<T>(value);
                }

                const auto value = it->get<std::int64_t>();
                if constexpr (std::is_unsigned_v<T>) {
                    if (value < 0 ||
                        static_cast<std::uint64_t>(value) >
                            static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
                        return fallback;
                } else {
                    if (value < static_cast<std::int64_t>(std::numeric_limits<T>::lowest()) ||
                        value > static_cast<std::int64_t>(std::numeric_limits<T>::max()))
                        return fallback;
                }
                return static_cast<T>(value);
            } else if constexpr (std::is_floating_point_v<T>) {
                return it->is_number() ? it->get<T>() : fallback;
            }
            return fallback;
        };
        s.performance.entity_parallel_threshold = std::max<std::size_t>(1, read(p, "entity_parallel_threshold", s.performance.entity_parallel_threshold));
        s.performance.entity_min_batch_size = std::max<std::size_t>(1, read(p, "entity_min_batch_size", s.performance.entity_min_batch_size));
        s.performance.profiling_enabled = read(p, "profiling_enabled", s.performance.profiling_enabled);
        s.performance.profiling_sample_count = std::max(1, read(p, "profiling_sample_count", s.performance.profiling_sample_count));
        s.performance.thread_pool_workers = read(p, "thread_pool_workers", s.performance.thread_pool_workers);
        s.world.default_width = std::max(1, read(w, "default_width", s.world.default_width));
        s.world.default_height = std::max(1, read(w, "default_height", s.world.default_height));
        s.world.default_chunk_size = std::max(1, read(w, "default_chunk_size", s.world.default_chunk_size));
        s.world.default_density_threshold = std::clamp(read(w, "default_density_threshold", s.world.default_density_threshold), 0.0f, 255.0f);
        // Terrain schema: nested sections and reusable biome palettes.
        auto child = [](const nlohmann::json& obj, const char* key) -> nlohmann::json {
            if (!obj.is_object()) return nlohmann::json::object();
            auto it = obj.find(key);
            return (it != obj.end() && it->is_object()) ? *it : nlohmann::json::object();
        };
        auto bounded = [&](const nlohmann::json& obj, const char* key,
                           float fallback, float lo, float hi) -> float {
            const float v = read(obj, key, fallback);
            return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
        };
        auto palette = [&](const nlohmann::json& obj, const char* key,
                           TerrainConfig::Palette& target) {
            if (!obj.is_object()) return;
            auto it = obj.find(key);
            if (it == obj.end() || !it->is_array() || it->size() != 2) return;
            if (!(*it)[0].is_number_integer() || !(*it)[1].is_number_integer()) return;
            // Only IDs 1..256 are currently supplied with atlas tiles.
            auto asId = [](const nlohmann::json& v) -> int {
                if (v.is_number_unsigned())
                    return static_cast<int>(std::min<std::uint64_t>(
                        256, v.get<std::uint64_t>()));
                return static_cast<int>(std::clamp<std::int64_t>(
                    v.get<std::int64_t>(), 1, 256));
            };
            const int a = asId((*it)[0]);
            const int b = std::max(a, asId((*it)[1]));
            target.first = static_cast<uint16_t>(a);
            target.last = static_cast<uint16_t>(b);
        };
        s.terrain.seed = read(t, "seed", s.terrain.seed);
        s.terrain.material_patch_cells = std::clamp(
            read(t,"material_patch_cells",s.terrain.material_patch_cells),1,256);
        s.terrain.underground_start_depth = std::max(1,
            read(t,"underground_start_depth",s.terrain.underground_start_depth));
        s.terrain.underground_biome_scale = bounded(t,"underground_biome_scale",
            s.terrain.underground_biome_scale,0.00001f,1.0f);

        const auto ts = child(t,"surface");
        auto& surface = s.terrain.surface;
        surface.base_height_fraction = bounded(ts,"base_height_fraction",surface.base_height_fraction,0.05f,0.95f);
        surface.noise_scale = bounded(ts,"noise_scale",surface.noise_scale,0.00001f,1.0f);
        surface.octaves = std::clamp(read(ts,"octaves",surface.octaves),1,8);
        surface.persistence = bounded(ts,"persistence",surface.persistence,0.0f,1.0f);
        surface.height_variation_fraction = bounded(ts,"height_variation_fraction",surface.height_variation_fraction,0.0f,0.5f);
        surface.biome_scale = bounded(ts,"biome_scale",surface.biome_scale,0.00001f,1.0f);
        surface.mountain_region_scale = bounded(ts,"mountain_region_scale",surface.mountain_region_scale,0.00001f,1.0f);
        surface.mountain_detail_scale = bounded(ts,"mountain_detail_scale",surface.mountain_detail_scale,0.00001f,1.0f);
        surface.mountain_amplitude_fraction = bounded(ts,"mountain_amplitude_fraction",surface.mountain_amplitude_fraction,0.0f,0.5f);
        surface.mountain_activation = bounded(ts,"mountain_activation",surface.mountain_activation,0.0f,0.95f);
        surface.surface_density_multiplier = bounded(ts,"surface_density_multiplier",surface.surface_density_multiplier,0.0f,255.0f);
        surface.surface_density_offset = bounded(ts,"surface_density_offset",surface.surface_density_offset,0.0f,255.0f);

        const auto tc = child(t,"caves");
        auto& caves=s.terrain.caves;
        caves.enabled = read(tc,"enabled",caves.enabled);
        caves.noise_scale = bounded(tc,"noise_scale",caves.noise_scale,0.00001f,1.0f);
        caves.octaves = std::clamp(read(tc,"octaves",caves.octaves),1,8);
        caves.threshold = bounded(tc,"threshold",caves.threshold,0.01f,0.99f);
        caves.edge_softness = bounded(tc,"edge_softness",caves.edge_softness,0.0f,0.5f);
        caves.minimum_depth = std::max(0,read(tc,"minimum_depth",caves.minimum_depth));
        caves.full_depth = std::max(caves.minimum_depth+1,read(tc,"full_depth",caves.full_depth));
        caves.large_cave_scale = bounded(tc,"large_cave_scale",caves.large_cave_scale,0.00001f,1.0f);
        caves.large_cave_threshold = bounded(tc,"large_cave_threshold",caves.large_cave_threshold,0.1f,0.99f);
        caves.surface_entrances_per_1000_width = std::clamp(read(tc,"surface_entrances_per_1000_width",caves.surface_entrances_per_1000_width),0,100);
        caves.tunnel_min_length = std::clamp(read(tc,"tunnel_min_length",caves.tunnel_min_length),1,1000);
        caves.tunnel_max_length = std::clamp(read(tc,"tunnel_max_length",caves.tunnel_max_length),caves.tunnel_min_length,1000);
        caves.tunnel_radius = bounded(tc,"tunnel_radius",caves.tunnel_radius,0.25f,25.0f);

        const auto ti = child(t,"islands");
        auto& islands=s.terrain.islands;
        islands.enabled=read(ti,"enabled",islands.enabled);
        islands.islands_per_1000_width=bounded(ti,"islands_per_1000_width",islands.islands_per_1000_width,0.0f,100.0f);
        islands.min_y_fraction=bounded(ti,"min_y_fraction",islands.min_y_fraction,0.0f,0.90f);
        islands.max_y_fraction=bounded(ti,"max_y_fraction",islands.max_y_fraction,islands.min_y_fraction,0.99f);
        islands.min_radius=std::clamp(read(ti,"min_radius",islands.min_radius),2,1000);
        islands.max_radius=std::clamp(read(ti,"max_radius",islands.max_radius),islands.min_radius,1000);
        islands.thickness=bounded(ti,"thickness",islands.thickness,0.05f,2.0f);

        const auto tp = child(t,"patches");
        auto& patches=s.terrain.patches;
        patches.enabled=read(tp,"enabled",patches.enabled);
        patches.attempts_per_chunk=bounded(tp,"attempts_per_chunk",patches.attempts_per_chunk,0.0f,32.0f);
        patches.probability=bounded(tp,"probability",patches.probability,0.0f,1.0f);
        patches.min_radius=std::clamp(read(tp,"min_radius",patches.min_radius),1,128);
        patches.max_radius=std::clamp(read(tp,"max_radius",patches.max_radius),patches.min_radius,128);
        palette(tp,"material_ids",patches.material);

        // Replacing the old grass_depth/surface_material_id/deep_material_id
        // with per-biome palettes. JSON arrays overwrite the defaults if valid.
        if (auto it=t.find("surface_biomes"); it!=t.end() && it->is_array()) {
            std::vector<TerrainConfig::SurfaceBiome> biomes;
            for (const auto& item:*it) {
                if (!item.is_object() || biomes.size()>=32) continue;
                TerrainConfig::SurfaceBiome b;
                if (auto name=item.find("name"); name!=item.end() && name->is_string())
                    b.name=name->get<std::string>();
                b.height_offset=bounded(item,"height_offset",b.height_offset,-0.2f,0.2f);
                b.hill_multiplier=bounded(item,"hill_multiplier",b.hill_multiplier,0.0f,3.0f);
                b.soil_depth=std::clamp(read(item,"soil_depth",b.soil_depth),1,64);
                palette(item,"top_ids",b.top);
                palette(item,"soil_ids",b.soil);
                palette(item,"rock_ids",b.rock);
                biomes.push_back(std::move(b));
            }
            if (!biomes.empty()) s.terrain.surface_biomes=std::move(biomes);
        }
        if (auto it=t.find("underground_biomes"); it!=t.end() && it->is_array()) {
            std::vector<TerrainConfig::UndergroundBiome> biomes;
            for (const auto& item:*it) {
                if (!item.is_object() || biomes.size()>=32) continue;
                TerrainConfig::UndergroundBiome b;
                if (auto name=item.find("name"); name!=item.end() && name->is_string())
                    b.name=name->get<std::string>();
                palette(item,"material_ids",b.material);
                biomes.push_back(std::move(b));
            }
            if (!biomes.empty()) s.terrain.underground_biomes=std::move(biomes);
        }
        if (auto it=t.find("structure_rules"); it!=t.end() && it->is_array()) {
            for (const auto& item:*it) {
                if (!item.is_object() || s.terrain.structure_rules.size()>=32) continue;
                TerrainConfig::StructureRule rule;
                rule.structure_id=read(item,"structure_id",rule.structure_id);
                rule.chance_per_chunk=bounded(item,"chance_per_chunk",rule.chance_per_chunk,0.0f,1.0f);
                rule.required_clearance=std::clamp(read(item,"required_clearance",rule.required_clearance),1,64);
                rule.footprint_width=std::clamp(read(item,"footprint_width",rule.footprint_width),1,64);
                rule.footprint_height=std::clamp(read(item,"footprint_height",rule.footprint_height),1,64);
                if (auto bi=item.find("surface_biome");bi!=item.end() && bi->is_string())
                    rule.surface_biome=bi->get<std::string>();
                if (rule.structure_id != 0) s.terrain.structure_rules.push_back(std::move(rule));
            }
        }
        s.rendering.entity_culling_margin = std::max(0.0f, read(r, "entity_culling_margin", s.rendering.entity_culling_margin));
        s.simulation.structure_spawn_interval = std::max(0.0, read(sim, "structure_spawn_interval", s.simulation.structure_spawn_interval));
        return s;
    }
};

#endif
