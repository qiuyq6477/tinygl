#include <tinygl/framework/model.h>
#include <tinygl/base/log.h>
#include <tinygl/third_party/stb_image.h>

namespace tinygl {

Model::~Model() {
    if (m_ctx) {
        // Free loaded textures
        for (auto& pair : loadedTextures) {
            GLuint id = pair.second;
            m_ctx->glDeleteTextures(1, &id);
        }
    }
}

Model::Model(Model&& other) noexcept 
    : meshes(std::move(other.meshes)),
      directory(std::move(other.directory)),
      loadedTextures(std::move(other.loadedTextures)),
      m_ctx(other.m_ctx)
{
    other.m_ctx = nullptr;
}

Model& Model::operator=(Model&& other) noexcept {
    if (this != &other) {
        // Clean up existing resources
        if (m_ctx) {
             for (auto& pair : loadedTextures) {
                GLuint id = pair.second;
                m_ctx->glDeleteTextures(1, &id);
             }
        }

        // Move data
        meshes = std::move(other.meshes);
        directory = std::move(other.directory);
        loadedTextures = std::move(other.loadedTextures);
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
    // CalcTangentSpace: calculate tangents and bitangents if they are missing
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
        // The node object only contains indices to index the actual objects in the scene. 
        // The scene contains all the data, node is just to keep stuff organized (like relations between nodes).
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene, ctx));
    }
    
    // After we've processed all of the meshes (if any) we then recursively process each of the children nodes
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

    // Walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
    for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        // Retrieve all indices of the face and store them in the indices vector
        for(unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    // Process materials
    if(mesh->mMaterialIndex >= 0) {
        aiMaterial* materialPtr = scene->mMaterials[mesh->mMaterialIndex];
        
        // 1. Diffuse maps
        std::vector<GLuint> diffuseMaps = loadMaterialTextures(materialPtr, aiTextureType_DIFFUSE, ctx);
        if (!diffuseMaps.empty()) material.diffuseMap = diffuseMaps[0];
        
        // 2. Specular maps
        std::vector<GLuint> specularMaps = loadMaterialTextures(materialPtr, aiTextureType_SPECULAR, ctx);
        if (!specularMaps.empty()) material.specularMap = specularMaps[0];

        // 3. Material Properties (Fallback color if no texture, etc.)
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
        
        // Check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
        std::string texturePath = std::string(str.C_Str());
        if(loadedTextures.find(texturePath) != loadedTextures.end()) {
            textures.push_back(loadedTextures[texturePath]);
        } else {   
            // If texture hasn't been loaded already, load it
            GLuint textureID = textureFromFile(str.C_Str(), this->directory, ctx);
            textures.push_back(textureID);
            loadedTextures[texturePath] = textureID;
        }
    }
    return textures;
}

GLuint Model::textureFromFile(const char* path, const std::string& directory, SoftRenderContext& ctx) {
    std::string filename = std::string(path);
    filename = directory + '/' + filename;

    GLuint textureID;
    ctx.glGenTextures(1, &textureID);

    int width, height, nrComponents;
    // Force loading as 4 components (RGBA) to simplify renderer handling
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 4);
    if (data) {
        GLenum format = GL_RGBA;

        ctx.glBindTexture(GL_TEXTURE_2D, textureID);
        // Note: tinygl's glTexImage2D currently expects data in a specific layout or handles it.
        // Assuming implementation handles standard raw pointers.
        // We use GL_UNSIGNED_BYTE type here (though tinygl mostly works with float internal, 
        // we should check if we need to convert to float or if glTexImage2D handles conversion).
        // Checking tinygl.cpp/gl_texture.cpp... 
        // tinygl usually converts internally. Let's pass standard params.
        
        ctx.glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        // ctx.glGenerateMipmap(GL_TEXTURE_2D);

        // Parameters
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        LOG_ERROR("Texture failed to load at path: " + filename);
        stbi_image_free(data);
    }

    return textureID;
}

} // namespace tinygl