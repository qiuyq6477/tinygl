#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <tinygl/framework/mesh.h>
#include <tinygl/core/tinygl.h>

namespace tinygl {

class TINYGL_API Model {
public:
    // Model data
    std::vector<Mesh> meshes;
    std::string directory;
    
    // Stores loaded textures to prevent duplicates (path -> textureID)
    std::unordered_map<std::string, GLuint> loadedTextures;

    // Constructor, expects a filepath to a 3D model.
    Model(const std::string& path, SoftRenderContext& ctx) : m_ctx(&ctx) {
        loadModel(path, ctx);
    }
    
    ~Model();

    // Disable copying
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // Enable moving
    Model(Model&& other) noexcept;
    Model& operator=(Model&& other) noexcept;

    // Draws the model, and thus all its meshes
    template <typename ShaderT>
    void Draw(SoftRenderContext& ctx, ShaderT& shader) {
        for(unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(ctx, shader);
    }

private:
    SoftRenderContext* m_ctx = nullptr;

    // Loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(std::string path, SoftRenderContext& ctx);

    // Processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode(aiNode *node, const aiScene *scene, SoftRenderContext& ctx);

    Mesh processMesh(aiMesh *mesh, const aiScene *scene, SoftRenderContext& ctx);

    // Checks all material textures of a given type and loads the textures if they're not loaded yet.
    std::vector<GLuint> loadMaterialTextures(aiMaterial *mat, aiTextureType type, SoftRenderContext& ctx);

    // Helper to load texture from file
    GLuint textureFromFile(const char* path, const std::string& directory, SoftRenderContext& ctx);
};

} // namespace tinygl
