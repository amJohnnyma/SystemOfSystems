#include "structure.h"
#include "memory_usage.h"

std::size_t Structure::getMemoryUsage() const
{
    return sizeof(*this)
         + MemoryUsage::dynamic(struct_);
}
