#include "world_grid.h"
#include "marching_squares.h"
#include "world_types.h"
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <thread>

Cell WorldGrid::get_cell(int x, int y) const
{
    if(x >= 0 && x < world_x_ && y >= 0 && y < world_y_)
    {
        size_t index = static_cast<size_t>(y) * world_x_ + x;
        return world_cells_[index]; 
    }
    return Cell();
    // returning default cell is empty cell (type_id = 0)
}
// bool = happened : error
bool WorldGrid::set_cell(int x, int y, uint16_t type_id, uint8_t density, uint8_t state_flags)
{

    if(x >= 0 && x < world_x_ && y >= 0 && y < world_y_)
    {
        size_t index = static_cast<size_t>(y) * world_x_ + x;
        Cell& c = world_cells_[index];
        c.type_id = type_id;
        c.density = density;
        c.state_flags = state_flags;

        // mark dirty chunk
        return true;
    }
    return false;
}


void WorldGrid::set_density(int x, int y, float radius, float strength, uint16_t type_id)
{
    if (strength == 0.0f || radius <= 0.0f) return;

    int cx = x;
    int cy = y;
    int sx = std::max(0, static_cast<int>(std::floor(cx - radius)));
    int ex = std::min(world_x_ - 1, static_cast<int>(std::floor(cx + radius)));
    int sy = std::max(0, static_cast<int>(std::floor(cy - radius)));
    int ey = std::min(world_y_ - 1, static_cast<int>(std::floor(cy + radius)));

    if (sx > ex || sy > ey) return;

    // Precalculate loop invariants
    const float radius_sq = radius * radius;
    const float inv_radius = 1.0f / radius;
    constexpr float PI = 3.14159265358979323846f;

    // Local stack buffer to batch dirty chunk updates (avoids std::set allocations in hot loop)
    const int max_dirty_chunks = 256;

    auto mark_chunk_dirty = [&](int ch_x, int ch_y) {
        if (ch_x < 0 || ch_x >= num_chunk_x || ch_y < 0 || ch_y >= num_chunk_y) return;

        int chunk_idx = ch_y * num_chunk_x + ch_x;
        if (!chunk_is_dirty_[chunk_idx])
        {
            chunk_is_dirty_[chunk_idx] = 1;
            dirty_chunk_indices_.push_back(chunk_idx);
        }

    };

    for (int iy = sy; iy <= ey; ++iy)
    {
        int dy = iy - cy;
        int dy_sq = dy * dy;
        size_t row_offset = static_cast<size_t>(iy) * world_x_;

        for (int ix = sx; ix <= ex; ++ix)
        {
            int dx = ix - cx;
            float dist_sq = static_cast<float>(dx * dx + dy_sq);

            if (dist_sq <= radius_sq)
            {
                size_t index = row_offset + ix;
                Cell& cell = world_cells_[index];

                float dist = std::sqrt(dist_sq);
                float falloff = 1.0f - (dist * inv_radius);
                falloff = (1.0f - std::cos(falloff * PI)) * 0.5f;
                float scaled_strength = strength * falloff;

                if (strength > 0.0f)
                {
                    if (cell.type_id == 0 || cell.type_id == type_id)
                    {
                        cell.type_id = type_id;
                        cell.density = std::clamp(cell.density + scaled_strength, 0.0f, 255.0f);
                    }
                    else
                    {
                        cell.density = std::clamp(cell.density - scaled_strength, 0.0f, 255.0f);
                        if (cell.density <= 128.0f)
                        {
                            cell.type_id = type_id;
                            cell.density = 255.0f - cell.density;
                        }
                    }
                }
                else // strength < 0.0f
                {
                    cell.density = std::clamp(cell.density + scaled_strength, 0.0f, 255.0f);
                }

                if (cell.density <= 0.0f)
                {
                    cell.type_id = 0;
                    cell.density = 0.0f;
                }

                // Collect dirty chunk coordinates in local buffer
                int chunk_x = ix / chunk_size_;
                int chunk_y = iy / chunk_size_;

                mark_chunk_dirty(chunk_x, chunk_y);

                if (ix % chunk_size_ == chunk_size_ - 1) {
                    mark_chunk_dirty(chunk_x + 1, chunk_y);
                }
                if (iy % chunk_size_ == chunk_size_ - 1) {
                    mark_chunk_dirty(chunk_x, chunk_y + 1);
                }
            }
        }
    }

}

std::array<int, 2> WorldGrid::get_chunk_coords(int x, int y)
{
    if (x < 0 || x >= world_x_ || y < 0 || y >= world_y_)
    {
        return {-1, -1};
    }

    return {x / chunk_size_, y / chunk_size_};

}
void WorldGrid::flag_dirty_chunk(int cx, int cy)
{
    if (cx < 0 || cx >= num_chunk_x || cy < 0 || cy >= num_chunk_y) return;

    int chunk_idx = cy * num_chunk_x + cx;
    if (!chunk_is_dirty_[chunk_idx]) {
        chunk_is_dirty_[chunk_idx] = 1;
        dirty_chunk_indices_.push_back(chunk_idx);
    }
}

namespace NativeNoise {
    static const int p[512] = {
        151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
        8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
        35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
        134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
        55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
        18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
        250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
        189,28,42,223,183,170,213,119,248,152,2,44,145,31,179,228,167,218,204,124,34,
        254,19,84,236,98,138,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,
        156,180,151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,
        69,142,8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,
        203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,
        165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,
        92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,
        89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,
        226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,
        182,189,28,42,223,183,170,213,119,248,152,2,44,145,31,179,228,167,218,204,124,
        34,254,19,84,236,98,138,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,
        61,156,180
    };

    inline float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    inline float lerp(float t, float a, float b) { return a + t * (b - a); }
    inline float grad(int hash, float x, float y) {
        int h = hash & 7;
        float u = h < 4 ? x : y;
        float v = h < 4 ? y : x;
        return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
    }

    inline float eval(float x, float y) {
        int X = static_cast<int>(std::floor(x)) & 255;
        int Y = static_cast<int>(std::floor(y)) & 255;
        x -= std::floor(x);
        y -= std::floor(y);
        float u = fade(x);
        float v = fade(y);
        int A = p[X] + Y, B = p[X + 1] + Y;
        return lerp(v, lerp(u, grad(p[A], x, y), grad(p[B], x - 1, y)),
                lerp(u, grad(p[A + 1], x, y - 1), grad(p[B + 1], x - 1, y - 1)));
    }

    // Octave/Fractal Noise
    inline float fBm(float x, float y, int octaves, float frequency, float persistence) {
        float total = 0.0f;
        float max_val = 0.0f;
        float amp = 1.0f;
        for (int i = 0; i < octaves; ++i) {
            total += eval(x * frequency, y * frequency) * amp;
            max_val += amp;
            amp *= persistence;
            frequency *= 2.0f;
        }
        return total / max_val;
    }
}

void WorldGrid::init(int x_size, int y_size, int g_size)
{
    world_x_ = x_size;
    world_y_ = y_size;
    chunk_size_ = g_size;
    num_chunk_x = world_x_ / chunk_size_;
    num_chunk_y = world_y_ / chunk_size_;
    world_cells_.resize(world_x_ * world_y_);
    float base_height = world_y_ * 0.45f;       // Surface line (~45% down the map)
    float height_amplitude = world_y_ * 0.25f; // Hill variance height
                                               // PARALLEL TIME !!!


    int numChunks = num_chunk_x * num_chunk_y;
    mesh.resize(numChunks);
    chunk_is_dirty_.assign(numChunks, 0);
    dirty_chunk_indices_.clear();

    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 1;

    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    int numCells = world_x_ * world_y_;

    size_t x_cells_per_thread = ((world_x_) + num_threads - 1) / num_threads;

    for (unsigned int t = 0; t < num_threads; t++)
    {

        size_t start_idx = t * x_cells_per_thread;
        size_t end_idx = std::min(start_idx + x_cells_per_thread, static_cast<size_t>(world_x_));

        if (start_idx >= world_x_) break;

        workers.emplace_back([this, start_idx, end_idx, &base_height, &height_amplitude]
                {

                for (size_t i = start_idx; i < end_idx; i ++)
                {
                for (size_t j = 0; j < world_y_; j ++)
                {

                size_t index = j * world_x_ + i;

                // 1. Calculate surface height curve (1D noise across X axis)
                float noise_val = NativeNoise::fBm(static_cast<float>(i) * 0.03f, 0.0f, 4, 1.0f, 0.5f);
                float surface_y = base_height + (noise_val * height_amplitude);

                // 2. Calculate cell density relative to depth
                float depth = static_cast<float>(j) - surface_y;
                float density = std::clamp((depth + 1.0f) * 128.0f, 0.0f, 255.0f);

                uint16_t type_id = 0; // Default: Air (ID 0)

                if (density > 0.0f)
                {
                    // 3. Biome depth layering
                    if (depth < 2.5f) {
                        type_id = 2; // Grass (ID 2): Surface layer
                    } 
                    else if (depth < 14.0f) {
                        // Dirt layer with Stone pockets using 2D noise
                        float mat_n = NativeNoise::eval(static_cast<float>(i) * 0.1f, static_cast<float>(j) * 0.1f);
                        type_id = (mat_n > 0.25f) ? 3 : 1; // Stone (3) vs Dirt (1)
                    } 
                    else {
                        // Underground: Mostly Stone with Dirt veins
                        float mat_n = NativeNoise::eval(static_cast<float>(i) * 0.06f, static_cast<float>(j) * 0.06f);
                        type_id = (mat_n > -0.1f) ? 3 : 1; // Stone (3) vs Dirt (1)
                    }
                }

                world_cells_[index].type_id = type_id;
                world_cells_[index].density = density;
                world_cells_[index].state_flags = 0;

                }
                }
                });
    }

    for (auto& worker : workers)
    {
        worker.join();
    }

    chunk_is_dirty_.assign(numChunks, 1);

    dirty_chunk_indices_.clear();
    dirty_chunk_indices_.reserve(numChunks);
    for (int i = 0; i < numChunks; ++i)
    {
        dirty_chunk_indices_.push_back(i);
    }

   // updateMesh();





}

std::vector<std::pair<std::array<int, 2>, MeshData>> WorldGrid::updateMesh()
{
    if (dirty_chunk_indices_.empty()) return {};


    size_t total_dirty = dirty_chunk_indices_.size();

    using ChunkMeshResult = std::pair<std::array<int, 2>, MeshData>;
    std::vector<ChunkMeshResult> local_outputs(total_dirty);

    unsigned int num_threads = std::min(
            static_cast<size_t>(std::thread::hardware_concurrency()),
            total_dirty);
    if (num_threads == 0) num_threads = 1;

    size_t chunks_per_thread = (total_dirty + num_threads - 1) / num_threads;
    std::vector<std::thread> workers;

    workers.reserve(num_threads);

    for (unsigned int t = 0; t < num_threads; t++)
    {
        size_t start_idx = t * chunks_per_thread;
        size_t end_idx = std::min(start_idx + chunks_per_thread, total_dirty);

        if (start_idx >= total_dirty) break;

        workers.emplace_back([this, start_idx, end_idx, &local_outputs]
                {
                MarchingSquares local_marching_squares;
                for (size_t i = start_idx; i < end_idx; i ++)
                {

                int chunk_idx = dirty_chunk_indices_[i];

                int cx = chunk_idx % num_chunk_x;
                int cy = chunk_idx / num_chunk_x;

                MeshData mesh_data = local_marching_squares.GenerateMesh(world_cells_, 128.f, world_x_, world_y_, cx, cy, chunk_size_);

                local_outputs[i] = std::make_pair(std::array<int, 2>{cx, cy}, std::move(mesh_data));

                }
                });
    }

    for (auto& worker : workers)
    {
        worker.join();
    }

    for (size_t i = 0; i < total_dirty; i ++)
    {
        int chunk_idx = dirty_chunk_indices_[i];

        mesh[chunk_idx] = local_outputs[i].second;
        chunk_is_dirty_[chunk_idx] = 0;
    }
    dirty_chunk_indices_.clear();

    return local_outputs;



}


void WorldGrid::fill_cell(int x, int y, uint16_t type_id, bool negativeFill)
{
    if (x < 0 || x >= world_x_ || y < 0 || y >= world_y_) return;

    float density = negativeFill ? 0.0f : 255.0f;
    size_t index = static_cast<size_t>(y) * world_x_ + x;
    world_cells_[index].density = density;
    world_cells_[index].type_id = type_id;

    int cx = x / chunk_size_;
    int cy = y / chunk_size_;

    flag_dirty_chunk(cx, cy);

    // Seam neighbor checks
    if (x % chunk_size_ == chunk_size_ - 1) {
        flag_dirty_chunk(cx + 1, cy);
    }
    if (y % chunk_size_ == chunk_size_ - 1) {
        flag_dirty_chunk(cx, cy + 1);
    }
}

WorldGrid::WorldGrid()
{
}
WorldGrid::~WorldGrid()
{
}
