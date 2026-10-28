#ifndef WORLD_GRID_H
#define WORLD_GRID_H

#include "math_types.h"
#include "world_types.h"
#include "marching_squares.h"
#include <cstdint>
#include "thread_pool.h"
#include "structure.h"
#include <unordered_map>
#include <random>
#include "entity.h"
#include "player.h"
#include "memory_usage.h"
#include <fstream>
#include <iomanip>
#include <cstddef>
#include "terrain_generator.h"
#include "item.h"

// what will be updated on each tick if any
namespace TickFlags {
    constexpr uint32_t NONE          = 0;
    constexpr uint32_t MESH_UPDATE   = 1 << 0; // 1
    constexpr uint32_t UI_UPDATE     = 1 << 1; // 2
    constexpr uint32_t ENTITY_UPDATE = 1 << 2; // 4
}

class WorldGrid
{
    private:
        int world_x_ = 6400;
        int world_y_ = 1800;
        int chunk_size_ = 64; // always square
        int num_chunk_x = 0;
        int num_chunk_y = 0;
        float threshold = 0.f;
        std::vector<Cell> world_cells_;
        std::vector<Cell> placed_cells_;
        std::vector<ChunkMeshData> mesh;
        std::vector<int> dirty_chunk_indices_;
        std::vector<uint8_t> chunk_is_dirty_;
        std::unique_ptr<ThreadPool> pool_;
        TerrainGenerationResult generation_data_; // surface heights, biome IDs, proposals

        MarchingSquares marchingSquares = MarchingSquares();

        std::unordered_map<uint32_t, Structure> structures_ = {}; // All structures
        std::vector<uint32_t> tick_structures_; // store ID and lookup data when needed

        uint32_t current_structure_unique_ID_ = 1; // for anything that needs a uid
        uint32_t current_entity_unique_ID_ = 1; // for anything that needs a uid
        std::unordered_map<int, Structure> structure_registry_;
        std::unordered_map<int, Entity> entity_registry_;

        std::unordered_map<uint32_t, Entity> entities_ = {};
        std::vector<uint32_t> tick_entities_;


        //Just testing the tick
        double time_accumulator_ = 0.0;
        double spawn_interval_ = 1;


        Player player;
        
        ItemRegistry item_registry_;


        



    private: // technically all helper functions

        // default access in vector: index = (y * world_x) + x
        Cell get_cell(int x, int y) const;
        // bool = happened : error
        bool set_cell(int x, int y, uint16_t type_id, uint8_t density, uint8_t state_flags);

        

        void flag_dirty_chunk(int cx, int cy);
        std::array<int, 2> get_chunk_coords(int x, int y);
        uint32_t getNextStructureUniqueID() 
        {
            uint32_t ID = current_structure_unique_ID_;
            current_structure_unique_ID_ ++;
            return ID;
        }

        uint32_t getNextEntityUniqueID() 
        {
            uint32_t ID = current_entity_unique_ID_;
            current_entity_unique_ID_ ++;
            return ID;
        }


        
    public:
        void init(
            int x_size,
            int y_size,
            int g_size,
            float threshold
        );
        WorldGrid();
        ~WorldGrid();

        const std::vector<ChunkMeshData>& getMeshData() const {return mesh;}
        std::vector<std::pair<std::array<int, 2>, ChunkMeshData>> updateMesh();

        void set_density(int x, int y, float radius, float strength, uint16_t type_id);
        void fill_cell(int x, int y, uint16_t type_id, bool negativeFill = false);

        int get_num_chunk_x() const {return num_chunk_x; }
        const TerrainGenerationResult& getGenerationData() const { return generation_data_; }

        // Enable move assignment if re-assigning world_grid_ = WorldGrid();
        WorldGrid(WorldGrid&&) noexcept = default;
        WorldGrid& operator=(WorldGrid&&) noexcept = default;

        // Delete copy assignment explicitly
        WorldGrid(const WorldGrid&) = delete;
        WorldGrid& operator=(const WorldGrid&) = delete;

        ThreadPool& get_thread_pool() { return *pool_; }

        uint32_t tick(double deltaTime); // Each frame update function
                                     //
        void placeStructure(int bl_x, int bl_y, int structureID);
        void placeEntity(int bl_x, int bl_y, int entityID);

        struct_template selectStructure(int x, int y);

        const std::unordered_map<int, Structure>& getStructureRegistry() const {return structure_registry_;}
        const std::unordered_map<int, Entity>& getEntityRegistry() const {return entity_registry_;}
        const std::unordered_map<uint32_t, Entity>& getEntities() const {return entities_;}
        const ItemRegistry& getItemRegistry() const { return item_registry_; }


        void createPlayer(PlayerSettings ps)
        {
            player = Player(std::move(ps));
        }

        void updatePlayerPosition(float x, float y) {
            //player.setPosition(x,y);
        }
        
        const Vec2& getPlayerPosition() const {
            return player.getPosition();
        }

        void movePlayer(Vec2 input, bool jump_pressed, double deltaTime);
        const Player& getPlayer() const {return player;}



    protected:
};

#endif
