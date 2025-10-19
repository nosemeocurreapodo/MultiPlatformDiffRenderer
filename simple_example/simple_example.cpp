#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include <opencv2/opencv.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Eigen/Core>
#include <Eigen/Geometry>

#include "core/common.h"
#include "common/test_helpers.h"
#include "core/format_converters.h"
// #include "core/types.h"

// #include "backends/cpu/devicecpu.h"
#include "backends/cpu/buffercpu.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/meshcpu.h"
#include "backends/cpu/renderercpu.h"

// #ifdef COMPILE_GL
#include "backends/gl/devicegl_glad.h"
#include "backends/gl/texturegl.h"
#include "backends/gl/meshgl.h"
#include "backends/gl/renderergl.h"
// #endif

#include <iostream>

template <typename T>
using Texture = TextureCPU<T>;
using Mesh = MeshCPU;
using Renderer = ImageRendererCPU;

// template <typename T>
// using Texture = TextureGL<T>;
// using Mesh = MeshGL;
// using Renderer = ImageRendererGL;

static bool LoadAssimpMesh(const std::string &path,
                           std::vector<Eigen::Vector3f> &vertices,
                           std::vector<Eigen::Vector3f> &normals,
                           std::vector<Eigen::Vector2f> &texcoords,
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

static cv::Mat MakeCheckerTex(int w = 512, int h = 512, int checker = 32, int channels = 3)
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

linalg::Mat4<float> EigenToLinalg(const Eigen::Matrix4f &m)
{
    linalg::Mat4<float> lm;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            lm(r, c) = m(r, c);
    return lm;
}

int main()
{
    std::vector<Eigen::Vector3f> vertices;
    std::vector<Eigen::Vector3f> normals;
    std::vector<Eigen::Vector2f> texcoords;
    std::vector<unsigned int> indices;
    std::vector<std::string> textures;

    // std::vector<float> vertices;
    // std::vector<float> normals;
    // std::vector<float> texcoords;
    // std::vector<unsigned int> indices;

    std::string mesh_path = "/workspaces/MultiPlatformDiffRenderer/tests/data/planet/planet.obj";
    // std::string mesh_path = "/workspaces/MultiPlatformDiffRenderer/tests/data/bunny/bun_zipper_res4.ply";

    if (!LoadAssimpMesh(mesh_path,
                        vertices,
                        normals,
                        texcoords,
                        indices,
                        textures))
    {
        std::cout << "Failed to load model" << std::endl;
        return -1;
    }

    if (!InitEGL())
    {
        std::cout << "Failed to initialize EGL" << std::endl;
        return -1;
    }

    int width = 640;
    int height = 480;

    // Texture<linalg::Vec3<float>> in_texture(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    // Texture<linalg::Vec3<float>> out_texture(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));

    Texture<float> in_texture(width, height, -1.0);
    Texture<float> out_texture(width, height, -1.0);

    cv::Mat diffuse;
    if (textures.size() > 0)
    {
        diffuse = cv::imread(textures[0], cv::IMREAD_GRAYSCALE);
        cv::resize(diffuse, diffuse, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);
        diffuse.convertTo(diffuse, CV_32F, 1.0 / 255.0);
    }
    else
    {
        diffuse = MakeCheckerTex(width, height, 32, 1);
    }

    UploadMatToTexture(in_texture, 0, diffuse);

    // Mesh mesh(vertices, normals, texcoords, indices);
    Mesh mesh(vertices, texcoords, normals, indices);

    Renderer renderer;

    cv::namedWindow("Simple Example", cv::WINDOW_AUTOSIZE);

    // render loop
    // -----------
    float dist = 2.0f;

    float fov_deg = 90.0f;

    float fx = 0.5f * width / std::tan(0.5f * fov_deg * float(M_PI / 180.0));
    float fy = fx;
    float cx = 0.5f * (width - 1);
    float cy = 0.5f * (height - 1);

    Camera<float> camera1(fx, fy, cx, cy, width, height);

    linalg::Mat4<float> p_ = camera1.GetProjectiveMatrix(0.1f, 100.0f);

    linalg::SE3<float> view_l_;
    view_l_.translation() = linalg::Vec3<float>(0.0f, 0.0f, -dist);
    linalg::Mat4<float> v_ = view_l_.matrix();

    linalg::SE3<float> model_l_;
    linalg::Mat4<float> m_ = model_l_.matrix();

    int i = 0;
    while (true)
    {
        i++;
        float t = float(i) * 0.016f; // ~60deg/s at 60fps for yaw
        Eigen::Matrix3f R =
            Eigen::AngleAxisf(0.15f * t, Eigen::Vector3f::UnitY())
                //     Eigen::AngleAxisf(M_PI / 2.0f, Eigen::Vector3f::UnitX())*/
                //        /*(Eigen::AngleAxisf(0.6f * t, Eigen::Vector3f::UnitY()))*/
                .toRotationMatrix();

        Eigen::Vector3f camPos = R.transpose() * Eigen::Vector3f(0, 0, dist);

        Eigen::Matrix4f view_e_;
        view_e_.block<3, 3>(0, 0) = R.transpose();
        view_e_.block<3, 1>(0, 3) = -R * camPos;
        view_e_.row(3) = Eigen::Vector4f(0, 0, 0, 1);

        v_ = EigenToLinalg(view_e_);

        linalg::SE3<float> transform(v_);

        // renderer.Render(mesh, p_, v_, m_, 0, 0, in_texture, out_texture);
        renderer.Render(mesh, transform, camera1, 0, 0, in_texture, out_texture);

        cv::Mat out_f = DownloadTextureToMat(out_texture, 0, CV_32FC1);

        // Pretty up the single-channel output
        cv::Mat out_u8, out_color;
        out_f.convertTo(out_u8, CV_8U, 255.0);
        // cv::applyColorMap(out_u8, out_color, cv::COLORMAP_TURBO);

        cv::imshow("Simple Example", out_u8);
        cv::waitKey(30);
    }

    return 0;
}
