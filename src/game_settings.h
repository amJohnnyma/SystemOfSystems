
#ifndef GAME_SETTINGS_H
#define GAME_SETTINGS_H

#include "json_loader.h"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>
#include <thread>

struct GameSettings {
    struct Performance {
        std::size_t entity_parallel_threshold = 250;
        std::size_t entity_min_batch_size = 125;
        bool profiling_enabled = true;
        int profiling_sample_count = 300;
        unsigned int thread_pool_workers = 0; // 0 = hardware concurrency
    } performance;

    struct World {
        int default_width = 6400;
        int default_height = 1800;
        int default_chunk_size = 64;
        float default_density_threshold = 128.0f;
    } world;

    struct Terrain {
        float surface_noise_frequency = 0.008f;
        int surface_noise_octaves = 4;
        float surface_noise_amplitude = 1.0f;
        float surface_noise_persistence = 0.5f;
        float base_surface_height_fraction = 0.25f;
        float height_variation_fraction = 0.1f;
        int grass_depth = 8;
        int surface_material_id = 2;
        int deep_material_id = 3;
        float patch_probability = 0.8f;
        int patch_material_id_min = 1;
        int patch_material_id_max = 256;
        float patch_min_world_y_fraction = 0.3f;
        float patch_radius_min = 3.0f;
        float patch_radius_max = 10.0f;
        float patch_density_strength = 255.0f;
    } terrain;

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
        auto read = [](const nlohmann::json& j, const char* key, auto fallback) {
            using T = decltype(fallback);
            if (!j.contains(key)) return fallback;
            try { return j.at(key).get<T>(); }
            catch (const nlohmann::json::exception&) { return fallback; }
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
        s.terrain.surface_noise_frequency = std::max(0.0f, read(t, "surface_noise_frequency", s.terrain.surface_noise_frequency));
        s.terrain.surface_noise_octaves = std::max(1, read(t, "surface_noise_octaves", s.terrain.surface_noise_octaves));
        s.terrain.surface_noise_amplitude = read(t, "surface_noise_amplitude", s.terrain.surface_noise_amplitude);
        s.terrain.surface_noise_persistence = read(t, "surface_noise_persistence", s.terrain.surface_noise_persistence);
        s.terrain.base_surface_height_fraction = read(t, "base_surface_height_fraction", s.terrain.base_surface_height_fraction);
        s.terrain.height_variation_fraction = read(t, "height_variation_fraction", s.terrain.height_variation_fraction);
        s.terrain.grass_depth = std::max(0, read(t, "grass_depth", s.terrain.grass_depth));
        s.terrain.surface_material_id = std::clamp(read(t, "surface_material_id", s.terrain.surface_material_id), 0, 65535);
        s.terrain.deep_material_id = std::clamp(read(t, "deep_material_id", s.terrain.deep_material_id), 0, 65535);
        s.terrain.patch_probability = std::clamp(read(t, "patch_probability", s.terrain.patch_probability), 0.0f, 1.0f);
        s.terrain.patch_material_id_min = std::clamp(read(t, "patch_material_id_min", s.terrain.patch_material_id_min), 0, 65535);
        s.terrain.patch_material_id_max = std::clamp(read(t, "patch_material_id_max", s.terrain.patch_material_id_max), s.terrain.patch_material_id_min, 65535);
        s.terrain.patch_min_world_y_fraction = std::clamp(read(t, "patch_min_world_y_fraction", s.terrain.patch_min_world_y_fraction), 0.0f, 1.0f);
        s.terrain.patch_radius_min = std::max(0.001f, read(t, "patch_radius_min", s.terrain.patch_radius_min));
        s.terrain.patch_radius_max = std::max(s.terrain.patch_radius_min, read(t, "patch_radius_max", s.terrain.patch_radius_max));
        s.terrain.patch_density_strength = read(t, "patch_density_strength", s.terrain.patch_density_strength);
        s.rendering.entity_culling_margin = std::max(0.0f, read(r, "entity_culling_margin", s.rendering.entity_culling_margin));
        s.simulation.structure_spawn_interval = std::max(0.0, read(sim, "structure_spawn_interval", s.simulation.structure_spawn_interval));
        return s;
    }
};

#endif
