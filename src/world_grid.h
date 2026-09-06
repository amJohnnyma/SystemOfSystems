// data layer
//
// Allocate and manage 1D cell density array in RAM



#ifndef WORLD_GRID_H
#define WORLD_GRID_H

#include "world_types.h"
#include "marching_squares.h"
#include <cstdint>
#include <thread>

#include <set>
/*
 Replace std::map with Flat std::vector: Using std::map<std::array<int,2>, MeshData> incurs tree-node dynamic allocations and cache misses. Use a 1D std::vector<MeshData> sized to num_chunks_x * num_chunks_y and access chunks via cy * num_chunks_x + cx.

Replace std::set for Dirty Chunks: std::set<std::array<int,2>> allocates memory on every inserted dirty chunk during painting. Replace it with a std::vector<bool> chunk_is_dirty mask alongside a std::vector<int> dirty_chunk_indices list.

Pass Mesh Data by Reference: getMeshData() in world_grid.h returns std::map by value, duplicating all CPU mesh buffers before Godot converts them. Change the signature to return const auto&.
Implement a Persistent Thread Pool: updateMesh() spawns and joins fresh OS threads every time terrain is modified. Replacing on-demand std::thread construction with a persistent Thread Pool or task queue will remove thread lifecycle overhead during active terrain updates.
 */

class WorldGrid
{
    private:
        int world_x_ = 6400;
        int world_y_ = 1800;
        int chunk_size_ = 64; // always square
        int num_chunk_x = 0;
        int num_chunk_y = 0;
        std::vector<Cell> world_cells_;
        std::vector<MeshData> mesh;
        std::vector<int> dirty_chunk_indices_;
        std::vector<uint8_t> chunk_is_dirty_;

        MarchingSquares marchingSquares = MarchingSquares();



    private: // technically all helper functions

        // default access in vector: index = (y * world_x) + x
        //
        //
        Cell get_cell(int x, int y) const;
        // bool = happened : error
        bool set_cell(int x, int y, uint16_t type_id, uint8_t density, uint8_t state_flags);

        // Would i need a specific addDensity, removeDensity, etc.
        

        void flag_dirty_chunk(int cx, int cy);
        std::array<int, 2> get_chunk_coords(int x, int y);


        
    public:
        void init(int x_size = 6400, int y_size = 1800, int g_size = 64);
        WorldGrid();
        ~WorldGrid();

        const std::vector<MeshData>& getMeshData() const {return mesh;}
        std::vector<std::pair<std::array<int, 2>, MeshData>> updateMesh();

        void set_density(int x, int y, float radius, float strength, uint16_t type_id);
        void fill_cell(int x, int y, uint16_t type_id, bool negativeFill = false);

        int get_num_chunk_x() const {return num_chunk_x; }


    protected:
};

#endif
