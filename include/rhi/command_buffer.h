#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <rhi/commands.h>

namespace rhi {

class CommandBuffer {
public:
    CommandBuffer(size_t initialCapacity = 4096) {
        m_buffer.reserve(initialCapacity);
    }

    void Reset() {
        m_buffer.clear();
    }

    const uint8_t* GetData() const {
        return m_buffer.data();
    }

    size_t GetSize() const {
        return m_buffer.size();
    }

    bool IsEmpty() const {
        return m_buffer.empty();
    }

    // Generic write for POD packets
    template <typename T>
    void Write(const T& packet) {
        static_assert(std::is_base_of<CommandPacket, T>::value, "T must derive from CommandPacket");
        size_t offset = m_buffer.size();
        size_t size = sizeof(T);
        m_buffer.resize(offset + size);
        std::memcpy(m_buffer.data() + offset, &packet, size);
    }

    // Special write for variable sized packets (like UpdateUniform)
    void WriteUniform(uint8_t slot, const void* data, size_t size) {
        size_t headerSize = sizeof(PacketUpdateUniform);
        size_t totalSize = headerSize + size;
        
        // Align total size to 4 bytes to ensure next packet starts aligned
        size_t alignedSize = (totalSize + 3) & ~3;
        size_t padding = alignedSize - totalSize;

        size_t offset = m_buffer.size();
        m_buffer.resize(offset + alignedSize);
        
        // Write Header
        auto* header = reinterpret_cast<PacketUpdateUniform*>(m_buffer.data() + offset);
        header->type = CommandType::UpdateUniform;
        header->size = static_cast<uint16_t>(alignedSize);
        header->slot = slot;
        
        // Write Data
        std::memcpy(m_buffer.data() + offset + headerSize, data, size);
        
        // Zero out padding (optional but good for debugging)
        if (padding > 0) {
            std::memset(m_buffer.data() + offset + totalSize, 0, padding);
        }
    }

private:
    std::vector<uint8_t> m_buffer;
};

}