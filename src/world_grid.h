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

class WorldGrid
{
    private:
        int world_x_ = 6400;
        int world_y_ = 1800;
        int chunk_size_ = 64; // always square
        std::vector<Cell> world_cells_;
        std::map<std::array<int,2>, MeshData> mesh; 
        std::set<std::array<int, 2>> dirtyChunks;

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

        std::map<std::array<int,2 >, MeshData> getMeshData() const {return mesh;}
        std::vector<std::pair<std::array<int, 2>, MeshData>> updateMesh();

        void set_density(int x, int y, float radius, float strength, uint16_t type_id);
        void fill_cell(int x, int y, uint16_t type_id, bool negativeFill = false);


    protected:
};

#endif
