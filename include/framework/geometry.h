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

struct Geometry {
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
    void finalize() {
        size_t vertexCount = vertices.size() / 4;
        
        // Ensure bitangents exist
        if (bitangents.size() != vertexCount * 3) {
            bitangents.resize(vertexCount * 3);
            for (size_t i = 0; i < vertexCount; ++i) {
                // Calculate bitangent = cross(normal, tangent)
                // Assuming normal and tangent are valid.
                Vec4 n(0,0,0,0);
                if (i * 3 + 2 < normals.size()) {
                    n = Vec4(normals[i*3+0], normals[i*3+1], normals[i*3+2], 0.0f);
                } else {
                    n = Vec4(0, 0, 1, 0); // Default Normal
                }
                
                Vec4 t(1,0,0,0);
                if (i * 3 + 2 < tangents.size()) {
                    t = Vec4(tangents[i*3+0], tangents[i*3+1], tangents[i*3+2], 0.0f);
                }
                
                Vec4 b = cross(n, t); // Using tinygl::cross
                bitangents[i*3+0] = b.x;
                bitangents[i*3+1] = b.y;
                bitangents[i*3+2] = b.z;
            }
        }
        
        // Interleave: Pos(4) + Norm(3) + Tan(3) + Bitan(3) + UV(2) = 15 floats
        size_t stride = 4 + 3 + 3 + 3 + 2;
        allAttributes.resize(vertexCount * stride);
        
        for (size_t i = 0; i < vertexCount; ++i) {
            size_t offset = i * stride;
            // Pos
            allAttributes[offset + 0] = vertices[i*4 + 0];
            allAttributes[offset + 1] = vertices[i*4 + 1];
            allAttributes[offset + 2] = vertices[i*4 + 2];
            allAttributes[offset + 3] = vertices[i*4 + 3];
            
            // Normal
            if (i * 3 + 2 < normals.size()) {
                allAttributes[offset + 4] = normals[i*3 + 0];
                allAttributes[offset + 5] = normals[i*3 + 1];
                allAttributes[offset + 6] = normals[i*3 + 2];
            } else {
                allAttributes[offset + 4] = 0.0f; allAttributes[offset + 5] = 0.0f; allAttributes[offset + 6] = 1.0f;
            }
            
            // Tangent
            if (i * 3 + 2 < tangents.size()) {
                allAttributes[offset + 7] = tangents[i*3 + 0];
                allAttributes[offset + 8] = tangents[i*3 + 1];
                allAttributes[offset + 9] = tangents[i*3 + 2];
            } else {
                allAttributes[offset + 7] = 1.0f; allAttributes[offset + 8] = 0.0f; allAttributes[offset + 9] = 0.0f;
            }

            // Bitangent
            allAttributes[offset + 10] = bitangents[i*3 + 0];
            allAttributes[offset + 11] = bitangents[i*3 + 1];
            allAttributes[offset + 12] = bitangents[i*3 + 2];
            
            // UV
            if (i * 2 + 1 < texCoords.size()) {
                allAttributes[offset + 13] = texCoords[i*2 + 0];
                allAttributes[offset + 14] = texCoords[i*2 + 1];
            } else {
                 allAttributes[offset + 13] = 0.0f;
                 allAttributes[offset + 14] = 0.0f;
            }
        }
    }
};

namespace geometry {

inline Geometry createPlane(float horizontalExtend, float verticalExtend) {
    Geometry geo;
    geo.mode = GL_TRIANGLES;

    // 顶点位置：直接在初始化列表中完成缩放计算
    geo.vertices = {
        -horizontalExtend, -verticalExtend, 0.0f, 1.0f,
         horizontalExtend, -verticalExtend, 0.0f, 1.0f,
        -horizontalExtend,  verticalExtend, 0.0f, 1.0f,
         horizontalExtend,  verticalExtend, 0.0f, 1.0f
    };

    // 法线：所有顶点相同
    geo.normals.assign(4 * 3, 0.0f); // 先填充0
    for(int i=2; i<12; i+=3) geo.normals[i] = 1.0f; // 仅设置Z轴为1

    // 切线：所有顶点相同 (1, 0, 0)
    geo.tangents.assign(4 * 3, 0.0f);
    for(int i=0; i<12; i+=3) geo.tangents[i] = 1.0f;

    // 纹理坐标
    geo.texCoords = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };

    // 索引
    geo.indices = { 0, 1, 2, 1, 3, 2 };

    geo.finalize();
    return geo;
}

inline Geometry createCube(float halfExtend) {
    Geometry geo;
    geo.mode = GL_TRIANGLES;

    // 立方体 6 个面的法线方向
    float normals[6][3] = {
        {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}
    };
    // 对应的切线方向
    float tangents[6][3] = {
        {1,0,0}, {1,0,0}, {-1,0,0}, {1,0,0}, {0,0,1}, {0,0,-1}
    };

    for (int face = 0; face < 6; ++face) {
        float n[3] = {normals[face][0], normals[face][1], normals[face][2]};
        float t[3] = {tangents[face][0], tangents[face][1], tangents[face][2]};
        // 通过叉积计算副法线，确定平面的两个轴
        float b[3] = {n[1]*t[2]-n[2]*t[1], n[2]*t[0]-n[0]*t[2], n[0]*t[1]-n[1]*t[0]};

        // 每个面 4 个顶点
        float uvs[4][2] = {{0,0}, {0,1}, {1,1}, {1,0}};
        for (int i = 0; i < 4; ++i) {
            float x = (uvs[i][0] * 2 - 1);
            float y = (uvs[i][1] * 2 - 1);
            
            // 顶点位置 = 中心 + 法线方向偏移 + (轴1 * u) + (轴2 * v)
            geo.vertices.push_back((n[0] + x*t[0] + y*b[0]) * halfExtend);
            geo.vertices.push_back((n[1] + x*t[1] + y*b[1]) * halfExtend);
            geo.vertices.push_back((n[2] + x*t[2] + y*b[2]) * halfExtend);
            geo.vertices.push_back(1.0f);

            geo.normals.insert(geo.normals.end(), n, n + 3);
            geo.tangents.insert(geo.tangents.end(), t, t + 3);
            geo.texCoords.insert(geo.texCoords.end(), uvs[i], uvs[i] + 2);
        }

        // 索引 (每个面两个三角形)
        uint32_t offset = face * 4;
        uint32_t ids[] = { offset, offset + 2, offset + 1, offset, offset + 3, offset + 2 };
        geo.indices.insert(geo.indices.end(), ids, ids + 6);
    }

    geo.finalize();
    return geo;
}

inline Geometry createSphere(float radius, uint32_t numberSlices) {
    Geometry geo;
    geo.mode = GL_TRIANGLES;

    uint32_t numberParallels = numberSlices / 2;
    float angleStep = (2.0f * PI) / (float)numberSlices;

    Vec4 helpVector(1.0f, 0.0f, 0.0f, 0.0f);

    for (uint32_t i = 0; i < numberParallels + 1; i++) {
        for (uint32_t j = 0; j < numberSlices + 1; j++) {
            float x = radius * std::sin(angleStep * (float)i) * std::sin(angleStep * (float)j);
            float y = radius * std::cos(angleStep * (float)i);
            float z = radius * std::sin(angleStep * (float)i) * std::cos(angleStep * (float)j);
            
            geo.addVertex(x, y, z);
            
            geo.addNormal(x / radius, y / radius, z / radius);

            float s = (float)j / (float)numberSlices;
            float t = 1.0f - (float)i / (float)numberParallels;
            geo.addTexCoord(s, t);

            // Tangent
            Mat4 rot = Mat4::RotateY(360.0f * s);
            Vec4 tan = rot * helpVector;
            geo.addTangent(tan.x, tan.y, tan.z);
        }
    }

    for (uint32_t i = 0; i < numberParallels; i++) {
        for (uint32_t j = 0; j < numberSlices; j++) {
            geo.addIndices({
                i * (numberSlices + 1) + j,
                (i + 1) * (numberSlices + 1) + j,
                (i + 1) * (numberSlices + 1) + (j + 1),

                i * (numberSlices + 1) + j,
                (i + 1) * (numberSlices + 1) + (j + 1),
                i * (numberSlices + 1) + (j + 1)
            });
        }
    }

    geo.finalize();
    return geo;
}

inline Geometry createTorus(float innerRadius, float outerRadius, uint32_t numberSlices, uint32_t numberStacks) {
    Geometry geo;
    geo.mode = GL_TRIANGLES;

    float torusRadius = (outerRadius - innerRadius) / 2.0f;
    float centerRadius = outerRadius - torusRadius;
    
    Vec4 helpVector(0.0f, 1.0f, 0.0f, 0.0f);

    float sIncr = 1.0f / (float)numberSlices;
    float tIncr = 1.0f / (float)numberStacks;
    float s = 0.0f;

    for (uint32_t sideCount = 0; sideCount <= numberSlices; ++sideCount, s += sIncr) {
        float cos2PIs = std::cos(2.0f * PI * s);
        float sin2PIs = std::sin(2.0f * PI * s);

        float t = 0.0f;
        for (uint32_t faceCount = 0; faceCount <= numberStacks; ++faceCount, t += tIncr) {
            float cos2PIt = std::cos(2.0f * PI * t);
            float sin2PIt = std::sin(2.0f * PI * t);

            float x = (centerRadius + torusRadius * cos2PIt) * cos2PIs;
            float y = (centerRadius + torusRadius * cos2PIt) * sin2PIs;
            float z = torusRadius * sin2PIt;

            geo.addVertex(x, y, z);
            
            geo.addNormal(cos2PIs * cos2PIt, sin2PIs * cos2PIt, sin2PIt);
            geo.addTexCoord(s, t);

            // Tangent: Rotate (0,1,0) around Z by 360*s
            Mat4 rot = Mat4::RotateZ(360.0f * s);
            Vec4 tan = rot * helpVector;
            geo.addTangent(tan.x, tan.y, tan.z);
        }
    }

    for (uint32_t sideCount = 0; sideCount < numberSlices; ++sideCount) {
        for (uint32_t faceCount = 0; faceCount < numberStacks; ++faceCount) {
            uint32_t v0 = ((sideCount * (numberStacks + 1)) + faceCount);
            uint32_t v1 = (((sideCount + 1) * (numberStacks + 1)) + faceCount);
            uint32_t v2 = (((sideCount + 1) * (numberStacks + 1)) + (faceCount + 1));
            uint32_t v3 = ((sideCount * (numberStacks + 1)) + (faceCount + 1));

            geo.addIndices({v0, v1, v2, v0, v2, v3});
        }
    }
    
    geo.finalize();
    return geo;
}

inline Geometry createCylinder(float halfExtend, float radius, uint32_t numberSlices) {
    Geometry geo;
    geo.mode = GL_TRIANGLES;

    float angleStep = (2.0f * PI) / (float)numberSlices;
    
    // Center bottom
    geo.addVertex(0.0f, -halfExtend, 0.0f);
    geo.addNormal(0.0f, -1.0f, 0.0f);
    geo.addTangent(0.0f, 0.0f, 1.0f);
    geo.addTexCoord(0.0f, 0.0f);
    
    // Bottom circle
    for (uint32_t i = 0; i <= numberSlices; i++) {
        float angle = angleStep * (float)i;
        float x = std::cos(angle) * radius;
        float z = -std::sin(angle) * radius;
        geo.addVertex(x, -halfExtend, z);
        geo.addNormal(0.0f, -1.0f, 0.0f);
        geo.addTangent(std::sin(angle), 0.0f, std::cos(angle));
        geo.addTexCoord(0.0f, 0.0f);
    }

    // Center top
    geo.addVertex(0.0f, halfExtend, 0.0f);
    geo.addNormal(0.0f, 1.0f, 0.0f);
    geo.addTangent(0.0f, 0.0f, -1.0f);
    geo.addTexCoord(1.0f, 1.0f);

    // Top circle
    for (uint32_t i = 0; i <= numberSlices; i++) {
        float angle = angleStep * (float)i;
        float x = std::cos(angle) * radius;
        float z = -std::sin(angle) * radius;
        geo.addVertex(x, halfExtend, z);
        geo.addNormal(0.0f, 1.0f, 0.0f);
        geo.addTangent(-std::sin(angle), 0.0f, -std::cos(angle));
        geo.addTexCoord(1.0f, 1.0f);
    }

    // Sides
    for (uint32_t i = 0; i <= numberSlices; i++) {
        float angle = angleStep * (float)i;
        float sign = -1.0f;
        for (int j = 0; j < 2; j++) {
            float x = std::cos(angle) * radius;
            float y = halfExtend * sign;
            float z = -std::sin(angle) * radius;
            geo.addVertex(x, y, z);
            geo.addNormal(std::cos(angle), 0.0f, -std::sin(angle));
            geo.addTangent(-std::sin(angle), 0.0f, -std::cos(angle));
            geo.addTexCoord((float)i/numberSlices, (sign + 1.0f) / 2.0f);
            sign = 1.0f;
        }
    }

    uint32_t centerIndex = 0;
    uint32_t indexCounter = 1;
    
    // Bottom indices
    for (uint32_t i = 0; i < numberSlices; i++) {
        geo.addIndices({centerIndex, indexCounter + 1, indexCounter});
        indexCounter++;
    }
    indexCounter++;

    // Top indices
    centerIndex = indexCounter;
    indexCounter++;
    for (uint32_t i = 0; i < numberSlices; i++) {
        geo.addIndices({centerIndex, indexCounter, indexCounter + 1});
        indexCounter++;
    }
    indexCounter++;

    // Side indices
    for (uint32_t i = 0; i < numberSlices; i++) {
        geo.addIndices({
            indexCounter, indexCounter + 2, indexCounter + 1,
            indexCounter + 2, indexCounter + 3, indexCounter + 1
        });
        indexCounter += 2;
    }

    geo.finalize();
    return geo;
}

} // namespace geometry
} // namespace tinygl