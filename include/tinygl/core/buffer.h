#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <iostream>
#include "../log.h"
#include "gl_defs.h"

// Buffer
struct BufferObject {
    std::vector<uint8_t> data;
    GLenum usage = GL_STATIC_DRAW; // Default usage
    
    template <typename T>
    bool readSafe(size_t offset, T& outValue) const {
        if (offset + sizeof(T) > data.size()) {
            static bool logged = false;
            if (!logged) {
                LOG_ERROR("Buffer Read Overflow! Offset: " + std::to_string(offset) + ", Size: " + std::to_string(data.size()));
                logged = true;
            }
            return false;
        }
        std::memcpy(&outValue, data.data() + offset, sizeof(T));
        return true;
    }
};