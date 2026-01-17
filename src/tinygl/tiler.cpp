#include <tinygl/core/tiler.h>
#include <algorithm>
#include <cmath>

namespace tinygl {

void TileBinningSystem::Init(int width, int height, int tileSize) {
    m_width = width;
    m_height = height;
    m_tileSize = tileSize;
    
    m_gridWidth = (width + tileSize - 1) / tileSize;
    m_gridHeight = (height + tileSize - 1) / tileSize;
    
    m_tiles.resize(m_gridWidth * m_gridHeight);
}

void TileBinningSystem::Reset() {
    for (auto& tile : m_tiles) {
        tile.Reset();
    }
}

void TileBinningSystem::BinTriangle(const TriangleData& tri, uint16_t pipelineId, uint32_t dataOffset) {
    // 1. Calculate Bounding Box of the triangle in screen space
    // Screen coords are usually in [0, width] x [0, height]
    
    float minX = std::min({tri.p[0].x, tri.p[1].x, tri.p[2].x});
    float minY = std::min({tri.p[0].y, tri.p[1].y, tri.p[2].y});
    float maxX = std::max({tri.p[0].x, tri.p[1].x, tri.p[2].x});
    float maxY = std::max({tri.p[0].y, tri.p[1].y, tri.p[2].y});

    // Clip against screen bounds
    int minTx = std::max(0, static_cast<int>(minX) / m_tileSize);
    int minTy = std::max(0, static_cast<int>(minY) / m_tileSize);
    int maxTx = std::min(m_gridWidth - 1, static_cast<int>(maxX) / m_tileSize);
    int maxTy = std::min(m_gridHeight - 1, static_cast<int>(maxY) / m_tileSize);

    // 2. Add command to all covered tiles
    TileCommand cmd;
    cmd.type = TileCommand::DRAW_TRIANGLE;
    cmd.pipelineId = pipelineId;
    cmd.dataIndex = dataOffset;

    for (int y = minTy; y <= maxTy; ++y) {
        for (int x = minTx; x <= maxTx; ++x) {
            // Note: simple bounding box binning is conservative.
            // A more advanced binner would check triangle-tile intersection.
            m_tiles[y * m_gridWidth + x].commands.push_back(cmd);
        }
    }
}

} // namespace tinygl
