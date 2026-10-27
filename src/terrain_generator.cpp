
#include "terrain_generator.h"
#include "thread_pool.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <future>
#include <iostream>
#include <random>
#include <utility>

namespace {
constexpr float PI = 3.14159265358979323846f;

uint32_t hash32(uint32_t n) {
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    n *= 0x846ca68bu;
    return n ^ (n >> 16);
}
uint32_t hash2(int x, int y, uint32_t seed) {
    return hash32(seed ^ hash32(static_cast<uint32_t>(x) * 0x9e3779b9u)
                          ^ hash32(static_cast<uint32_t>(y) * 0x85ebca6bu));
}
float lerp(float a, float b, float t) { return a + (b-a)*t; }
float smooth(float t) { return t * t * (3.0f - 2.0f*t); }
float smoothstep(float a, float b, float v) {
    if (b <= a) return v >= b ? 1.0f : 0.0f;
    return smooth(std::clamp((v-a)/(b-a), 0.0f, 1.0f));
}
// Stateless value noise: safe for concurrent columns and independent of task order.
float noise(float x, float y, uint32_t seed) {
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const float tx = smooth(x - static_cast<float>(ix));
    const float ty = smooth(y - static_cast<float>(iy));
    auto corner = [&](int ox, int oy) -> float {
        return (hash2(ix+ox, iy+oy, seed) & 0x00ffffffu) /
               8388607.5f - 1.0f;
    };
    return lerp(lerp(corner(0,0), corner(1,0), tx),
                lerp(corner(0,1), corner(1,1), tx), ty);
}
float fbm(float x, float y, int octaves, float persistence, uint32_t seed) {
    float value = 0.0f, amplitude = 1.0f, total = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        value += noise(x, y, seed + static_cast<uint32_t>(o) * 7919u) * amplitude;
        total += amplitude;
        amplitude *= persistence;
        x *= 2.0f;
        y *= 2.0f;
    }
    return total > 0.0f ? value / total : 0.0f;
}
int biomeIndex(float n, std::size_t count) {
    if (count == 0) return 0;
    const float normalized = std::clamp((n+1.0f)*0.5f, 0.0f, 0.999999f);
    return std::min(static_cast<int>(count)-1, static_cast<int>(normalized * count));
}
uint16_t paletteId(const TerrainConfig::Palette& p, int x, int y,
                   int patchCells, uint32_t seed) {
    const uint32_t lo = std::max<uint32_t>(1, p.first);
    const uint32_t hi = std::max<uint32_t>(lo, p.last);
    const int patch = std::max(1, patchCells);
    return static_cast<uint16_t>(lo + hash2(x/patch, y/patch, seed) % (hi-lo+1));
}
std::size_t at(int x, int y, int width) {
    return static_cast<std::size_t>(y)*static_cast<std::size_t>(width)
         + static_cast<std::size_t>(x);
}
void setMaterial(Cell& c, uint16_t id, uint8_t density) {
    c.type_id = density ? id : 0;
    c.density = density;
    c.state_flags = 0;
}
void carveDisc(std::vector<Cell>& cells, int width, int height,
               float x, float y, float radius) {
    const int minX = std::max(0, static_cast<int>(std::floor(x-radius)));
    const int maxX = std::min(width-1, static_cast<int>(std::ceil(x+radius)));
    const int minY = std::max(0, static_cast<int>(std::floor(y-radius)));
    const int maxY = std::min(height-1, static_cast<int>(std::ceil(y+radius)));
    for (int yy=minY; yy<=maxY; ++yy)
        for (int xx=minX; xx<=maxX; ++xx) {
            const float dx = xx-x, dy = yy-y;
            if (dx*dx+dy*dy <= radius*radius)
                setMaterial(cells[at(xx,yy,width)], 0, 0);
        }
}

template<class Fn>
void forColumns(int width, ThreadPool* pool, Fn&& process) {
    if (!pool || width < 128 || pool->get_num_threads() <= 1) {
        process(0, width);
        return;
    }
    const int taskCount = std::min(static_cast<int>(pool->get_num_threads()),
                                   std::max(1, (width + 127)/128));
    const int stride = (width + taskCount-1)/taskCount;
    std::vector<std::future<void>> futures;
    futures.reserve(taskCount);
    for (int t=0; t<taskCount; ++t) {
        const int begin = t*stride;
        const int end = std::min(width, begin+stride);
        if (begin >= end) break;
        futures.emplace_back(pool->enqueue([&,begin,end]{ process(begin,end); }));
    }
    for (auto& future : futures) future.get();
}
} // namespace

TerrainGenerator::TerrainGenerator(TerrainConfig config)
    : config_(std::move(config)) {}

TerrainGenerationResult TerrainGenerator::generate(std::vector<Cell>& cells,
                                                  int width, int height,
                                                  int chunkSize,
                                                  ThreadPool* pool) const {
    TerrainGenerationResult result;
    if (width <= 0 || height <= 0 || chunkSize <= 0 ||
        cells.size() != static_cast<std::size_t>(width)*height) {
        std::cerr << "[TerrainGenerator] Invalid world dimensions or cell vector.\n";
        return result;
    }
    TerrainConfig cfg = config_;
    if (cfg.surface_biomes.empty()) cfg.surface_biomes.emplace_back();
    if (cfg.underground_biomes.empty()) cfg.underground_biomes.emplace_back();
    uint32_t seed = cfg.seed;
    if (seed == 0) {
        std::random_device rd;
        seed = rd();
        if (seed == 0) seed = 1;
    }
    result.seed_used = seed;
    //std::cout << "[TerrainGenerator] Seed: " << seed << "\n";
    result.surface_y.resize(width);
    result.surface_biome_ids.resize(width, 0);
    std::vector<float> surface_fraction(width, 0.0f);

    // Stage 1: climate + heightmap (smooth height despite discrete material biomes).
    forColumns(width, pool, [&](int begin, int end) {
        for (int x=begin; x<end; ++x) {
            const float xf = static_cast<float>(x);
            const float climate = noise(xf*cfg.surface.biome_scale, 17.3f, seed+101u);
            const float biomeCoord = std::clamp((climate+1.0f)*0.5f,
                0.0f,0.999999f)*cfg.surface_biomes.size();
            const int bi = biomeIndex(climate, cfg.surface_biomes.size());
            result.surface_biome_ids[x] = static_cast<uint8_t>(bi);
            const auto& biome = cfg.surface_biomes[bi];
            const auto& nextBiome = cfg.surface_biomes[
                std::min(bi+1,static_cast<int>(cfg.surface_biomes.size())-1)];
            // Heights blend between climate regions; material ID changes remain discrete.
            const float transition = smoothstep(0.6f,1.0f,biomeCoord-bi);
            const float biomeHeightOffset = lerp(biome.height_offset,
                                                 nextBiome.height_offset,transition);
            const float biomeHillMultiplier = lerp(biome.hill_multiplier,
                                                   nextBiome.hill_multiplier,transition);
            const float hills = fbm(xf*cfg.surface.noise_scale, 0.0f,
                                    cfg.surface.octaves, cfg.surface.persistence, seed+202u);
            const float mountainRegion = (noise(xf*cfg.surface.mountain_region_scale,
                                               55.0f, seed+303u)+1.0f)*0.5f;
            const float mountainMask = smoothstep(cfg.surface.mountain_activation,
                                                  std::min(1.0f, cfg.surface.mountain_activation+0.22f),
                                                  mountainRegion);
            const float ridgeNoise = fbm(xf*cfg.surface.mountain_detail_scale, 9.4f,
                                         3, 0.55f, seed+404u);
            const float ridges = std::pow(std::max(0.0f, 1.0f-std::abs(ridgeNoise)), 2.0f);
            const float y = height * (cfg.surface.base_height_fraction + biomeHeightOffset)
                          + hills * height * cfg.surface.height_variation_fraction * biomeHillMultiplier
                          - mountainMask*ridges*height*cfg.surface.mountain_amplitude_fraction;
            const float clampedY=std::clamp(y,2.0f,static_cast<float>(height-1));
            result.surface_y[x] = static_cast<int>(std::floor(clampedY));
            surface_fraction[x] = clampedY-std::floor(clampedY);
        }
    });

    // Stage 2: fill world (no allocation per cell and no shared mutable PRNG).
    forColumns(width, pool, [&](int begin, int end) {
        for (int x=begin; x<end; ++x) {
            const int surfaceY = result.surface_y[x];
            const auto& biome = cfg.surface_biomes[result.surface_biome_ids[x]];
            for (int y=0; y<height; ++y) {
                Cell& cell = cells[at(x,y,width)];
                if (y < surfaceY) { setMaterial(cell, 0, 0); continue; }
                const int depth = y-surfaceY;
                const TerrainConfig::Palette* palette = &biome.rock;
                if (depth == 0) palette = &biome.top;
                else if (depth < biome.soil_depth) palette = &biome.soil;
                else if (depth >= cfg.underground_start_depth) {
                    const float v = noise(x*cfg.underground_biome_scale,
                                          y*cfg.underground_biome_scale, seed+505u);
                    const int ui = biomeIndex(v, cfg.underground_biomes.size());
                    palette = &cfg.underground_biomes[ui].material;
                }
                const uint16_t id = paletteId(*palette, x,y, cfg.material_patch_cells, seed+606u);
                uint8_t density = 255;
                if (depth == 0) {
                    // Deterministic partial surface cell: keeps marching-squares slopes.
                    const float frac = surface_fraction[x];
                    density = static_cast<uint8_t>(std::clamp(
                        (1.0f-frac)*cfg.surface.surface_density_multiplier
                        +cfg.surface.surface_density_offset, 1.0f, 254.0f));
                }
                setMaterial(cell, id, density);
            }
        }
    });

    // Stage 3: underground cave field (full air cores, partial-density walls).
    if (cfg.caves.enabled) {
        forColumns(width, pool, [&](int begin,int end) {
            for (int x=begin; x<end; ++x) {
                const int start = std::max(0, result.surface_y[x]+cfg.caves.minimum_depth);
                for (int y=start; y<height; ++y) {
                    Cell& cell = cells[at(x,y,width)];
                    if (cell.type_id == 0) continue;
                    const float px = x*cfg.caves.noise_scale;
                    const float py = y*cfg.caves.noise_scale;
                    const float primary = 0.5f*(1.0f+fbm(px,py,cfg.caves.octaves,0.55f,seed+707u));
                    const float large = 0.5f*(1.0f+noise(
                        x*cfg.caves.large_cave_scale,y*cfg.caves.large_cave_scale,seed+808u));
                    const float depthFactor = smoothstep(cfg.caves.minimum_depth,
                        cfg.caves.full_depth,static_cast<float>(y-result.surface_y[x]));
                    const float limit = cfg.caves.threshold + (1.0f-depthFactor)*0.14f;
                    const float cavity = std::max(primary,
                        large-(cfg.caves.large_cave_threshold-limit));
                    if (cavity >= limit) { setMaterial(cell,0,0); }
                    else if (cfg.caves.edge_softness > 0.0f &&
                             cavity > limit-cfg.caves.edge_softness) {
                        const float fraction = (limit-cavity)/cfg.caves.edge_softness;
                        cell.density = std::min(cell.density,
                            static_cast<uint8_t>(std::clamp(fraction*255.0f,1.0f,254.0f)));
                    }
                }
            }
        });

        // Stage 4: surface-cave entrances. Short descending worm tunnels.
        const int entrances = static_cast<int>(std::round(
            width*cfg.caves.surface_entrances_per_1000_width/1000.0f));
        std::mt19937 rng(hash32(seed+909u));
        std::uniform_int_distribution<int> spawnX(4,std::max(4,width-5));
        std::uniform_int_distribution<int> lengthDist(cfg.caves.tunnel_min_length,
                                                       cfg.caves.tunnel_max_length);
        std::uniform_real_distribution<float> jitter(-1.0f,1.0f);
        for (int e=0;e<entrances && width>8;++e) {
            float cx = static_cast<float>(spawnX(rng));
            float cy = static_cast<float>(result.surface_y[static_cast<int>(cx)]-1);
            float vx = jitter(rng)*0.5f;
            for (int step=0, len=lengthDist(rng);step<len;++step) {
                carveDisc(cells,width,height,cx,cy,cfg.caves.tunnel_radius);
                vx = std::clamp(vx*0.8f+jitter(rng)*0.22f,-0.85f,0.85f);
                cx = std::clamp(cx+vx,1.0f,static_cast<float>(width-2));
                cy += 0.85f;
                if (cy >= height-3) break;
            }
        }
    }

    // Stage 5: independent floating islands in the open sky.
    if (cfg.islands.enabled) {
        const int islandCount = static_cast<int>(std::round(
            width*cfg.islands.islands_per_1000_width/1000.0f));
        std::mt19937 rng(hash32(seed+1001u));
        std::uniform_int_distribution<int> radiusDist(cfg.islands.min_radius,
                                                       cfg.islands.max_radius);
        std::uniform_real_distribution<float> heightDist(cfg.islands.min_y_fraction,
                                                         cfg.islands.max_y_fraction);
        std::uniform_int_distribution<int> centerX(0,width-1);
        for (int i=0;i<islandCount;++i) {
            const int cx=centerX(rng);
            const int cy=static_cast<int>(height*heightDist(rng));
            const float rx=static_cast<float>(radiusDist(rng));
            const float ry=std::max(2.0f,rx*cfg.islands.thickness);
            for (int x=std::max(0,static_cast<int>(cx-rx));
                 x<=std::min(width-1,static_cast<int>(cx+rx));++x) {
                const auto& biome=cfg.surface_biomes[result.surface_biome_ids[x]];
                for (int y=std::max(0,static_cast<int>(cy-ry-2));
                     y<=std::min(height-1,static_cast<int>(cy+ry+2));++y) {
                    Cell& cell=cells[at(x,y,width)];
                    if (cell.type_id != 0 || y >= result.surface_y[x]-3) continue;
                    const float dx=(x-cx)/rx, dy=(y-cy)/ry;
                    const float edge=noise(x*0.13f,y*0.13f,seed+1102u)*0.10f;
                    const float dist=dx*dx+dy*dy+edge;
                    if (dist>1.0f) continue;
                    const auto& palette=y<cy ? biome.top : biome.soil;
                    const uint16_t id=paletteId(palette,x,y,cfg.material_patch_cells,seed+1203u);
                    uint8_t density=static_cast<uint8_t>(std::clamp(
                        (1.0f-dist)*500.0f,32.0f,255.0f));
                    setMaterial(cell,id,density);
                }
            }
        }
    }

    // Stage 6: colored material patches. Replaces IDs only; never fills a cave.
    if (cfg.patches.enabled && cfg.patches.attempts_per_chunk>0.0f) {
        const int chunksX=(width+chunkSize-1)/chunkSize;
        const int chunksY=(height+chunkSize-1)/chunkSize;
        for (int cy=0;cy<chunksY;++cy) for (int cx=0;cx<chunksX;++cx) {
            std::mt19937 rng(hash2(cx,cy,seed+1304u));
            std::uniform_real_distribution<float> unit(0.0f,1.0f);
            std::uniform_int_distribution<int> radiusDist(cfg.patches.min_radius,
                                                          cfg.patches.max_radius);
            const int beginX=cx*chunkSize,beginY=cy*chunkSize;
            const int endX=std::min(width-1,beginX+chunkSize-1);
            const int endY=std::min(height-1,beginY+chunkSize-1);
            std::uniform_int_distribution<int> posX(beginX,endX),posY(beginY,endY);
            const int base=static_cast<int>(cfg.patches.attempts_per_chunk);
            const int attempts=base+(unit(rng)<cfg.patches.attempts_per_chunk-base);
            for (int a=0;a<attempts;++a) {
                if (unit(rng)>cfg.patches.probability) continue;
                const int x=posX(rng),y=posY(rng),r=radiusDist(rng);
                if (y<result.surface_y[x]+3) continue;
                const auto id=paletteId(cfg.patches.material,x,y,1,
                                        hash32(seed+1405u+static_cast<uint32_t>(a)));
                for (int yy=std::max(0,y-r);yy<=std::min(height-1,y+r);++yy)
                    for (int xx=std::max(0,x-r);xx<=std::min(width-1,x+r);++xx) {
                        const int dx=xx-x,dy=yy-y;
                        if (dx*dx+dy*dy>r*r) continue;
                        Cell& cell=cells[at(xx,yy,width)];
                        if (cell.type_id != 0 && cell.density >= 128)
                            cell.type_id=id;
                    }
            }
        }
    }

    // Stage 7: propose ground structures. WorldGrid validates and places them.
    // We never mutate the structures layer or call placeStructure from a worker.
    if (!cfg.structure_rules.empty()) {
        const int chunksX=(width+chunkSize-1)/chunkSize;
        for (int cx=0;cx<chunksX;++cx) {
            std::mt19937 rng(hash2(cx,0,seed+1506u));
            std::uniform_real_distribution<float> chance(0.0f,1.0f);
            const int minX=cx*chunkSize;
            const int maxX=std::min(width-1,minX+chunkSize-1);
            std::uniform_int_distribution<int> pos(minX,maxX);
            for (const auto& rule:cfg.structure_rules) {
                if (rule.structure_id==0 || chance(rng)>rule.chance_per_chunk) continue;
                const int x=pos(rng), y=result.surface_y[x]-1;
                const auto& biome=cfg.surface_biomes[result.surface_biome_ids[x]];
                if (!rule.surface_biome.empty() && rule.surface_biome!=biome.name) continue;
                if (x+rule.footprint_width>width ||
                    y-std::max(rule.required_clearance,rule.footprint_height)+1<0 ||
                    y+1>=height) continue;
                bool clear=true;
                for (int dx=0;dx<rule.footprint_width && clear;++dx) {
                    if (cells[at(x+dx,y+1,width)].density < 128) { clear=false;break; }
                    for (int dy=0;dy<std::max(rule.required_clearance,rule.footprint_height);++dy)
                        if (cells[at(x+dx,y-dy,width)].density>=128) {clear=false;break;}
                }
                if (clear)
                    result.proposed_structures.push_back({rule.structure_id,x,y,
                        rule.footprint_width,rule.footprint_height});
            }
        }
    }

    return result;
}
