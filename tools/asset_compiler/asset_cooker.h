#pragma once

#include <filesystem>
#include <string>

namespace tools {

class AssetCooker {
public:
    static bool CookTexture(const std::filesystem::path& source, const std::filesystem::path& dest);
    static bool CookModel(const std::filesystem::path& source, const std::filesystem::path& dest);
};

} // namespace tools
