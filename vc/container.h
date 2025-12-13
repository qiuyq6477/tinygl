#pragma once
#include <cstddef>

template<typename T, size_t Capacity>
struct StaticVector {
    T data[Capacity];
    size_t count = 0;

    void push_back(const T& val) {
        if (count < Capacity) {
            data[count++] = val;
        }
    }

    void clear() { count = 0; }
    bool empty() const { return count == 0; }
    size_t size() const { return count; }

    T& operator[](size_t i) { return data[i]; }
    const T& operator[](size_t i) const { return data[i]; }

    T* begin() { return data; }
    T* end() { return data + count; }
    const T* begin() const { return data; }
    const T* end() const { return data + count; }
    
    T& back() { return data[count - 1]; }
    const T& back() const { return data[count - 1]; }
};

