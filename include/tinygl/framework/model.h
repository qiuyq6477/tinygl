#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <tinygl/framework/mesh.h>
#include <tinygl/framework/texture.h>
#include <tinygl/core/tinygl.h>
#include <memory>

namespace tinygl {

class TINYGL_API Model {
public:
    // Model data
    std::vector<Mesh> meshes;
    std::string directory;
    
    // Hold references to textures to keep them alive (TextureManager uses weak_ptr)
    std::vector<std::shared_ptr<Texture>> texturesKeepAlive;

    // Constructor, expects a filepath to a 3D model.
    Model(const std::string& path, SoftRenderContext& ctx) : m_ctx(&ctx) {
        loadModel(path, ctx);
    }
    
    ~Model(); // Default destructor is fine if we use shared_ptr, but we might want to ensure context valid?
              // Actually Texture destructor needs context... 
              // But Texture destructor stores its own pointer to context.
              // So default dtor for Model should be fine if Texture handles its cleanup.
              // We'll keep explicit ~Model in cpp to clear meshes if needed.

    // Disable copying
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // Enable moving
    Model(Model&& other) noexcept;
    Model& operator=(Model&& other) noexcept;

    // Draws the model, and thus all its meshes
    void Draw(SoftRenderContext& ctx, const Mat4& modelMatrix, const RenderState& state) {
        for(unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(ctx, modelMatrix, state);
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
};

} // namespace tinygl
