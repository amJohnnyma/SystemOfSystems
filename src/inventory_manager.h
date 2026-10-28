#ifndef INVENTORY_MANAGER_H
#define INVENTORY_MANAGER_H

#include "inventory.h"
#include "item.h"

class InventoryManager
{
public:
    // Add items to existing compatible stacks first,
    // then to empty slots. Returns the count not inserted.
    static uint32_t add(
        Inventory& inventory,
        const ItemRegistry& registry,
        uint32_t item_id,
        uint32_t count
    );

    // Remove items by ID across multiple slots.
    // Returns the number actually removed.
    static uint32_t remove(
        Inventory& inventory,
        uint32_t item_id,
        uint32_t count
    );

    // Move items between slots, merging compatible
    // stacks or swapping when appropriate.
    static bool move(
        Inventory& inventory,
        const ItemRegistry& registry,
        size_t from,
        size_t to
    );

    // Split a specified quantity into an empty slot.
    static bool split(
        Inventory& inventory,
        const ItemRegistry& registry,
        size_t from,
        size_t to,
        uint32_t count
    );

    // Consume a quantity from an individual slot.
    // Used by building, potions, ammunition, etc.
    static bool consumeSlot(
        Inventory& inventory,
        size_t slot,
        uint32_t count
    );
};
#endif
