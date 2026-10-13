#include "world_grid.h"
#include "json_loader.h"
#include "marching_squares.h"
#include "structure.h"
#include "thread_pool.h"
#include "world_types.h"
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>
#include <memory>
#include <utility>

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
        int coords[4][2]
        {
            {x, y},         // Bottom-Left
                {x + 1, y},     // Bottom-Right
                {x + 1, y + 1}, // Top-Right
                {x, y + 1}      // Top-Left
        };

        for (auto& coord : coords) {
            size_t index = static_cast<size_t>(coord[1]) * world_x_ + coord[0];
            Cell& c = world_cells_[index];
            c.type_id = type_id;
            c.density = density;
            c.state_flags = state_flags;

            int chunk_x = coord[0] / chunk_size_;
            int chunk_y = coord[1] / chunk_size_;

            // Mark the corresponding chunks as dirty so they re-render
            flag_dirty_chunk(chunk_x, chunk_y);
        }


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
    world_x_ = x_size + 1;
    world_y_ = y_size + 1;
    chunk_size_ = g_size;
    num_chunk_x = std::max(1, (world_x_ + chunk_size_ - 1) / chunk_size_);
    num_chunk_y = std::max(1,(world_y_ + chunk_size_ - 1) / chunk_size_);
    world_cells_.resize(world_x_ * world_y_);
    placed_cells_.resize(world_x_ * world_y_);
    float base_height = world_y_ * 1.f;       // Surface line 
                                               //
    float height_amplitude = world_y_ * 0.f; // Hill variance height
                                               // PARALLEL TIME !!!

    pool_ = std::make_unique<ThreadPool>();

    int numChunks = num_chunk_x * num_chunk_y;
    mesh.resize(numChunks);
    chunk_is_dirty_.assign(numChunks, 0);
    dirty_chunk_indices_.clear();

    // Load JSON structures and any other future jsons
    nlohmann::json& structure_data = JsonLoader::Instance().Get("game/structures.json");
    if(structure_data.is_array())
    {
        for (const auto& item : structure_data)
        {
            jStructure js = item.get<jStructure>();
            struct_template s;
            s.structureID = js.structureID;
            s.x_width = js.x_width;
            s.y_width = js.y_width;
            s.canTick = js.cantick;
            s.isBuilding = js.isBuilding;
            s.x_origin = 0;
            s.y_origin = 0;
            s.processing_time = js.processing_time;
            s.blockType = js.blockType;
            s.sprite = js.sprite;
            
            for (const auto& ri : js.resource_input)
            {
                resource_requirement r;
                r.rID = ri.objectID;
                r.count = ri.count;
                s.resource_input.push_back(r);
            }

            for (const auto& ro : js.resource_output)
            {

                resource_requirement r;
                r.rID = ro.objectID;
                r.count = ro.count;
                s.resource_output.push_back(r);
            }


            Structure structure = Structure(s);
            structure_registry_.insert({js.structureID, structure});
        }
    }
// Step 1: Load attack.json into a lookup map
std::unordered_map<std::string, attack> attacks_memory;
nlohmann::json& attack_data = JsonLoader::Instance().Get("game/attack.json");
if (attack_data.is_array()) {
    for (const auto& item : attack_data) {
        jAttack ja = item.get<jAttack>();
        attack a;
        a.name = ja.name;
        a.damage = ja.damage;
        a.range = ja.range;
        a.at = ja.at;
        a.cooldownBeforeNextAttack = ja.cooldownBeforeNextAttack;
        
        attacks_memory[a.name] = a;
    }
}

// Step 2: Load attack_pattern.json into a lookup map (using attacks loaded into memory)
std::unordered_map<std::string, attack_pattern> attack_patterns_memory;
nlohmann::json& pattern_data = JsonLoader::Instance().Get("game/attack_pattern.json");
if (pattern_data.is_array()) {
    for (const auto& item : pattern_data) {
        jAttackPattern jap = item.get<jAttackPattern>();
        attack_pattern ap;
        ap.name = jap.name;

        for (const auto& ref : jap.attacks) {
            if (attacks_memory.find(ref.name) != attacks_memory.end()) {
                ap.attacks.push_back(attacks_memory[ref.name]);
            }
        }
        attack_patterns_memory[ap.name] = ap;
    }
}

// Step 3: Load entity.json and create each entity by reading the attack patterns from memory
nlohmann::json& entity_data = JsonLoader::Instance().Get("game/entity.json");
if (entity_data.is_array()) {
    for (const auto& item : entity_data) {
        jEntityTemplate jet = item.get<jEntityTemplate>();
        entity_template et;
        et.entityTypeID = jet.entityTypeID;
        et.name = jet.name;
        et.sprite = jet.sprite;
        et.entity_class = jet.entity_class;
        et.behavior_type = jet.behavior_type;
        et.max_health = jet.max_health;
        et.move_speed = jet.move_speed;
        et.is_pickupable = jet.is_pickupable;
        et.width = jet.width;
        et.height= jet.height;

        // Resolve attack patterns from memory using the names provided in entity.json
        for (const auto& patRef : jet.attack_patterns) {
            if (attack_patterns_memory.find(patRef.name) != attack_patterns_memory.end()) {
                et.attack_patterns.push_back(attack_patterns_memory[patRef.name]);
            }
        }

        Entity entity = Entity(et);
        entity_registry_.insert({jet.entityTypeID, entity});
    }
}


    unsigned int num_threads = pool_->get_num_threads();
    size_t x_cells_per_thread = ((world_x_) + num_threads - 1) / num_threads;

    std::vector<std::future<void>> futures;
    futures.reserve(num_threads);

    for (unsigned int t = 0; t < num_threads; t++)
    {

        size_t start_idx = t * x_cells_per_thread;
        size_t end_idx = std::min(start_idx + x_cells_per_thread, static_cast<size_t>(world_x_));

        if (start_idx >= world_x_) break;

        futures.push_back(pool_->enqueue([this, start_idx, end_idx, &base_height, &height_amplitude]
                    {

                    for (size_t i = start_idx; i < end_idx; i ++)
                    {
                    for (size_t j = 0; j < world_y_; j ++)
                    {

                    size_t index = j * world_x_ + i;
// Calculate percentage height from bottom (0.0) to top (1.0)
                    float height_pct = 1.f - (static_cast<float>(j) / static_cast<float>(world_y_));

                    uint16_t type_id = 0; // Default: Air (ID 0)
                    float density = 0.0f;

                    if (height_pct >= 0.9f) {
                        // Top 10%: Air
                        type_id = 0;
                        density = 0.0f;
                    } 
                    else if (height_pct >= 0.8f) {
                        type_id = 1;
                        density = 255.0f;
                    } 
                    else if (height_pct >= 0.5f) {
                        type_id = 3;
                        density = 255.0f;
                    } 
                    else {
                        type_id = 4;
                        density = 255.0f;
                    }


                    world_cells_[index].type_id = type_id;
                    world_cells_[index].density = density;
                    world_cells_[index].state_flags = 0;

                    }
                    }
                    }));
    }

    for (auto& f:futures)
    {
        f.get();
    }
// ==========================================
    // MANUAL TEST FEATURES (Hill, Cave, Cutout)
    // ==========================================
    auto carve_shape = [&](int cx, int cy, float radius, uint16_t type_id, float density, bool is_additive) {
        int sx = std::max(0, static_cast<int>(cx - radius));
        int ex = std::min(world_x_ - 1, static_cast<int>(cx + radius));
        int sy = std::max(0, static_cast<int>(cy - radius));
        int ey = std::min(world_y_ - 1, static_cast<int>(cy + radius));
        float r_sq = radius * radius;

        for (int y = sy; y <= ey; ++y) {
            for (int x = sx; x <= ex; ++x) {
                float dx = x - cx;
                float dy = y - cy;
                if (dx * dx + dy * dy <= r_sq) {
                    size_t idx = y * world_x_ + x;
                    if (is_additive) {
                        world_cells_[idx].type_id = type_id;
                        world_cells_[idx].density = std::max(world_cells_[idx].density,static_cast<uint8_t>(density));
                    } else {
                        world_cells_[idx].type_id = type_id; // 0 for air cutout
                        world_cells_[idx].density = density;   // 0.0f for air cutout
                    }
                }
            }
        }
    };

    // 1. A Hill (Additive mound of Dirt/Grass on the surface)
    // Placed at 30% width, positioned near the upper surface layer
    int hill_cx = static_cast<int>(world_x_ * 0.3f);
    int hill_cy = static_cast<int>(world_y_ * 0.2f); 
    carve_shape(hill_cx, hill_cy, 15.0f, 2, 255.0f, true); // Grass mound

    // 2. A Cave Entrance (Subtractive circular arch cut into a wall/hillside)
    // Placed at 50% width, deeper down in the dirt/stone layers
    int cave_cx = static_cast<int>(world_x_ * 0.5f);
    int cave_cy = static_cast<int>(world_y_ * 0.5f);
    carve_shape(cave_cx, cave_cy, 8.0f, 0, 0.0f, false); // Air pocket

    // 3. A Small Cutout (A distinct geometric bite taken out of the lower stone layer)
    // Placed at 70% width, near the bottom
    int cut_cx = static_cast<int>(world_x_ * 0.7f);
    int cut_cy = static_cast<int>(world_y_ * 0.75f);
    carve_shape(cut_cx, cut_cy, 10.0f, 0, 0.0f, false); // Air pocket
    // ==========================================

    chunk_is_dirty_.assign(numChunks, 1);

    dirty_chunk_indices_.clear();
    dirty_chunk_indices_.reserve(numChunks);
    for (int i = 0; i < numChunks; ++i)
    {
        dirty_chunk_indices_.push_back(i);
    }

    // updateMesh();



}

std::vector<std::pair<std::array<int, 2>,ChunkMeshData>> WorldGrid::updateMesh()
{
    if (dirty_chunk_indices_.empty()) return {};


    size_t total_dirty = dirty_chunk_indices_.size();

    using ChunkMeshResult = std::pair<std::array<int, 2>, ChunkMeshData>;
    std::vector<ChunkMeshResult> local_outputs(total_dirty);

    unsigned int num_threads = std::min(
            static_cast<size_t>(pool_->get_num_threads()),
            total_dirty);

    size_t chunks_per_thread = (total_dirty + num_threads - 1) / num_threads;
    std::vector<std::future<void>> futures;
    futures.reserve(num_threads);


    for (unsigned int t = 0; t < num_threads; t++)
    {
        size_t start_idx = t * chunks_per_thread;
        size_t end_idx = std::min(start_idx + chunks_per_thread, total_dirty);

        if (start_idx >= total_dirty) break;

        futures.push_back(pool_->enqueue([this, start_idx, end_idx, &local_outputs]
                    {
                    MarchingSquares local_marching_squares;
                    for (size_t i = start_idx; i < end_idx; i ++)
                    {

                    int chunk_idx = dirty_chunk_indices_[i];

                    int cx = chunk_idx % num_chunk_x;
                    int cy = chunk_idx / num_chunk_x;

                    MeshData terrain_mesh= local_marching_squares.GenerateMesh(world_cells_, 128.f, world_x_, world_y_, cx, cy, chunk_size_);
                    MeshData structure_mesh = local_marching_squares.generate_blocky_mesh(placed_cells_, world_x_, world_y_, cx, cy, chunk_size_, structures_);

                    ChunkMeshData chunk_data{std::move(terrain_mesh), std::move(structure_mesh)};
                    local_outputs[i] = std::make_pair(std::array<int, 2>{cx, cy}, std::move(chunk_data));

                    }
                    }));
    }

    for (auto& f:futures)
    {
        f.get();
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

uint32_t WorldGrid::tick(double deltaTime)
{
    // could be parallel
 //   for(uint32_t sID : tick_structures_)
    {
        //Structure& s = structures_.at(sID); 
       // s.on_tick(deltaTime);
    }

    // also do entity processes
    
    
    time_accumulator_ += deltaTime;
    uint32_t active_flags = TickFlags::NONE;

    if (!entities_.empty()) {
        for (auto& [uID, entity] : entities_) {
            entity.on_tick(deltaTime); // updates internal smooth positions
        }
        active_flags |= TickFlags::ENTITY_UPDATE;
    }
    /*

    if (time_accumulator_ >= spawn_interval_)
    {
        time_accumulator_ -= spawn_interval_;

    }
    // 2. Check if counter/interval is reached (e.g., every 2 seconds)
    if (time_accumulator_ >= spawn_interval_)
    {
        time_accumulator_ -= spawn_interval_; // Reset/subtract interval

        // 3. Generate random coordinates on the map
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist_x(0, world_x_- 1);
        std::uniform_int_distribution<int> dist_y(0, world_y_- 1);

        int random_x = dist_x(gen);
        int random_y = dist_y(gen);

        // 4. Place structure ID 5 at the random location
        placeStructure(random_x, random_y, 5);
        active_flags |= TickFlags::MESH_UPDATE;
    }
    */
    return active_flags;

}

 struct_template WorldGrid::selectStructure(int x, int y)
{
    struct_template r;
    r.structureID = 0;
    r.blockType = -1;
    r.canTick = false;
    r.instanceID = 0; // instance id 0 is possible though...
    r.isBuilding = false;
    r.processing_time = -1.f;
    r.sprite = "NAN";

    if (x< 0 || x >= world_x_ ||
        y < 0 || y >= world_y_)
        return r;

    size_t index= static_cast<size_t>(y) * world_x_ + x;
    const Cell& c= placed_cells_[index];

    if (c.structure_on_cell_uID != -1)
    {
        //Have an entity 
        return structures_.at(c.structure_on_cell_uID).getTemplate();
    }

    // else we may just have a regular block so lets check
    if (c.type_id != 0)
    {
        //There's just a block here
        // Will have to figure out this return at some point
        return r;
    }

    return r;

} 

// placing a structure
// -> Placing actual buildings (System not yet implemented ill figure it out)
// -> Placing blocks
void WorldGrid::placeStructure(int bl_x, int bl_y, int structureID)
{

    if (bl_x < 0 || bl_x >= world_x_ ||
        bl_y < 0 || bl_y >= world_y_)
        return;

    size_t origin_index = static_cast<size_t>(bl_y) * world_x_ + bl_x;

    const Cell& origin = placed_cells_[origin_index];

    // Don't place on an occupied cell.
    if (origin.type_id != 0 || origin.structure_on_cell_uID != -1)
        return;
    if (structure_registry_.find(structureID) == structure_registry_.end()) return;

    struct_template st = structure_registry_.at(structureID).getTemplate();

    int w = st.x_width;
    int h = st.y_width;
    // Validate the entire footprint before placing anything.
    for (int dy = 0; dy < h; ++dy)
    {
        for (int dx = 0; dx < w; ++dx)
        {
            int x = bl_x + dx;
            int y = bl_y - dy;

            // Reject if any part of the structure is outside the world.
            if (x < 0 || x >= world_x_ || y < 0 || y >= world_y_)
                return;

            size_t index = static_cast<size_t>(y) * world_x_ + x;
            const Cell& cell = placed_cells_[index];

            // Reject if any cell is already occupied.
            if (cell.type_id != 0 ||
                cell.structure_on_cell_uID != -1)
            {
                return;
            }
        }
    }

    for (int dy = 0; dy < h; ++dy)
    {
        for (int dx = 0; dx < w; ++dx)
        {
            int x = bl_x + dx;
            int y = bl_y - dy;

            if (x >= 0 && x < world_x_ && y >= 0 && y < world_y_)
            {
                size_t index = static_cast<size_t>(y) * world_x_ + x;

                placed_cells_[index].type_id = static_cast<uint16_t>(structureID);
                placed_cells_[index].density = 255.0f;
                placed_cells_[index].state_flags = 0;

                world_cells_[index].type_id = 0;
                world_cells_[index].density = 0.0f;


                int cx = x / chunk_size_;
                int cy = y / chunk_size_;
                flag_dirty_chunk(cx, cy);
            }
        }
    }

    if (st.isBuilding)
    {

        int uID = getNextStructureUniqueID(); // buildings require unique ID's
                                              //
        st.instanceID = uID;
        st.x_origin = bl_x;
        st.y_origin = bl_y;

        if(st.canTick)
        {
            tick_structures_.push_back(uID);
        }
        structures_.try_emplace(uID, Structure(st));

        for (int dy = 0; dy < h; ++dy)
        {
            for (int dx = 0; dx < w; ++dx)
            {
                int x = bl_x + dx;
                int y = bl_y - dy;

                if (x >= 0 && x < world_x_ && y >= 0 && y < world_y_)
                {
                    size_t index = static_cast<size_t>(y) * world_x_ + x;

                    placed_cells_[index].structure_on_cell_uID = uID;
                }
            }
        }

    }
}
void WorldGrid::placeEntity(int bl_x, int bl_y, int entityTypeID)
{
    // 1. Check world boundary bounds for the origin point
    if (bl_x < 0 || bl_x >= world_x_ ||
        bl_y < 0 || bl_y >= world_y_)
        return;

    // 2. Ensure entity template exists in the registry
    if (entity_registry_.find(entityTypeID) == entity_registry_.end()) 
        return;

    entity_template et = entity_registry_.at(entityTypeID).getTemplate();

    int w = et.width;  
    int h = et.height; 

    // 3. Validate the entire footprint (must be air/empty blocks, can't be placed inside solid tiles)
    for (int dy = 0; dy < h; ++dy)
    {
        for (int dx = 0; dx < w; ++dx)
        {
            int x = bl_x + dx;
            int y = bl_y - dy;

            // Reject if any part of the entity is outside the world boundary
            if (x < 0 || x >= world_x_ || y < 0 || y >= world_y_)
                return;

            size_t index = static_cast<size_t>(y) * world_x_ + x;
            const Cell& cell = placed_cells_[index];

            // Rule: The background where placed must be air (type_id == 0 means air/empty)
            // Note: Other entities can share the tile, but it cannot be a solid block type.
            if (cell.type_id != 0)
            {
                return;
            }
        }
    }

    // 4. Generate unique ID for this entity instance
    int uID = getNextEntityUniqueID(); 
    et.instanceID = uID;
    et.x_origin = bl_x;
    et.y_origin = bl_y;

    et.x_smooth = bl_x;
    et.y_smooth = bl_y;


    // 5. Register and spawn the entity instance
    entities_.try_emplace(uID, Entity(et));

    // you can push the uID into cell reference lists here since multiple entities can share a tile.
    for (int dy = 0; dy < h; ++dy)
    {
        for (int dx = 0; dx < w; ++dx)
        {
            int x = bl_x + dx;
            int y = bl_y - dy;

            if (x >= 0 && x < world_x_ && y >= 0 && y < world_y_)
            {
                size_t index = static_cast<size_t>(y) * world_x_ + x;
                // How would i track that this entity takes up the footprint of w*h?
                // Simply adding uID is technically a solution but i can see more spaghetti forming as a result
                // If the user attacks the entity on 5,5 with size (2x3)
                // -> What if i attack on cell 6,4 ? It should still hit but how do i know
                placed_cells_[index].entities_on_cell_uID.push_back(uID);
            }
        }
    }
}


/*
 * ALL OF THIS NEEDS A BIG REFACTOR
 *
 *
 */
// magic numbers everwhere
bool WorldGrid::isCellSolid(float x, float y) const {
    int cell_x = static_cast<int>(std::floor(x));
    int cell_y = static_cast<int>(std::floor(y));

    if (cell_x < 0 || cell_x >= world_x_ || cell_y < 0 || cell_y >= world_y_) {
        return true; // Treat out-of-bounds as solid walls
    }

    size_t index = static_cast<size_t>(cell_y) * world_x_ + cell_x;
    const Cell& c = world_cells_[index];
    
    return (c.type_id != 0 && c.density > 128.0f);
}

float WorldGrid::getDensityAt(float x, float y) const {
    int cell_x = static_cast<int>(std::floor(x));
    int cell_y = static_cast<int>(std::floor(y));

    if (cell_x < 0 || cell_x >= world_x_ || cell_y < 0 || cell_y >= world_y_) {
        return 0.0f; // Treat outside as empty air
    }

    size_t index = static_cast<size_t>(cell_y) * world_x_ + cell_x;
    return static_cast<float>(world_cells_[index].density);
}

Vec2 WorldGrid::calculateDensityGradient(float x, float y) const {
    float eps = 1.0f; // 1 cell offset
    
    float d_left   = getDensityAt(x - eps, y);
    float d_right  = getDensityAt(x + eps, y);
    float d_top    = getDensityAt(x, y - eps);
    float d_bottom = getDensityAt(x, y + eps);

    float gx = (d_right - d_left) * 0.5f;
    float gy = (d_top - d_bottom) * 0.5f; // Normal points upward into air

    Vec2 grad(gx, gy);
    float len = std::sqrt(grad.x * grad.x + grad.y * grad.y);
    if (len > 0.0001f) {
        grad.x /= len;
        grad.y /= len;
    }
    return grad;
}
Vec2 WorldGrid::resolveCollision(Vec2 current_pos, Vec2& velocity, float width_cells, float height_cells, double deltaTime) {
    Vec2 new_pos = current_pos;

    float dx = velocity.x * static_cast<float>(deltaTime);
    new_pos.x += dx;

    if (checkAABBCollision(new_pos, width_cells, height_cells)) {
        new_pos.x = current_pos.x; // Stop dead on X walls
        velocity.x = 0.0f;
    }

    float dy = velocity.y * static_cast<float>(deltaTime);
    new_pos.y += dy;

    float half_w = width_cells * 0.5f;
    float half_h = height_cells * 0.5f;

    if (velocity.y > 0.0f) {
        // Falling: Check bottom corners
        float left = new_pos.x - half_w;
        float right = new_pos.x + half_w;
        float bottom = new_pos.y + half_h;

        if (isCellSolid(left, bottom) || isCellSolid(right, bottom)) {
            velocity.y = 0.0f;
            new_pos.y = current_pos.y; // Stop dead on ground with zero bounce
        }
    } 
    else if (velocity.y < 0.0f) {
        // Jumping/Rising: Check top corners
        float left = new_pos.x - half_w;
        float right = new_pos.x + half_w;
        float top = new_pos.y - half_h;

        if (isCellSolid(left, top) || isCellSolid(right, top)) {
            velocity.y = 0.0f;
            new_pos.y = current_pos.y; // Hit ceiling, stop rise
        }
    }

    return new_pos;
}

bool WorldGrid::checkAABBCollision(const Vec2& pos, float width, float height) const {
    float half_w = width * 0.5f;
    float half_h = height * 0.5f;

    float left = pos.x - half_w;
    float right = pos.x + half_w;
    float top = pos.y - half_h;
    float bottom = pos.y + half_h;

    return isCellSolid(left, top) || isCellSolid(right, top) ||
           isCellSolid(left, bottom) || isCellSolid(right, bottom);
}
WorldGrid::WorldGrid()
{
}
WorldGrid::~WorldGrid()
{
}
