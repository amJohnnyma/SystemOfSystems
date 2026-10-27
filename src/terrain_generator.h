#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include "world_types.h"
#include <cstdint>
#include <string>
#include <vector>

class ThreadPool;

// No Godot or JSON dependency. One independent generator per world/settings.
// Coordinates are CELL coordinates, +Y down, matching WorldGrid and marching squares.
struct TerrainConfig {
    struct Palette {
        uint16_t first = 1;
        uint16_t last = 256;
    };
    struct SurfaceBiome {
        std::string name = "plains";
        float height_offset = 0.0f; // fraction of world height, +Y down
        float hill_multiplier = 1.0f;
        int soil_depth = 7;
        Palette top{1, 32};
        Palette soil{33, 96};
        Palette rock{97, 160};
    };
    struct UndergroundBiome {
        std::string name = "deep";
        Palette material{161, 224};
    };
    struct Surface {
        float base_height_fraction = 0.38f;
        float noise_scale = 0.0065f;
        int octaves = 4;
        float persistence = 0.52f;
        float height_variation_fraction = 0.10f;
        float biome_scale = 0.0014f;
        float mountain_region_scale = 0.0018f;
        float mountain_detail_scale = 0.017f;
        float mountain_amplitude_fraction = 0.20f;
        float mountain_activation = 0.53f;
        float surface_density_multiplier = 128.0f;
        float surface_density_offset = 64.0f;
    } surface;
    struct Caves {
        bool enabled = true;
        float noise_scale = 0.031f;
        int octaves = 3;
        float threshold = 0.62f;
        float edge_softness = 0.09f;
        int minimum_depth = 8;
        int full_depth = 34;
        float large_cave_scale = 0.008f;
        float large_cave_threshold = 0.78f;
        int surface_entrances_per_1000_width = 6;
        int tunnel_min_length = 14;
        int tunnel_max_length = 42;
        float tunnel_radius = 2.5f;
    } caves;
    struct Islands {
        bool enabled = true;
        float islands_per_1000_width = 5.0f;
        float min_y_fraction = 0.06f;
        float max_y_fraction = 0.25f;
        int min_radius = 9;
        int max_radius = 28;
        float thickness = 0.42f;
    } islands;
    struct Patches {
        bool enabled = true;
        float attempts_per_chunk = 2.0f;
        float probability = 0.76f;
        int min_radius = 3;
        int max_radius = 10;
        Palette material{1, 256};
    } patches;
    struct StructureRule {
        uint32_t structure_id = 0;
        float chance_per_chunk = 0.0f;
        std::string surface_biome = ""; // empty = any
        int required_clearance = 6;
        int footprint_width = 1;
        int footprint_height = 1;
    };

    uint32_t seed = 1337; // 0 = random seed per generation
    int underground_start_depth = 27;
    float underground_biome_scale = 0.006f;
    int material_patch_cells = 6; // stable random variation within palette
    std::vector<SurfaceBiome> surface_biomes = {
        {"plains", 0.00f, 0.65f, 8, {1, 16}, {17, 48}, {49, 96}},
        {"badlands", 0.035f, 1.00f, 4, {97, 112}, {113, 144}, {145, 176}},
        {"highlands", -0.035f, 1.40f, 5, {177, 192}, {193, 216}, {217, 240}},
        {"strange", 0.00f, 0.85f, 11, {241, 256}, {1, 64}, {65, 160}}
    };
    std::vector<UndergroundBiome> underground_biomes = {
        {"stone", {49, 112}},
        {"crystal", {113, 192}},
        {"ancient", {193, 256}}
    };
    std::vector<StructureRule> structure_rules; // only proposes, never places
};

struct GeneratedStructure {
    uint32_t structure_id = 0;
    int x = 0;
    int y = 0; // bottom occupied cell origin, above ground
    int footprint_width = 1;
    int footprint_height = 1;
};

struct TerrainGenerationResult {
    uint32_t seed_used = 0;
    std::vector<int> surface_y; // terrain surface BEFORE caves/islands
    std::vector<uint8_t> surface_biome_ids; // index into TerrainConfig::surface_biomes
    std::vector<GeneratedStructure> proposed_structures;
};

class TerrainGenerator final {
public:
    explicit TerrainGenerator(TerrainConfig config);

    // Overwrites only material fields in terrainCells (not entity/structure ownership).
    // Caller provides cells sized width*height and owns all lifetime/thread synchronization.
    TerrainGenerationResult generate(std::vector<Cell>& terrainCells,
                                     int width, int height, int chunkSize,
                                     ThreadPool* pool = nullptr) const;

private:
    TerrainConfig config_;
};

#endif
