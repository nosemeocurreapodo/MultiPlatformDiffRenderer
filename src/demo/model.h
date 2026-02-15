#pragma once

#include <opencv2/opencv.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Eigen/Core>
#include <Eigen/Geometry>

cv::Mat MakeCheckerTex(int w = 512, int h = 512, int checker = 32, int channels = 3)
{
    cv::Mat tex;

    if (channels == 3)
        tex = cv::Mat(h, w, CV_8UC3);
    else
        tex = cv::Mat(h, w, CV_8UC1);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            bool c = ((x / checker) + (y / checker)) & 1;
            if (channels == 3)
                tex.at<cv::Vec3b>(y, x) = c ? cv::Vec3b(127, 127, 127) : cv::Vec3b(255, 255, 255);
            else
                tex.at<unsigned char>(y, x) = c ? 127 : 255;
        }
    }
    return tex;
}

template <typename Mesh, typename Texture>
void LoadAssimpMesh(const std::string &path,
                    Mesh &mesh_,
                    Texture &diffuse_)
// static bool LoadAssimpMesh(const std::string &path,
//                            std::vector<float> &vertices,
//                            std::vector<float> &normals,
//                            std::vector<float> &texcoords,
//                            std::vector<unsigned int> &indices)
{
    std::vector<float> vertex;
    std::vector<int> indices;
    bool has_positions = false;
    bool has_texcoords = false;
    bool has_normals = false;

    std::vector<std::string> textures;

    Assimp::Importer imp;
    const aiScene *scene = imp.ReadFile(
        path,
        aiProcess_Triangulate |
            // aiProcess_JoinIdenticalVertices |
            aiProcess_GenNormals |
            aiProcess_CalcTangentSpace |
            // aiProcess_ImproveCacheLocality |
            // aiProcess_OptimizeMeshes |
            aiProcess_FlipUVs);
    // if (!scene || !scene->HasMeshes())
    //{
    //     std::cerr << "Assimp error: " << imp.GetErrorString() << std::endl;
    //     return false;
    // }

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
    {
        std::cout << "ERROR::ASSIMP:: " << imp.GetErrorString() << std::endl;
        // return false;
        throw std::runtime_error("Failed to load model: " + path);
    }

    vertex.clear();
    indices.clear();

    std::string base_path = path.substr(0, path.find_last_of('/'));

    size_t baseVertex = 0;
    Vec3<float> minB(std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max());
    Vec3<float> maxB(-std::numeric_limits<float>::max(),
                     -std::numeric_limits<float>::max(),
                     -std::numeric_limits<float>::max());

    // aiNode *root = scene->mRootNode;

    // for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    unsigned m = 0;
    {
        const aiMesh *mesh = scene->mMeshes[m];

        has_positions = true;

        if (mesh->mTextureCoords[0])
            has_texcoords = true;
        else
            has_texcoords = false;

        // if (mesh->HasNormals())
        //     has_normals = true;
        // else
        has_normals = false;

        vertex.reserve(mesh->mNumVertices * 8);
        indices.reserve(mesh->mNumFaces * 3);

        // vertices & uvs
        for (unsigned v = 0; v < mesh->mNumVertices; ++v)
        {
            aiVector3D p = mesh->mVertices[v];
            Vec3<float> vertice(p.x, p.y, p.z);
            // p.z = -p.z;
            // vertex.push_back(vertice);
            vertex.push_back(p.x);
            vertex.push_back(p.y);
            vertex.push_back(p.z);

            // if (mesh->HasTextureCoords(0))
            if (has_texcoords)
            {
                aiVector3D t = mesh->mTextureCoords[0][v];
                // texcoords.push_back(Eigen::Vector2f(t.x, t.y));
                vertex.push_back(t.x);
                vertex.push_back(t.y);
            }

            if (has_normals)
            {
                aiVector3D n = mesh->mNormals[v];
                // Eigen::Vector3f normal(n.x, n.y, n.z);
                // normals.push_back(normal);
                vertex.push_back(n.x);
                vertex.push_back(n.y);
                vertex.push_back(n.z);
            }

            minB = minB.cwiseMin(vertice);
            maxB = maxB.cwiseMax(vertice);
        }
        // indices
        for (unsigned f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace &face = mesh->mFaces[f];
            if (face.mNumIndices != 3)
                continue;
            indices.push_back(static_cast<unsigned>(baseVertex) + face.mIndices[0]);
            indices.push_back(static_cast<unsigned>(baseVertex) + face.mIndices[1]);
            indices.push_back(static_cast<unsigned>(baseVertex) + face.mIndices[2]);
        }
        baseVertex += mesh->mNumVertices;

        // materials
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

        for (unsigned int i = 0; i < material->GetTextureCount(aiTextureType_DIFFUSE); i++)
        {
            aiString str;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &str);
            textures.push_back(base_path + "/" + str.C_Str());
            // std::cout << str.C_Str() << std::endl;
        }
    }

    mesh_ = Mesh(vertex, indices, has_positions, has_texcoords, has_normals);

    cv::Mat diffuse_cv;
    if (!textures.empty())
    {
        diffuse_cv = cv::imread(textures[0], cv::IMREAD_GRAYSCALE);
    }
    else
    {
        diffuse_cv = MakeCheckerTex(1024, 1024, 32);
    }

    diffuse_ = Texture(diffuse_cv.cols, diffuse_cv.rows, 0);
    UploadMatToTexture(diffuse_, 0, diffuse_cv);

    std::vector<Vec3<float>> points = get_vertices(mesh_);

    Vec3<float> center = 0.5f * (minB + maxB);
    float radius = (maxB - center).norm();

    for (Vec3<float> &p : points)
    {
        p -= center;
        p /= radius;
    }

    set_vertices(mesh_, points);
}

// Try to load the first diffuse texture in the first material.
// Returns empty Mat if not found.
cv::Mat TryLoadDiffuseTexture(const std::string &modelPath)
{
    Assimp::Importer imp;
    const aiScene *scene = imp.ReadFile(modelPath, 0);
    if (!scene || !scene->HasMaterials())
        return cv::Mat();

    const aiMaterial *mat0 = scene->mMaterials[0];
    aiString rel;
    if (AI_SUCCESS == mat0->GetTexture(aiTextureType_DIFFUSE, 0, &rel))
    {
        // Resolve relative path
        std::string baseDir = modelPath.substr(0, modelPath.find_last_of("/\\"));
        std::string texPath = baseDir.empty() ? rel.C_Str() : (baseDir + "/" + std::string(rel.C_Str()));
        cv::Mat tex = cv::imread(texPath, cv::IMREAD_COLOR);
        if (!tex.empty())
            return tex;
    }
    return cv::Mat();
}