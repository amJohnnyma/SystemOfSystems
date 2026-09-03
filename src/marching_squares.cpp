#include "marching_squares.h"
#include <cstddef>
#include <cstdint>


//helpers
inline float GetInterpolationT(float d1, float d2, float threshold)
{
    if (std::abs(d2 - d1) < 0.00001f) return 0.5f;
    return (threshold - d1) / (d2 -d1);
}
uint16_t MarchingSquares::ResolvePrecedence(uint16_t t1, float d1, uint16_t t2, float d2)
{
    if(t1 == t2) return t1;
    if (t1 == 0 && t2 == 0) return 0;
    if (t1 == 0 && t2 != 0) return t2;
    if (t2 == 0 && t1 != 0) return t1;
    uint8_t p1 = GetPriority(t1);
    uint8_t p2 = GetPriority(t2);

    // 1. Higher priority takes complete precedence
    if (p1 > p2) return t1;
    if (p2 > p1) return t2;

    // 2. Equal priority: Fallback to highest density (proportional sharing)
    return (d1 >= d2) ? t1 : t2;
}
// Edge cuts: pick type_id of whichever corner along this edge has higher density
Vertex MarchingSquares::GetEdgeVertex(int edge, float x, float y, 
        float d1, float d2, uint16_t t1, uint16_t t2, float threshold)
{
    float t = GetInterpolationT(d1, d2, threshold);
    uint16_t edge_type = ResolvePrecedence(t1, d1, t2, d2);

    switch (edge)
    {
        case 0: return {Vec2{x + t, y + 1.0f}, Vec2{0.f, 0.f}, edge_type}; // Top: TL -> TR
        case 1: return {Vec2{x + 1.0f, y + t}, Vec2{0.f, 0.f}, edge_type}; // Right: BR -> TR
        case 2: return {Vec2{x + t, y + 0.0f}, Vec2{0.f, 0.f}, edge_type}; // Bottom: BL -> BR
        case 3: return {Vec2{x + 0.0f, y + t}, Vec2{0.f, 0.f}, edge_type}; // Left: BL -> TL
        default: return {Vec2{x, y}, Vec2{0.f, 0.f}, edge_type};
    }
}

Vertex MarchingSquares::GetCellVertex(int id, float x, float y, 
        float bl, float br, float tr, float tl,
        uint16_t bl_t, uint16_t br_t, uint16_t tr_t, uint16_t tl_t, 
        float threshold)
{
    switch (id)
    {
        // Solid Corners get their exact corresponding corner type
        case 0: return { Vec2{x + 0.0f, y + 0.0f}, Vec2{0.0f, 0.0f}, bl_t }; // BL
        case 1: return { Vec2{x + 1.0f, y + 0.0f}, Vec2{1.0f, 0.0f}, br_t }; // BR
        case 2: return { Vec2{x + 1.0f, y + 1.0f}, Vec2{1.0f, 1.0f}, tr_t }; // TR
        case 3: return { Vec2{x + 0.0f, y + 1.0f}, Vec2{0.0f, 1.0f}, tl_t }; // TL

                // Interpolated Edge Cuts pass the 2 corners forming that specific edge
        case 4: return GetEdgeVertex(0, x, y, tl, tr, tl_t, tr_t, threshold); // Top Edge
        case 5: return GetEdgeVertex(1, x, y, br, tr, br_t, tr_t, threshold); // Right Edge
        case 6: return GetEdgeVertex(2, x, y, bl, br, bl_t, br_t, threshold); // Bottom Edge
        case 7: return GetEdgeVertex(3, x, y, bl, tl, bl_t, tl_t, threshold); // Left Edge

        default: return { Vec2{x, y}, Vec2{0.0f, 0.0f}, 0 };
    }
}

//convert edges into actual 2D coords
MarchingSquares::MarchingSquares()
{
    PrecacheMaterials();

}
//Helpers for threads

MeshData MarchingSquares::GenerateMesh(const std::vector<Cell>& world, float threshold, int world_x, int world_y, int cx, int cy, int chunk_size)
{
    MeshData mesh;
    size_t estimated_max = static_cast<size_t>(chunk_size) * chunk_size * 2;
    mesh.vertices.reserve(estimated_max*3);
    mesh.indices.reserve(estimated_max*3);

    int start_x = cx * chunk_size;
    int start_y = cy * chunk_size;
    // Iterate up to chunk bounds, stopping at world limits - 1 for 2x2 sampling
    int end_x = std::min(start_x + chunk_size, world_x - 1);
    int end_y = std::min(start_y + chunk_size, world_y - 1);


    // Marching phase through "2D" grid
    // make parallel
    //How many threads
    
    for(int iy = start_y; iy < end_y; iy++)
    {
        for (int ix = start_x; ix < end_x; ix++)
        {
            size_t idx_bl = static_cast<size_t>(iy) * world_x + ix;
            size_t idx_br = static_cast<size_t>(iy) * world_x + ix + 1;
            size_t idx_tr = static_cast<size_t>(iy+1) * world_x + ix + 1;
            size_t idx_tl = static_cast<size_t>(iy+1) * world_x + ix;

            // if air set density to 0
            uint16_t bl_t = world[idx_bl].type_id;
            uint16_t br_t = world[idx_br].type_id;
            uint16_t tr_t = world[idx_tr].type_id;
            uint16_t tl_t = world[idx_tl].type_id;

            float bl = world[idx_bl].density;
            float br = world[idx_br].density;
            float tr = world[idx_tr].density;
            float tl = world[idx_tl].density;


            // each corner is either 1 (inside/solid) if >= threshold or 0 (outside/empty)
            // 4 bit case index
            int case_index = 0;
            if (bl >= threshold) case_index |= 1; 
            if (br >= threshold) case_index |= 2; 
            if (tr >= threshold) case_index |= 4; 
            if (tl >= threshold) case_index |= 8; 
            // early exit for inside/outside cases
            if (case_index == 0) continue;
            //read pairs of edges from edge table
            const auto& tris= TRIANGLE_TABLE[case_index]; 

            // MAIN THREAD ONLY
            for (int i =0; tris[i] != -1; i += 3)
            {

                Vertex v0 = GetCellVertex(tris[i], ix, iy, 
                        bl, br, tr, tl, bl_t, br_t, tr_t, tl_t, threshold);
                Vertex v1 = GetCellVertex(tris[i + 1], ix, iy, 
                        bl, br, tr, tl, bl_t, br_t, tr_t, tl_t, threshold);
                Vertex v2 = GetCellVertex(tris[i + 2], ix, iy, 
                        bl, br, tr, tl, bl_t, br_t, tr_t, tl_t, threshold);



                uint32_t base_index = static_cast<uint32_t>(mesh.vertices.size());

                // May need to swap winding order
                mesh.vertices.push_back(v0);
                mesh.vertices.push_back(v1);
                mesh.vertices.push_back(v2);
                //Maybe better? mesh.vertices.emplace_back(GetCellVertex(tris[i], ix, iy, bl, br, tr, tl, bl_t, br_t, tr_t, tl_t, threshold));

                mesh.indices.push_back(base_index);
                mesh.indices.push_back(base_index+1);
                mesh.indices.push_back(base_index+2);


            }
        }
    }
    //
    return mesh;
}


MarchingSquares::~MarchingSquares() {}
