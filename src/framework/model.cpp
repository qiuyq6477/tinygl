#include <tinygl/framework/model.h>
#include <tinygl/framework/texture_manager.h>
#include <tinygl/base/log.h>
#include <tinygl/third_party/stb_image.h>

namespace tinygl {

Model::~Model() {
}

Model::Model(Model&& other) noexcept 
    : meshes(std::move(other.meshes)),
      directory(std::move(other.directory)),
      texturesKeepAlive(std::move(other.texturesKeepAlive)),
      m_ctx(other.m_ctx)
{
    other.m_ctx = nullptr;
}

Model& Model::operator=(Model&& other) noexcept {
    if (this != &other) {
        // Move data
        meshes = std::move(other.meshes);
        directory = std::move(other.directory);
        texturesKeepAlive = std::move(other.texturesKeepAlive);
        m_ctx = other.m_ctx;

        // Reset source
        other.m_ctx = nullptr;
    }
    return *this;
}

void Model::loadModel(std::string path, SoftRenderContext& ctx) {
    Assimp::Importer importer;
    // Read file via Assimp
    // Triangulate: transform all primitives into triangles
    // FlipUVs: flip texture coordinates on y-axis if necessary (OpenGL expects Y up, some formats are Y down)
    // GenNormals: create normals if missing
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR("ERROR::ASSIMP:: " + std::string(importer.GetErrorString()));
        return;
    }
    
    // Retrieve the directory path of the filepath
    directory = path.substr(0, path.find_last_of('/'));

    // Process ASSIMP's root node recursively
    processNode(scene->mRootNode, scene, ctx);
}

void Model::processNode(aiNode *node, const aiScene *scene, SoftRenderContext& ctx) {
    // Process each mesh located at the current node
    for(unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene, ctx));
    }
    
    // Recursively process children
    for(unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene, ctx);
    }
}

Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene, SoftRenderContext& ctx) {
    // Data to fill
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    Material material;

    // Walk through each of the mesh's vertices
    for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        
        // Positions
        vertex.Position.x = mesh->mVertices[i].x;
        vertex.Position.y = mesh->mVertices[i].y;
        vertex.Position.z = mesh->mVertices[i].z;
        vertex.Position.w = 1.0f;
        
        // Normals
        if (mesh->HasNormals()) {
            vertex.Normal.x = mesh->mNormals[i].x;
            vertex.Normal.y = mesh->mNormals[i].y;
            vertex.Normal.z = mesh->mNormals[i].z;
            vertex.Normal.w = 0.0f;
        } else {
            vertex.Normal = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        // Texture Coordinates
        if(mesh->mTextureCoords[0]) { // Does the mesh contain texture coordinates?
            vertex.TexCoords.x = mesh->mTextureCoords[0][i].x;
            vertex.TexCoords.y = mesh->mTextureCoords[0][i].y;
            vertex.TexCoords.z = 0.0f;
            vertex.TexCoords.w = 0.0f;
        } else {
            vertex.TexCoords = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }
        
        vertices.push_back(vertex);
    }

    // Indices
    for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for(unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    // Process materials
    if(mesh->mMaterialIndex >= 0) {
        aiMaterial* materialPtr = scene->mMaterials[mesh->mMaterialIndex];
        
        // 1. Diffuse maps -> Slot 0
        std::vector<GLuint> diffuseMaps = loadMaterialTextures(materialPtr, aiTextureType_DIFFUSE, ctx);
        if (!diffuseMaps.empty()) {
             material.SetTexture(0, diffuseMaps[0]);
        }
        
        // 2. Specular maps -> Slot 1
        std::vector<GLuint> specularMaps = loadMaterialTextures(materialPtr, aiTextureType_SPECULAR, ctx);
        if (!specularMaps.empty()) {
            material.SetTexture(1, specularMaps[0]);
        }

        // 3. Material Properties
        aiColor3D color(0.f, 0.f, 0.f);
        if (AI_SUCCESS == materialPtr->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
            material.color = Vec4(color.r, color.g, color.b, 1.0f);
        }
        float shininess = 0.0f;
        if (AI_SUCCESS == materialPtr->Get(AI_MATKEY_SHININESS, shininess)) {
            material.shininess = shininess;
        }
    }

    return Mesh(vertices, indices, material, ctx);
}

std::vector<GLuint> Model::loadMaterialTextures(aiMaterial *mat, aiTextureType type, SoftRenderContext& ctx) {
    std::vector<GLuint> textures;
    for(unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        
        // Build full path
        std::string texturePath = std::string(str.C_Str());
        if (!directory.empty()) {
            texturePath = directory + '/' + texturePath;
        }
        
        // Delegate to TextureManager
        auto tex = TextureManager::Load(ctx, texturePath);
        if (tex && tex->id != 0) {
            textures.push_back(tex->id);
            // Keep it alive
            texturesKeepAlive.push_back(tex);
        }
    }
    return textures;
}

} // namespace tinygl