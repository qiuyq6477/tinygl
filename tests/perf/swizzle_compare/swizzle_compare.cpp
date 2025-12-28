#include <tinygl/tinygl.h>
#include <vector>
#include <chrono>
#include <iostream>
#include <random>
#include <iomanip>
#include <cmath>

using namespace tinygl;

// Simple Linear Texture implementation for comparison
struct LinearTexture {
    std::vector<uint32_t> data;
    int width;
    int height;

    LinearTexture(int w, int h) : width(w), height(h) {
        data.resize(w * h);
    }

    // Linear access: row by row
    inline uint32_t getTexel(int x, int y) const {
        // Clamp to edge for safety in test
        if (x < 0) x = 0; if (x >= width) x = width - 1;
        if (y < 0) y = 0; if (y >= height) y = height - 1;
        return data[y * width + x];
    }
};

// Helper to populate TextureObject manually
void setupSwizzledTexture(TextureObject& tex, int w, int h, const std::vector<uint32_t>& srcData) {
    tex.width = w;
    tex.height = h;
    
    int blocksX = (w + 3) / 4;
    int blocksY = (h + 3) / 4;
    size_t sizeNeeded = blocksX * blocksY * 16;
    
    tex.data.resize(sizeNeeded);
    tex.mipLevels.resize(1);
    tex.mipLevels[0] = {0, w, h};
    
    // Swizzle copy
    uint32_t* destBase = tex.data.data();
    for (int by = 0; by < blocksY; ++by) {
        for (int bx = 0; bx < blocksX; ++bx) {
            int blockIdx = by * blocksX + bx;
            uint32_t* blockPtr = destBase + blockIdx * 16;
            
            for (int ly = 0; ly < 4; ++ly) {
                for (int lx = 0; lx < 4; ++lx) {
                    int srcX = bx * 4 + lx;
                    int srcY = by * 4 + ly;
                    
                    if (srcX < w && srcY < h) {
                        blockPtr[ly * 4 + lx] = srcData[srcY * w + srcX];
                    } else {
                        blockPtr[ly * 4 + lx] = 0;
                    }
                }
            }
        }
    }
    
    // Set nearest filter for simple fetch
    tex.minFilter = GL_NEAREST;
    tex.magFilter = GL_NEAREST;
    tex.updateSampler();
}

int main() {
    const int SIZE = 4096; // Large texture to bust cache
    const int TRIALS = 5;
    
    std::cout << "[Swizzle vs Linear] Texture Size: " << SIZE << "x" << SIZE 
              << " (" << (SIZE*SIZE*4 / 1024 / 1024) << " MB)" << std::endl;

    // 1. Data Generation
    std::vector<uint32_t> rawData(SIZE * SIZE);
    std::mt19937 rng(12345);
    std::uniform_int_distribution<uint32_t> dist;
    for(auto& p : rawData) p = dist(rng);

    // 2. Setup Structures
    LinearTexture linearTex(SIZE, SIZE);
    linearTex.data = rawData;

    TextureObject swizzledTex;
    setupSwizzledTexture(swizzledTex, SIZE, SIZE, rawData);

    // Define Access Pattern: 16x16 Tiles (simulating rasterizer block processing)
    const int TILE_W = 16;
    const int TILE_H = 16;
    int tilesX = (SIZE + TILE_W - 1) / TILE_W;
    int tilesY = (SIZE + TILE_H - 1) / TILE_H;

    // Variables to prevent optimization
    volatile uint32_t sink = 0;
    uint32_t accum = 0;

    // --- Benchmark Linear ---
    double minTimeLinear = 1e9;
    for(int t=0; t<TRIALS; ++t) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Access in Tiles
        for(int ty=0; ty<tilesY; ++ty) {
            for(int tx=0; tx<tilesX; ++tx) {
                // Inside Tile
                for(int y=0; y<TILE_H; ++y) {
                    int py = ty * TILE_H + y;
                    if (py >= SIZE) continue;
                    
                    for(int x=0; x<TILE_W; ++x) {
                        int px = tx * TILE_W + x;
                        if (px >= SIZE) continue;
                        
                        accum += linearTex.getTexel(px, py);
                    }
                }
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> ms = end - start;
        if (ms.count() < minTimeLinear) minTimeLinear = ms.count();
    }
    sink = accum;

    // --- Benchmark Swizzled (Integer Fetch) ---
    // Helper lambda simulating `getTexelRaw` without float overhead for fair memory layout comparison
    auto getSwizzledTexel = [&](const TextureObject& obj, int x, int y) -> uint32_t {
        int bx = x / 4; int by = y / 4;
        int lx = x % 4; int ly = y % 4;
        int blocksW = (obj.width + 3) / 4;
        size_t idx = (by * blocksW + bx) * 16 + (ly * 4 + lx);
        return obj.data[idx];
    };

    accum = 0;
    double minTimeSwizzledInt = 1e9;
    
    for(int t=0; t<TRIALS; ++t) {
        auto start = std::chrono::high_resolution_clock::now();
        
        for(int ty=0; ty<tilesY; ++ty) {
            for(int tx=0; tx<tilesX; ++tx) {
                for(int y=0; y<TILE_H; ++y) {
                    int py = ty * TILE_H + y;
                    if (py >= SIZE) continue;
                    
                    for(int x=0; x<TILE_W; ++x) {
                        int px = tx * TILE_W + x;
                        if (px >= SIZE) continue;
                        accum += getSwizzledTexel(swizzledTex, px, py);
                    }
                }
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> ms = end - start;
        if (ms.count() < minTimeSwizzledInt) minTimeSwizzledInt = ms.count();
    }
    sink = accum;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Access Pattern: " << TILE_W << "x" << TILE_H << " Tiled Iteration" << std::endl;
    std::cout << "Linear Layout Time:   " << minTimeLinear << " ms" << std::endl;
    std::cout << "Swizzled Layout Time: " << minTimeSwizzledInt << " ms" << std::endl;

    double speedup = minTimeLinear / minTimeSwizzledInt;
    std::cout << "Speedup: " << speedup << "x" << std::endl;
    
    // Diagonal Access
    std::cout << "\n--- Diagonal Access Test ---" << std::endl;
    int DIAG_STEPS = SIZE * 1000; 
    
    // Linear Diagonal
    accum = 0;
    double minTimeDiagLin = 1e9;
    for(int t=0; t<TRIALS; ++t) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i=0; i<DIAG_STEPS; ++i) {
            int x = i % SIZE;
            int y = i % SIZE; 
            accum += linearTex.getTexel(x, y);
        }
        auto end = std::chrono::high_resolution_clock::now();
        if ((end-start).count() < minTimeDiagLin) minTimeDiagLin = std::chrono::duration<double, std::milli>(end-start).count();
    }
    sink = accum;
    std::cout << "Linear Diagonal:   " << minTimeDiagLin << " ms" << std::endl;

    // Swizzled Diagonal
    accum = 0;
    double minTimeDiagSwiz = 1e9;
    for(int t=0; t<TRIALS; ++t) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i=0; i<DIAG_STEPS; ++i) {
            int x = i % SIZE;
            int y = i % SIZE; 
            accum += getSwizzledTexel(swizzledTex, x, y);
        }
        auto end = std::chrono::high_resolution_clock::now();
        if ((end-start).count() < minTimeDiagSwiz) minTimeDiagSwiz = std::chrono::duration<double, std::milli>(end-start).count();
    }
    sink = accum;
    std::cout << "Swizzled Diagonal: " << minTimeDiagSwiz << " ms" << std::endl;

    return 0;
}