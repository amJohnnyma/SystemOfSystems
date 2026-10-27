#include "marching_squares.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_set>


//helpers
inline float GetInterpolationT(float d1, float d2, float threshold)
{
    if (std::abs(d2 - d1) < 0.00001f) return 0.5f;
    return (threshold - d1) / (d2 -d1);
}
// Edge cuts: pick type_id of whichever corner along this edge has higher density
Vertex MarchingSquares::GetEdgeVertex(int edge, float x, float y, 
        float d1, float d2, uint16_t t1, uint16_t t2, float threshold)
{
    float t = GetInterpolationT(d1, d2, threshold);
   // uint16_t edge_type = ResolvePrecedence(t1, d1, t2, d2);
   if (t1 == t2)
   {
        switch (edge) {
            case 0: return {Vec2{x + t, y + 1.0f}, Vec2{0.f, 0.f}, t1, t1, Vec2(1.0f, 0.0f)};
            case 1: return {Vec2{x + 1.0f, y + t}, Vec2{0.f, 0.f}, t1, t1, Vec2(1.0f, 0.0f)};
            case 2: return {Vec2{x + t, y + 0.0f}, Vec2{0.f, 0.f}, t1, t1, Vec2(1.0f, 0.0f)};
            case 3: return {Vec2{x + 0.0f, y + t}, Vec2{0.f, 0.f}, t1, t1, Vec2(1.0f, 0.0f)};
            default: return {Vec2{x, y}, Vec2{0.f, 0.f}, t1, t1, Vec2(1.0f, 0.0f)};
        }

   }
    Vec2 weights(1.0f - t, t);

    switch (edge)
    {
        case 0: return {Vec2{x + t, y + 1.0f}, Vec2{0.f, 0.f}, t1, t2, weights}; // Top: TL -> TR
        case 1: return {Vec2{x + 1.0f, y + t}, Vec2{0.f, 0.f}, t1, t2, weights}; // Right: BR -> TR
        case 2: return {Vec2{x + t, y + 0.0f}, Vec2{0.f, 0.f}, t1, t2, weights}; // Bottom: BL -> BR
        case 3: return {Vec2{x + 0.0f, y + t}, Vec2{0.f, 0.f}, t1, t2, weights}; // Left: BL -> TL
        default: return {Vec2{x, y}, Vec2{0.f, 0.f}, t1, t2, weights};
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

}
//Helpers for threads

/*
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

                mesh.vertices.push_back(v0);
                mesh.vertices.push_back(v1);
                mesh.vertices.push_back(v2);

                mesh.indices.push_back(base_index);
                mesh.indices.push_back(base_index+1);
                mesh.indices.push_back(base_index+2);


            }
        }
    }
    //
    return mesh;
}
*/

//density of surrounding
// arr[0] = TL 
// arr[1] = TM 
// arr[2] = TR 
// arr[3] = ML 
// arr[4] = M // the cell called from 
// arr[5] = MR 
// arr[6] = BL 
// arr[7] = BM 
// arr[8] = BR 
std::array<float, 9> MarchingSquares::getNeighbourhood(const std::vector<Cell>& world, int world_x, int world_y, int ix, int iy)
{
    // i suppose this couldjust store orthogonals...
    int indexCount = 0;
    std::array<float , 9> arr = {};
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            int nx = ix + dx;
            int ny = iy + dy;

            if (nx < 0 || nx >= world_x || ny < 0 || ny >= world_y) {
                arr[indexCount] = 0.0f;
                
            } else {
                size_t idx = static_cast<size_t>(ny) * world_x + nx;
                arr[indexCount] = world[idx].density; 
            }
            indexCount ++;

        }
    }

    return arr;
}

// 0 = empty
// 1 = full
// 2 = slope_bl
// 3 = slop_br
// 4 = slope_tl
// 5 = slope_tr
// 6 = half_b
// 7 = half_t
// 8 = half_l
// 9 = half_r
// 10 = half_centered_vertical
// 11 = half_centered_horizontal
// 12 = half_block_full // The full block but half size -> "Floats", maybe a star shape could look nicer. we'll see
int MarchingSquares::getBlockSlopeType(const std::array<float, 9>& nb, float threshold)
{
    float center_density = nb[4]; // Center is index 4 in a 3x3 (-1 to 1)

    // 1. Air / Empty rule
    if (center_density <= 0.0f) {
        return 0;
    }

    // 3. Within diagonal/slope range (e.g., 1 to 128)
    // Extract neighbor solidity (boolean flags based on threshold)
    // Each of these are "solid"
    bool top    = nb[1] >= threshold;
    bool bottom = nb[7] >= threshold;
    bool left   = nb[3] >= threshold;
    bool right  = nb[5] >= threshold;
    // 2. Full block rule (above upper density threshold)
    if (center_density >= threshold) { 

        return 1;
    }


    // Apply directional neighbor rules
    // The center_density is NOT FULL
    if (top && bottom && left && right) return 12;
    if (top && left && right) return 6;
    if (bottom && left && right) return 7;
    if (top && left && bottom) return 8;
    if (top && right && bottom) return 9;
    if (top && left) return 2;
    if (top && right) return 3;
    if (bottom && left) return 4;
    if (bottom && right) return 5;
    if (top && bottom) return 10;
    if (left && right) return 11; 


    return 0;

}
void AppendShapeGeometry(int shape_id, int ix, int iy, uint16_t material_id, MeshData& mesh)
{
    if (shape_id == 0) return; // Empty, do nothing

    float fx = static_cast<float>(ix);
    float fy = static_cast<float>(iy);
    uint32_t base_index = static_cast<uint32_t>(mesh.vertices.size());

    // Helper lambda to push a vertex with world offset and material mapping
auto add_v = [&](float x, float y, float u, float v) {
    Vertex vert;
    vert.position = Vec2{fx + x, fy + y};
    vert.uv = Vec2{u, v};
    vert.type_id = material_id;
    vert.other_type_id = material_id; // Or 0 if no secondary material
    vert.weights = Vec2(1.0f, 0.0f);   // 100% weight to primary material
    mesh.vertices.push_back(vert);
};

    switch (shape_id)
    {
        case 1: // Full block (Quad: 4 vertices, 6 indices)
            add_v(0.0f, 0.0f, 0.0f, 0.0f); // BL
            add_v(1.0f, 0.0f, 1.0f, 0.0f); // BR
            add_v(1.0f, 1.0f, 1.0f, 1.0f); // TR
            add_v(0.0f, 1.0f, 0.0f, 1.0f); // TL
            
            mesh.indices.insert(mesh.indices.end(), {
                base_index, base_index + 1, base_index + 2,
                base_index, base_index + 2, base_index + 3
            });
            break;

        case 2: // slope_bl (Triangle: BL, BR, TL)
            add_v(0.0f, 0.0f, 0.0f, 0.0f);
            add_v(1.0f, 0.0f, 1.0f, 0.0f);
            add_v(0.0f, 1.0f, 0.0f, 1.0f);

            mesh.indices.insert(mesh.indices.end(), { base_index, base_index + 1, base_index + 2 });
            break;

        case 3: // slope_br (Triangle: BL, BR, TR)
            add_v(0.0f, 0.0f, 0.0f, 0.0f);
            add_v(1.0f, 0.0f, 1.0f, 0.0f);
            add_v(1.0f, 1.0f, 1.0f, 1.0f);

            mesh.indices.insert(mesh.indices.end(), { base_index, base_index + 1, base_index + 2 });
            break;

        case 4: // slope_tl (Triangle: BL, TR, TL)
            add_v(0.0f, 0.0f, 0.0f, 0.0f);
            add_v(1.0f, 1.0f, 1.0f, 1.0f);
            add_v(0.0f, 1.0f, 0.0f, 1.0f);

            mesh.indices.insert(mesh.indices.end(), { base_index, base_index + 1, base_index + 2 });
            break;

        case 5: // slope_tr (Triangle: BR, TR, TL)
            add_v(1.0f, 0.0f, 1.0f, 0.0f);
            add_v(1.0f, 1.0f, 1.0f, 1.0f);
            add_v(0.0f, 1.0f, 0.0f, 1.0f);

            mesh.indices.insert(mesh.indices.end(), { base_index, base_index + 1, base_index + 2 });
            break;

        case 6: // half_b (Bottom half slab)
            add_v(0.0f, 0.0f, 0.0f, 0.0f);
            add_v(1.0f, 0.0f, 1.0f, 0.0f);
            add_v(1.0f, 0.5f, 1.0f, 0.5f);
            add_v(0.0f, 0.5f, 0.0f, 0.5f);

            mesh.indices.insert(mesh.indices.end(), {
                base_index, base_index + 1, base_index + 2,
                base_index, base_index + 2, base_index + 3
            });
            break;

        case 7: // half_t (Top half slab)
            add_v(0.0f, 0.5f, 0.0f, 0.5f);
            add_v(1.0f, 0.5f, 1.0f, 0.5f);
            add_v(1.0f, 1.0f, 1.0f, 1.0f);
            add_v(0.0f, 1.0f, 0.0f, 1.0f);

            mesh.indices.insert(mesh.indices.end(), {
                base_index, base_index + 1, base_index + 2,
                base_index, base_index + 2, base_index + 3
            });
            break;

        case 8: // half_l (Left half strip)
            add_v(0.0f, 0.0f, 0.0f, 0.0f);
            add_v(0.5f, 0.0f, 0.5f, 0.0f);
            add_v(0.5f, 1.0f, 0.5f, 1.0f);
            add_v(0.0f, 1.0f, 0.0f, 1.0f);

            mesh.indices.insert(mesh.indices.end(), {
                base_index, base_index + 1, base_index + 2,
                base_index, base_index + 2, base_index + 3
            });
            break;

        case 9: // half_r (Right half strip)
            add_v(0.5f, 0.0f, 0.5f, 0.0f);
            add_v(1.0f, 0.0f, 1.0f, 0.0f);
            add_v(1.0f, 1.0f, 1.0f, 1.0f);
            add_v(0.5f, 1.0f, 0.5f, 1.0f);

            mesh.indices.insert(mesh.indices.end(), {
                base_index, base_index + 1, base_index + 2,
                base_index, base_index + 2, base_index + 3
            });
            break;

        case 10: // half_centered_vertical
            add_v(0.25f, 0.0f, 0.25f, 0.0f);
            add_v(0.75f, 0.0f, 0.75f, 0.0f);
            add_v(0.75f, 1.0f, 0.75f, 1.0f);
            add_v(0.25f, 1.0f, 0.25f, 1.0f);

            mesh.indices.insert(mesh.indices.end(), {
                base_index, base_index + 1, base_index + 2,
                base_index, base_index + 2, base_index + 3
            });
            break;

        case 11: // half_centered_horizontal
            add_v(0.0f, 0.25f, 0.0f, 0.25f);
            add_v(1.0f, 0.25f, 1.0f, 0.25f);
            add_v(1.0f, 0.75f, 1.0f, 0.75f);
            add_v(0.0f, 0.75f, 0.0f, 0.75f);

            mesh.indices.insert(mesh.indices.end(), {
                base_index, base_index + 1, base_index + 2,
                base_index, base_index + 2, base_index + 3
            });
            break;

        case 12: // half_block_full (Shrunk centered square / placeholder)
            add_v(0.25f, 0.25f, 0.25f, 0.25f);
            add_v(0.75f, 0.25f, 0.75f, 0.25f);
            add_v(0.75f, 0.75f, 0.75f, 0.75f);
            add_v(0.25f, 0.75f, 0.25f, 0.75f);

            mesh.indices.insert(mesh.indices.end(), {
                base_index, base_index + 1, base_index + 2,
                base_index, base_index + 2, base_index + 3
            });
            break;

        default:
            break;
    }
}
MeshData MarchingSquares::GenerateMesh(const std::vector<Cell>& world, float threshold, int world_x, int world_y, int cx, int cy, int chunk_size)
{
    MeshData mesh;
    size_t estimated_max = static_cast<size_t>(chunk_size) * chunk_size * 2;
    mesh.vertices.reserve(estimated_max*3);
    mesh.indices.reserve(estimated_max*3);

   
    int start_x = cx * chunk_size;
    int start_y = cy * chunk_size;
    // Iterate up to chunk bounds, stopping at world limits - 1 for 3x3 sampling
    int end_x = std::min(start_x + chunk_size, world_x - 1);
    int end_y = std::min(start_y + chunk_size, world_y - 1);


    
    for(int iy = start_y; iy < end_y; iy++)
    {
        for (int ix = start_x; ix < end_x; ix++)
        {
            size_t center_idx = static_cast<size_t>(iy) * world_x + ix;
            if (world[center_idx].density <= 0.0f) continue;

            std::array<float, 9> arr = getNeighbourhood(world, world_x, world_y, ix, iy);
            int shape_id = getBlockSlopeType(arr, threshold);

            if (shape_id == 0) continue;

            uint16_t material_id = world[center_idx].type_id;

            // Append geometry directly into the mesh by reference
            AppendShapeGeometry(shape_id, ix, iy, material_id, mesh);

        }
    }
    //
    return mesh;
}

MeshData MarchingSquares::generate_blocky_mesh(const std::vector<Cell>& world, int world_x, int world_y, int cx, int cy, int chunk_size,const std::unordered_map<uint32_t, Structure>& active_structures) {

    MeshData mesh;
    int start_x = cx * chunk_size;
    int start_y = cy * chunk_size;
    int end_x = std::min(start_x + chunk_size, world_x);
    int end_y = std::min(start_y + chunk_size, world_y);

    std::unordered_set<uint32_t> processed_uids;

    
    for (int iy = start_y; iy < end_y; iy++) {
        for (int ix = start_x; ix < end_x; ix++) {
            size_t idx = static_cast<size_t>(iy) * world_x + ix;
            const Cell& cell = world[idx];

            if (cell.type_id == 0) continue; // Skip air

                if (cell.structure_on_cell_uID != -1) {
                uint32_t uid = static_cast<uint32_t>(cell.structure_on_cell_uID);
                if (processed_uids.count(uid)) {
                    continue; // Already processed this building in this chunk scan
                }
                processed_uids.insert(uid);

                auto it = active_structures.find(uid);
                if (it == active_structures.end()) continue;

                const auto& st = it->second.getTemplate();

                // Origin Chunk Rule: Ensure only the chunk owning the origin renders the full quad
                int owner_cx = st.x_origin / chunk_size;
                int owner_cy = st.y_origin / chunk_size;

                if (owner_cx != cx || owner_cy != cy) {
                    continue; // Skip: The owner chunk will draw the entire building quad
                }

                // Compute exact bounding box using the map origin and template width/height
                float x = static_cast<float>(st.x_origin);
                float y = static_cast<float>(st.y_origin - st.y_width + 1);
                float w = static_cast<float>(st.x_width);
                float h = static_cast<float>(st.y_width);

                Vertex v0 = {Vec2{x, y}, Vec2{0.0f, 0.0f}, static_cast<uint16_t>(st.structureID)};             // BL
                Vertex v1 = {Vec2{x + w, y}, Vec2{1.0f, 0.0f}, static_cast<uint16_t>(st.structureID)};         // BR
                Vertex v2 = {Vec2{x + w, y + h}, Vec2{1.0f, 1.0f}, static_cast<uint16_t>(st.structureID)};     // TR
                Vertex v3 = {Vec2{x, y + h}, Vec2{0.0f, 1.0f}, static_cast<uint16_t>(st.structureID)};         // TL

                uint32_t base_index = static_cast<uint32_t>(mesh.vertices.size());
                
                mesh.vertices.push_back(v0);
                mesh.vertices.push_back(v1);
                mesh.vertices.push_back(v2);
                
                mesh.vertices.push_back(v0);
                mesh.vertices.push_back(v2);
                mesh.vertices.push_back(v3);

                mesh.indices.push_back(base_index);
                mesh.indices.push_back(base_index + 1);
                mesh.indices.push_back(base_index + 2);

                mesh.indices.push_back(base_index + 3);
                mesh.indices.push_back(base_index + 4);
                mesh.indices.push_back(base_index + 5);

                continue;
            }
            // Define a full 1x1 quad for this cell
            float x = static_cast<float>(ix);
            float y = static_cast<float>(iy);


            Vertex v0 = {Vec2{x, y}, Vec2{0,0}, cell.type_id};         // BL
            Vertex v1 = {Vec2{x + 1.0f, y}, Vec2{1,0}, cell.type_id};     // BR
            Vertex v2 = {Vec2{x + 1.0f, y + 1.0f}, Vec2{1,1}, cell.type_id}; // TR
            Vertex v3 = {Vec2{x, y + 1.0f}, Vec2{0,1}, cell.type_id};     // TL

            uint32_t base_index = static_cast<uint32_t>(mesh.vertices.size());
            
            // Push two triangles to form a full square
            mesh.vertices.push_back(v0);
            mesh.vertices.push_back(v1);
            mesh.vertices.push_back(v2);
            
            mesh.vertices.push_back(v0);
            mesh.vertices.push_back(v2);
            mesh.vertices.push_back(v3);

// Add corresponding indices for the 6 pushed vertices
            mesh.indices.push_back(base_index);     // Triangle 1: v0
            mesh.indices.push_back(base_index + 1); // Triangle 1: v1
            mesh.indices.push_back(base_index + 2); // Triangle 1: v2

            mesh.indices.push_back(base_index + 3); // Triangle 2: v0
            mesh.indices.push_back(base_index + 4); // Triangle 2: v2
            mesh.indices.push_back(base_index + 5); // Triangle 2: v3

        }
    }
    return mesh;
}

MarchingSquares::~MarchingSquares() {}
