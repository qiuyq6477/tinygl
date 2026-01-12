#pragma once
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <array>
#include <tinygl/base/tmath.h>
#include <tinygl/core/gl_defs.h>

using namespace tinygl;

namespace framework {

struct TINYGL_API Geometry {
    std::vector<float> vertices;  // 4 components (x,y,z,w)
    std::vector<float> normals;   // 3 components
    std::vector<float> tangents;  // 3 components
    std::vector<float> bitangents; // 3 components
    std::vector<float> texCoords; // 2 components
    std::vector<float> allAttributes; // Interleaved data
    std::vector<uint32_t> indices;
    GLenum mode = GL_TRIANGLES;

    // Helper to add data
    void addVertex(float x, float y, float z, float w = 1.0f) {
        vertices.insert(vertices.end(), {x, y, z, w});
    }
    void addNormal(float x, float y, float z) {
        normals.insert(normals.end(), {x, y, z});
    }
    void addTangent(float x, float y, float z) {
        tangents.insert(tangents.end(), {x, y, z});
    }
    void addTexCoord(float u, float v) {
        texCoords.insert(texCoords.end(), {u, v});
    }
    void addIndices(std::initializer_list<uint32_t> idxs) {
        indices.insert(indices.end(), idxs);
    }
    
    // Finalize the geometry: compute bitangents and interleave attributes
    void finalize();
};


namespace geometry {

TINYGL_API Geometry createPlane(float horizontalExtend, float verticalExtend);
TINYGL_API Geometry createCube(float halfExtend);
TINYGL_API Geometry createSphere(float radius, uint32_t numberSlices);
TINYGL_API Geometry createTorus(float innerRadius, float outerRadius, uint32_t numberSlices, uint32_t numberStacks);
TINYGL_API Geometry createCylinder(float halfExtend, float radius, uint32_t numberSlices);

} // namespace tinygl
} // namespace geometry