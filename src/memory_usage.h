#ifndef MEMORY_USAGE_H
#define MEMORY_USAGE_H

#include "world_types.h"
#include "structure.h"

#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>

namespace MemoryUsage
{
    // Estimate heap storage for a std::string.
    // Assumes a common 64-bit STL with a 15-character
    // small-string optimization.
    inline std::size_t heap(const std::string& s)
    {
        return s.capacity() > 15
            ? s.capacity() + 1
            : 0;
    }

    // Only dynamically allocated vector storage.
    // The vector object itself is NOT counted.
    template<typename T>
    std::size_t vectorHeap(const std::vector<T>& v)
    {
        return v.capacity() * sizeof(T);
    }

    // Map bucket storage and node storage.
    // Node overhead varies between STL implementations.
    template<typename K, typename V, typename Extra>
    std::size_t mapHeap(
        const std::unordered_map<K, V>& map,
        Extra extra)
    {
        using Map = std::unordered_map<K, V>;

        constexpr std::size_t nodeOverhead =
            sizeof(void*) * 2;

        std::size_t bytes =
            map.bucket_count() * sizeof(void*);

        bytes += map.size() *
            (sizeof(typename Map::value_type)
             + nodeOverhead);

        for (const auto& [key, value] : map)
        {
            bytes += extra(key, value);
        }

        return bytes;
    }

    // ------------------------------------------------
    // Cell
    // ------------------------------------------------
    inline std::size_t dynamic(const Cell& cell)
    {
        return vectorHeap(cell.entities_on_cell_uID);
    }

    // ------------------------------------------------
    // Mesh
    // ------------------------------------------------
    inline std::size_t dynamic(const MeshData& mesh)
    {
        return vectorHeap(mesh.vertices)
             + vectorHeap(mesh.indices);
    }

    inline std::size_t dynamic(const ChunkMeshData& chunk)
    {
        return dynamic(chunk.terrain_mesh)
             + dynamic(chunk.structure_mesh);
    }

    // ------------------------------------------------
    // Attack
    // ------------------------------------------------
    inline std::size_t dynamic(const attack& a)
    {
        return heap(a.name);
    }

    inline std::size_t dynamic(const attack_pattern& pattern)
    {
        std::size_t bytes = heap(pattern.name);

        bytes += vectorHeap(pattern.attacks);

        for (const auto& a : pattern.attacks)
        {
            bytes += dynamic(a);
        }

        return bytes;
    }

    // ------------------------------------------------
    // Entity animations
    // ------------------------------------------------
    inline std::size_t dynamic(const EntityAnimation& a)
    {
        return heap(a.sprite);
    }

    // ------------------------------------------------
    // Entity template
    // ------------------------------------------------
    inline std::size_t dynamic(const entity_template& et)
    {
        std::size_t bytes = 0;

        bytes += heap(et.name);
        bytes += heap(et.sprite);
        bytes += heap(et.entity_class);
        bytes += heap(et.behavior_type);

        // Animation map
        bytes += mapHeap(
            et.animations,
            [](const std::string& key,
               const EntityAnimation& anim)
            {
                return heap(key) + dynamic(anim);
            }
        );

        // Attack patterns
        bytes += vectorHeap(et.attack_patterns);

        for (const auto& pattern : et.attack_patterns)
        {
            bytes += dynamic(pattern);
        }

        return bytes;
    }

    // ------------------------------------------------
    // Player sprite settings
    // ------------------------------------------------
    inline std::size_t dynamic(
        const PlayerSpriteSettings& sprite)
    {
        std::size_t bytes = heap(sprite.filename);

        // Per-frame render overrides
        bytes += mapHeap(
            sprite.frameRenderOverrides,
            [](int, const FrameRenderData&)
            {
                return std::size_t{0};
            }
        );

        // Per-frame attack hitboxes
        bytes += mapHeap(
            sprite.frameHitboxes,
            [](int, const std::vector<AttackHitboxData>& v)
            {
                return vectorHeap(v);
            }
        );

        return bytes;
    }

    // ------------------------------------------------
    // Player settings
    // ------------------------------------------------
    inline std::size_t dynamic(const PlayerSettings& ps)
    {
        std::size_t bytes = heap(ps.name);

        bytes += mapHeap(
            ps.sprites,
            [](const std::string& state,
               const PlayerSpriteSettings& sprite)
            {
                return heap(state) + dynamic(sprite);
            }
        );

        return bytes;
    }
    inline std::size_t dynamic(const struct_template& st)
    {
        return heap(st.sprite)
            + vectorHeap(st.resource_input)
            + vectorHeap(st.resource_output);
    }
}

#endif
