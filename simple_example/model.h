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
        tex = cv::Mat(h, w, CV_32FC3);
    else
        tex = cv::Mat(h, w, CV_32FC1);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            bool c = ((x / checker) + (y / checker)) & 1;
            if (channels == 3)
                tex.at<cv::Vec3f>(y, x) = c ? cv::Vec3f(0.5, 0.5, 0.5) : cv::Vec3f(1.0, 1.0, 1.0);
            else
                tex.at<float>(y, x) = c ? 0.5f : 1.0f;
        }
    }
    return tex;
}

bool LoadAssimpMesh(const std::string &path,
                    std::vector<Eigen::Vector3f> &vertices,
                    std::vector<Eigen::Vector2f> &texcoords,
                    std::vector<Eigen::Vector3f> &normals,
                    std::vector<unsigned int> &indices,
                    std::vector<std::string> &textures)
// static bool LoadAssimpMesh(const std::string &path,
//                            std::vector<float> &vertices,
//                            std::vector<float> &normals,
//                            std::vector<float> &texcoords,
//                            std::vector<unsigned int> &indices)
{
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
        return false;
    }

    vertices.clear();
    normals.clear();
    texcoords.clear();
    indices.clear();

    std::string base_path = path.substr(0, path.find_last_of('/'));

    size_t baseVertex = 0;
    Eigen::Vector3f minB(std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max());
    Eigen::Vector3f maxB(-std::numeric_limits<float>::max(),
                         -std::numeric_limits<float>::max(),
                         -std::numeric_limits<float>::max());

    // aiNode *root = scene->mRootNode;

    // for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    unsigned m = 0;
    {
        const aiMesh *mesh = scene->mMeshes[m];
        // vertices & uvs
        for (unsigned v = 0; v < mesh->mNumVertices; ++v)
        {
            aiVector3D p = mesh->mVertices[v];
            aiVector3D n = mesh->mNormals[v];

            Eigen::Vector3f vertice(p.x, p.y, p.z);
            // p.z = -p.z;
            vertices.push_back(vertice);
            // vertices.push_back(p.x);
            // vertices.push_back(p.y);
            // vertices.push_back(p.z);

            if (mesh->HasNormals())
            {
                Eigen::Vector3f normal(n.x, n.y, n.z);
                normals.push_back(normal);
                // normals.push_back(n.x);
                // normals.push_back(n.y);
                // normals.push_back(n.z);
            }

            minB = minB.cwiseMin(vertice);
            maxB = maxB.cwiseMax(vertice);

            // if (mesh->HasTextureCoords(0))
            if (mesh->mTextureCoords[0])
            {
                aiVector3D t = mesh->mTextureCoords[0][v];
                texcoords.push_back(Eigen::Vector2f(t.x, t.y));
                // texcoords.push_back(t.x);
                // texcoords.push_back(t.y);
            }
            else
            {
                texcoords.push_back(Eigen::Vector2f(0.0f, 0.0f));
                // texcoords.push_back(0.0f);
                // texcoords.push_back(0.0f);
            }
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

    Eigen::Vector3f center = 0.5f * (minB + maxB);
    float radius = (maxB - center).norm();

    for (size_t i = 0; i < vertices.size(); ++i)
    {
        vertices[i] -= center;
        vertices[i] /= radius;
    }

    return true;
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