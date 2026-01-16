#pragma once

#include <cstdint>
#include <limits>

namespace framework {

// T is the Resource Type (e.g. Texture, Mesh)
template<typename T>
struct AssetHandle {
    using HandleType = uint32_t;
    
    HandleType id;

    // Default constructor: Invalid handle
    AssetHandle() : id(0) {}
    
    // Explicit constructor
    explicit AssetHandle(HandleType handleId) : id(handleId) {}

    bool IsValid() const {
        return id != 0;
    }

    static AssetHandle Invalid() {
        return AssetHandle(0);
    }

    bool operator==(const AssetHandle& other) const {
        return id == other.id;
    }
    
    bool operator!=(const AssetHandle& other) const {
        return id != other.id;
    }

    // Helper to extract Index and Generation
    // Assuming 20 bits index, 12 bits generation
    uint32_t GetIndex() const {
        return id & 0x000FFFFF;
    }

    uint32_t GetGeneration() const {
        return (id >> 20) & 0xFFF;
    }
};

} // namespace framework
