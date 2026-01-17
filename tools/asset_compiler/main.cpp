#include "asset_cooker.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void print_usage() {
    std::cout << "Usage: tinygl_ac <input_file> <output_file>" << std::endl;
}

std::string to_lower(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    fs::path inputPath = argv[1];
    fs::path outputPath = argv[2];

    if (!fs::exists(inputPath)) {
        std::cerr << "Error: Input file does not exist: " << inputPath << std::endl;
        return 1;
    }

    std::string ext = to_lower(inputPath.extension().string());

    bool success = false;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") {
        std::cout << "Cooking Texture: " << inputPath << " -> " << outputPath << std::endl;
        success = tools::AssetCooker::CookTexture(inputPath, outputPath);
    } 
    else if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") {
        std::cout << "Cooking Model: " << inputPath << " -> " << outputPath << std::endl;
        success = tools::AssetCooker::CookModel(inputPath, outputPath);
    }
    else {
        std::cerr << "Error: Unsupported file extension: " << ext << std::endl;
        return 1;
    }

    if (success) {
        std::cout << "Success!" << std::endl;
        return 0;
    } else {
        std::cerr << "Compilation failed." << std::endl;
        return 1;
    }
}
