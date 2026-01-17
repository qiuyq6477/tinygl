#pragma once

#include "asset_handle.h"

namespace framework {

// Forward Declaration
class AssetManager;

template<typename T>
class SharedAsset {
public:
    SharedAsset() : m_handle(AssetHandle<T>::Invalid()) {}
    
    explicit SharedAsset(AssetHandle<T> handle) : m_handle(handle) {}

    // Copy: AddRef
    SharedAsset(const SharedAsset& other) : m_handle(other.m_handle) {
        if (m_handle.IsValid()) {
            AddRefInternal();
        }
    }

    // Move
    SharedAsset(SharedAsset&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = AssetHandle<T>::Invalid();
    }

    // Assignment
    SharedAsset& operator=(const SharedAsset& other) {
        if (this != &other) {
            Release();
            m_handle = other.m_handle;
            if (m_handle.IsValid()) {
                AddRefInternal();
            }
        }
        return *this;
    }

    SharedAsset& operator=(SharedAsset&& other) noexcept {
        if (this != &other) {
            Release();
            m_handle = other.m_handle;
            other.m_handle = AssetHandle<T>::Invalid();
        }
        return *this;
    }

    ~SharedAsset() {
        Release();
    }

    void Release() {
        if (m_handle.IsValid()) {
            ReleaseInternal();
            m_handle = AssetHandle<T>::Invalid();
        }
    }

    AssetHandle<T> GetHandle() const { return m_handle; }
    
    bool IsValid() const { return m_handle.IsValid(); }
    
    operator AssetHandle<T>() const { return m_handle; }

    bool operator==(const SharedAsset& other) const { return m_handle == other.m_handle; }
    bool operator!=(const SharedAsset& other) const { return m_handle != other.m_handle; }

private:
    AssetHandle<T> m_handle;

    // Helper to allow breaking dependency cycle. 
    // Implemented in a way that requires AssetManager visibility only at instantiation.
    void AddRefInternal();
    void ReleaseInternal();
};

} // namespace framework