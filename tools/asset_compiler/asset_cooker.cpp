#include "asset_cooker.h"
#include <framework/asset_formats.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <fstream>
#include <vector>
#include <iostream>
#include <functional>

using namespace framework;
namespace fs = std::filesystem;

namespace tools {

bool AssetCooker::CookTexture(const fs::path& source, const fs::path& dest) {
    int w, h, c;
    stbi_set_flip_vertically_on_load(true); 
    unsigned char* data = stbi_load(source.string().c_str(), &w, &h, &c, 4); // Force RGBA
    if (!data) {
        std::cerr << "Failed to load texture: " << source << std::endl;
        return false;
    }

    std::ofstream out(dest, std::ios::binary);
    if (!out) return false;

    AssetHeader header;
    header.magic = MAGIC_TTEX;
    header.version = ASSET_VERSION;
    header.compressionMode = CompressionMode::None;
    
    // Calculate total size: Header + TextureHeader + Data
    // We write AssetHeader first.
    
    TextureHeader texHeader;
    texHeader.width = w;
    texHeader.height = h;
    texHeader.channels = 4;
    texHeader.mipLevels = 1;
    texHeader.format = 0; // RGBA8

    size_t payloadSize = sizeof(TextureHeader) + (w * h * 4);
    header.dataSize = payloadSize;

    out.write(reinterpret_cast<char*>(&header), sizeof(header));
    out.write(reinterpret_cast<char*>(&texHeader), sizeof(texHeader));
    out.write(reinterpret_cast<char*>(data), w * h * 4);
    
    stbi_image_free(data);
    return true;
}

bool AssetCooker::CookModel(const fs::path& source, const fs::path& dest) {
    Assimp::Importer importer;
    // Use PreTransformVertices to flatten hierarchy for now (simplifies Phase 1)
    const aiScene* scene = importer.ReadFile(source.string(), 
        aiProcess_Triangulate | 
        aiProcess_GenNormals | 
        aiProcess_PreTransformVertices | 
        aiProcess_FlipUVs |
        aiProcess_CalcTangentSpace
    );

    if (!scene || !scene->mRootNode) {
        std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
        return false;
    }

    std::ofstream out(dest, std::ios::binary);
    if (!out) return false;

    // 1. Prepare Data
    std::vector<aiMesh*> meshes;
    // Flatten hierarchy collection
    std::function<void(aiNode*)> processNode = [&](aiNode* node) {
        for(unsigned int i = 0; i < node->mNumMeshes; i++) {
            meshes.push_back(scene->mMeshes[node->mMeshes[i]]);
        }
        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i]);
        }
    };
    processNode(scene->mRootNode);

    // 2. Write Asset Header
    AssetHeader assetHeader;
    assetHeader.magic = MAGIC_TMODEL;
    assetHeader.version = ASSET_VERSION;
    assetHeader.compressionMode = CompressionMode::None;
    // We'll come back and write dataSize later
    std::streampos headerPos = out.tellp();
    out.write(reinterpret_cast<char*>(&assetHeader), sizeof(assetHeader));

    // 3. Write Model Header
    ModelHeader modelHeader;
    modelHeader.meshCount = (uint32_t)meshes.size();
    modelHeader.materialCount = scene->mNumMaterials;
    modelHeader.nodeCount = 0; // Not used yet
    out.write(reinterpret_cast<char*>(&modelHeader), sizeof(modelHeader));

    // 4. Write Materials
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* aiMat = scene->mMaterials[i];
        MaterialHeader matHeader = {};
        
        // Extract Properties
        aiColor3D color(1.f, 1.f, 1.f);
        if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            matHeader.data.diffuse[0] = color.r; matHeader.data.diffuse[1] = color.g; matHeader.data.diffuse[2] = color.b; matHeader.data.diffuse[3] = 1.0f;
        } else {
            matHeader.data.diffuse[0] = 1.0f; matHeader.data.diffuse[1] = 1.0f; matHeader.data.diffuse[2] = 1.0f; matHeader.data.diffuse[3] = 1.0f;
        }

        if (aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
            matHeader.data.ambient[0] = color.r; matHeader.data.ambient[1] = color.g; matHeader.data.ambient[2] = color.b; matHeader.data.ambient[3] = 1.0f;
        }
        if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
            matHeader.data.specular[0] = color.r; matHeader.data.specular[1] = color.g; matHeader.data.specular[2] = color.b; matHeader.data.specular[3] = 1.0f;
        }
        if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) {
            matHeader.data.emissive[0] = color.r; matHeader.data.emissive[1] = color.g; matHeader.data.emissive[2] = color.b; matHeader.data.emissive[3] = 1.0f;
        }

        float val = 0.0f;
        if (aiMat->Get(AI_MATKEY_SHININESS, val) == AI_SUCCESS) matHeader.data.shininess = val; else matHeader.data.shininess = 32.0f;
        if (aiMat->Get(AI_MATKEY_OPACITY, val) == AI_SUCCESS) matHeader.data.opacity = val; else matHeader.data.opacity = 1.0f;
        
        int bVal = 0;
        if (aiMat->Get(AI_MATKEY_TWOSIDED, bVal) == AI_SUCCESS) matHeader.data.doubleSided = bVal;

        // Extract Texture Paths
        std::vector<std::string> texPaths;
        auto getTexturePath = [&](aiTextureType type) -> std::string {
            if (aiMat->GetTextureCount(type) > 0) {
                aiString str;
                aiMat->GetTexture(type, 0, &str);
                return str.C_Str();
            }
            return "";
        };

        texPaths.push_back(getTexturePath(aiTextureType_DIFFUSE));
        texPaths.push_back(getTexturePath(aiTextureType_SPECULAR));
        std::string normalPath = getTexturePath(aiTextureType_NORMALS);
        if (normalPath.empty()) normalPath = getTexturePath(aiTextureType_HEIGHT);
        texPaths.push_back(normalPath);
        texPaths.push_back(getTexturePath(aiTextureType_AMBIENT));
        texPaths.push_back(getTexturePath(aiTextureType_EMISSIVE));
        texPaths.push_back(getTexturePath(aiTextureType_OPACITY));

        // Write lengths
        for(int t=0; t<6; t++) {
            matHeader.texturePathLengths[t] = (uint32_t)texPaths[t].length();
        }

        // Write Header
        out.write(reinterpret_cast<char*>(&matHeader), sizeof(matHeader));
        
        // Write Path Strings
        for(const auto& p : texPaths) {
            if (!p.empty()) {
                out.write(p.c_str(), p.length());
            }
        }
    }

    // 5. Write Meshes
    for (aiMesh* mesh : meshes) {
        SubMeshHeader smh;
        smh.vertexCount = mesh->mNumVertices;
        smh.indexCount = mesh->mNumFaces * 3;
        smh.materialIndex = mesh->mMaterialIndex;

        // Calc Bounds (Simple AABB)
        aiVector3D min(1e10f, 1e10f, 1e10f);
        aiVector3D max(-1e10f, -1e10f, -1e10f);
        for(unsigned int v=0; v<mesh->mNumVertices; v++) {
            min.x = std::min(min.x, mesh->mVertices[v].x);
            min.y = std::min(min.y, mesh->mVertices[v].y);
            min.z = std::min(min.z, mesh->mVertices[v].z);
            max.x = std::max(max.x, mesh->mVertices[v].x);
            max.y = std::max(max.y, mesh->mVertices[v].y);
            max.z = std::max(max.z, mesh->mVertices[v].z);
        }
        smh.minBounds[0] = min.x; smh.minBounds[1] = min.y; smh.minBounds[2] = min.z;
        smh.maxBounds[0] = max.x; smh.maxBounds[1] = max.y; smh.maxBounds[2] = max.z;

        out.write(reinterpret_cast<char*>(&smh), sizeof(smh));

        // Write Vertices
        std::vector<VertexPacked> vertices(mesh->mNumVertices);
        for(unsigned int v=0; v<mesh->mNumVertices; v++) {
            vertices[v].pos[0] = mesh->mVertices[v].x; vertices[v].pos[1] = mesh->mVertices[v].y; vertices[v].pos[2] = mesh->mVertices[v].z; vertices[v].pos[3] = 1.0f;
            
            if(mesh->HasNormals()) {
                vertices[v].norm[0] = mesh->mNormals[v].x; vertices[v].norm[1] = mesh->mNormals[v].y; vertices[v].norm[2] = mesh->mNormals[v].z; vertices[v].norm[3] = 0.0f;
            } else {
                vertices[v].norm[0] = 0; vertices[v].norm[1] = 0; vertices[v].norm[2] = 0; vertices[v].norm[3] = 0;
            }

            if(mesh->mTextureCoords[0]) {
                vertices[v].uv[0] = mesh->mTextureCoords[0][v].x; vertices[v].uv[1] = mesh->mTextureCoords[0][v].y;
            } else {
                vertices[v].uv[0] = 0; vertices[v].uv[1] = 0;
            }
            vertices[v].uv[2] = 0; vertices[v].uv[3] = 0;
        }
        out.write(reinterpret_cast<char*>(vertices.data()), vertices.size() * sizeof(VertexPacked));

        // Write Indices
        std::vector<uint32_t> indices;
        indices.reserve(mesh->mNumFaces * 3);
        for(unsigned int f=0; f<mesh->mNumFaces; f++) {
            indices.push_back(mesh->mFaces[f].mIndices[0]);
            indices.push_back(mesh->mFaces[f].mIndices[1]);
            indices.push_back(mesh->mFaces[f].mIndices[2]);
        }
        out.write(reinterpret_cast<char*>(indices.data()), indices.size() * sizeof(uint32_t));
    }

    // Fixup Data Size
    std::streampos endPos = out.tellp();
    assetHeader.dataSize = (uint64_t)(endPos - headerPos) - sizeof(AssetHeader);
    
    out.seekp(headerPos);
    out.write(reinterpret_cast<char*>(&assetHeader), sizeof(assetHeader));
    
    return true;
}

} // namespace tools
