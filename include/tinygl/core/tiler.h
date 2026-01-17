#pragma once

#include <vector>
#include <tinygl/base/tmath.h>
#include <tinygl/core/gl_defs.h>
#include <tinygl/core/linear_allocator.h>

namespace tinygl {

// The baked data for a triangle, ready for rasterization
// Must be POD to be stored in LinearAllocator
struct TriangleData {
    Vec4 p[3];  // Screen space positions
    float varyings[3][MAX_VARYINGS]; // Interpolated data for 3 vertices
};

// A command in a tile's draw list
struct TileCommand {
    enum Type : uint8_t {
        DRAW_TRIANGLE,
        CLEAR
    };

    Type type;
    uint16_t pipelineId; // Reference to the Shader Pipeline
    uint32_t dataIndex;  // Offset/Index into the LinearAllocator triangle pool
};

// A screen tile (e.g. 64x64 pixels)
struct Tile {
    std::vector<TileCommand> commands;
    
    // Helper to clear commands for next frame
    void Reset() {
        commands.clear();
    }
};

class TINYGL_API TileBinningSystem {
public:
    void Init(int width, int height, int tileSize);
    void Reset();
    
    // Add a triangle to the relevant tiles
    // dataOffset is the byte offset or index in LinearAllocator where the triangle data is stored
    void BinTriangle(const TriangleData& tri, uint16_t pipelineId, uint32_t dataOffset);

    Tile& GetTile(int x, int y) {
        return m_tiles[y * m_gridWidth + x];
    }
    
    int GetGridWidth() const { return m_gridWidth; }
    int GetGridHeight() const { return m_gridHeight; }
    int GetTileSize() const { return m_tileSize; }

private:
    int m_width = 0;
    int m_height = 0;
    int m_tileSize = 64;
    int m_gridWidth = 0;
    int m_gridHeight = 0;
    
    std::vector<Tile> m_tiles;
};

} // namespace tinygl
