#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <tinygl/core/gl_defs.h> // For TINYGL_API

namespace tinygl {

class TINYGL_API LinearAllocator {
public:
    LinearAllocator() = default;
    ~LinearAllocator();

    // Prevent copying
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;

    void Init(size_t sizeInBytes);
    void* Allocate(size_t size);
    void Reset();
    size_t GetUsedMemory() const { return m_offset; }
    size_t GetTotalMemory() const { return m_totalSize; }

    template<typename T>
    T* New(size_t count = 1) {
        return static_cast<T*>(Allocate(sizeof(T) * count));
    }

private:
    uint8_t* m_basePtr = nullptr;
    size_t m_offset = 0;
    size_t m_totalSize = 0;
};

} // namespace tinygl
