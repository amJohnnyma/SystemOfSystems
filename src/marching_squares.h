#ifndef MARCHING_SQUARES_H
#define MARCHING_SQUARES_H

#include <array>
#include "world_types.h"
#include "json_loader.h"
#include <set>



// from gemini
// Edges: 0 = Top, 1 = Right, 2 = Bottom, 3 = Left
// Up to 2 line segments (4 edge references) per corner configuration, -1 padded
static constexpr std::array<std::array<int, 4>, 16> EDGE_TABLE = {{
    {-1, -1, -1, -1}, // Case 0
        { 3,  2, -1, -1}, // Case 1
        { 2,  1, -1, -1}, // Case 2
        { 3,  1, -1, -1}, // Case 3
        { 0,  1, -1, -1}, // Case 4
        { 3,  0,  2,  1}, // Case 5 (Ambiguous/Saddle)
        { 0,  2, -1, -1}, // Case 6
        { 3,  0, -1, -1}, // Case 7
        { 3,  0, -1, -1}, // Case 8
        { 0,  2, -1, -1}, // Case 9
        { 3,  2,  0,  1}, // Case 10 (Ambiguous/Saddle)
        { 0,  1, -1, -1}, // Case 11
        { 3,  1, -1, -1}, // Case 12
        { 2,  1, -1, -1}, // Case 13
        { 3,  2, -1, -1}, // Case 14
        {-1, -1, -1, -1}  // Case 15
}};
static constexpr int TRIANGLE_TABLE[16][16] = {
    { -1 },                                          // Case 0: Empty
    { 0, 6, 7, -1 },                                 // Case 1: BL solid
    { 1, 5, 6, -1 },                                 // Case 2: BR solid
    { 0, 1, 5,  0, 5, 7, -1 },                       // Case 3: BL + BR solid
    { 2, 4, 5, -1 },                                 // Case 4: TR solid
    { 0, 6, 7,  2, 4, 5, -1 },                       // Case 5: BL + TR solid (Saddle)
    { 1, 2, 4,  1, 4, 6, -1 },                       // Case 6: BR + TR solid
    { 0, 1, 2,  0, 2, 7,  7, 2, 4, -1 },             // Case 7: BL + BR + TR solid
    { 3, 7, 4, -1 },                                 // Case 8: TL solid
    { 0, 6, 3,  6, 4, 3, -1 },                       // Case 9: BL + TL solid
    { 1, 5, 6,  3, 7, 4, -1 },                       // Case 10: BR + TL solid (Saddle)
    { 0, 1, 5,  0, 5, 4,  0, 4, 3, -1 },             // Case 11: BL + BR + TL solid
    { 2, 3, 7,  2, 7, 5, -1 },                       // Case 12: TR + TL solid
    { 0, 6, 2,  6, 5, 2,  0, 2, 3, -1 },             // Case 13: BL + TR + TL solid
    { 1, 2, 3,  1, 3, 6,  6, 3, 7, -1 },             // Case 14: BR + TR + TL solid
    { 0, 1, 2,  0, 2, 3, -1 }                        // Case 15: Full quad (2 Triangles)
};

struct MaterialSpec {
    uint8_t priority = 0;
};

class MarchingSquares
{

    private:

        std::vector<MaterialSpec> g_MaterialTable;

        void PrecacheMaterials() {
            auto& j = JsonLoader::Instance().Get("materials.json");

            for (const auto& mat : j["materials"]) {
                uint16_t id = mat["id"];
                if (id >= g_MaterialTable.size()) {
                    g_MaterialTable.resize(id + 1);
                }
                g_MaterialTable[id].priority = mat["priority"];
            }
        }

        // Inside ResolvePrecedence during mesh generation (Zero overhead)

        uint16_t ResolvePrecedence(uint16_t t1, float d1, uint16_t t2, float d2);
        inline uint8_t GetPriority(uint16_t type_id) {
            return (type_id < g_MaterialTable.size()) ? g_MaterialTable[type_id].priority : 0;
        }

Vertex GetEdgeVertex(int edge, float x, float y, 
        float d1, float d2, uint16_t t1, uint16_t t2, float threshold);

Vertex GetCellVertex(int id, float x, float y, 
        float bl, float br, float tr, float tl,
        uint16_t bl_t, uint16_t br_t, uint16_t tr_t, uint16_t tl_t, 
        float threshold);

    public:
        MarchingSquares();
        ~MarchingSquares();

        MeshData GenerateMesh(const std::vector<Cell>& world, float threshold, int world_x, int world_y, int cx, int cy, int chunk_size);



};


#endif
