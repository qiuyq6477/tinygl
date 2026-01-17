#pragma once

#include "shared_asset.h"
#include <framework/asset_manager.h>

namespace framework {

template<typename T>
inline void SharedAsset<T>::AddRefInternal() {
    AssetManager::Get().AddRef(m_handle);
}

template<typename T>
inline void SharedAsset<T>::ReleaseInternal() {
    AssetManager::Get().Release(m_handle);
}

} // namespace framework
