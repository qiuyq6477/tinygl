#include <tinygl/core/linear_allocator.h>
#include <cstdlib>
#include <stdexcept>
#include <iostream>

namespace tinygl {

LinearAllocator::~LinearAllocator() {
    if (m_basePtr) {
        std::free(m_basePtr);
        m_basePtr = nullptr;
    }
}

void LinearAllocator::Init(size_t sizeInBytes) {
    if (m_basePtr) {
        std::free(m_basePtr);
    }
    m_basePtr = static_cast<uint8_t*>(std::malloc(sizeInBytes));
    if (!m_basePtr) {
        throw std::runtime_error("LinearAllocator: Failed to allocate memory");
    }
    m_totalSize = sizeInBytes;
    m_offset = 0;
}

void* LinearAllocator::Allocate(size_t size) {
    // 8-byte alignment
    size_t alignedSize = (size + 7) & ~7;
    
    if (m_offset + alignedSize > m_totalSize) {
        // Simple fallback: Print error and return nullptr or handle gracefully
        // For now, let's just log and maybe crash/throw in debug
        std::cerr << "LinearAllocator: Out of memory! Requested " << size << ", Available " << (m_totalSize - m_offset) << std::endl;
        return nullptr;
    }

    void* ptr = m_basePtr + m_offset;
    m_offset += alignedSize;
    return ptr;
}

void LinearAllocator::Reset() {
    m_offset = 0;
}

} // namespace tinygl
