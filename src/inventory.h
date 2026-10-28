#ifndef INVENTORY_H
#define INVENTORY_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <algorithm>

struct ItemStack
{
    // References an ItemDefinition in ItemRegistry.
    // ID 0 is reserved for empty slots.
    uint32_t item_id = 0;

    // Number of identical items in this slot.
    uint32_t count = 0;

    // Unique identity when necessary, e.g. a specific sword.
    // 0 means no unique identity is assigned.
    uint64_t instance_uid = 0;

    // Remaining durability, for nonstackable equipment.
    uint32_t durability = 0;

    bool empty() const
    {
        return item_id == 0 || count == 0;
    }
};

struct Inventory
{
    // All slots live in ONE vector.
    // Slots 0-8 are the hotbar by default.
    std::vector<ItemStack> slots;

    // Number of slots reserved for the hotbar.
    size_t hotbar_size = 9;

    // Currently selected hotbar position.
    size_t selected_hotbar_slot = 0;

    explicit Inventory(
        size_t slot_count = 54,
        size_t hotbar_count = 9
    )
        : slots(slot_count),
          hotbar_size(std::min(slot_count, hotbar_count))
    {
    }

    const ItemStack* selectedItem() const
    {
        if (selected_hotbar_slot >= hotbar_size)
            return nullptr;

        return &slots[selected_hotbar_slot];
    }

    size_t size() const
    {
        return slots.size();
    }
};

#endif
