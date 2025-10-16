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

// ---------- helpers ----------

static cv::Mat MakeCheckerTex(int w = 512, int h = 512, int checker = 32)
{
    cv::Mat tex(h, w, CV_8UC3);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            bool c = ((x / checker) + (y / checker)) & 1;
            tex.at<cv::Vec3b>(y, x) = c ? cv::Vec3b(40, 40, 40) : cv::Vec3b(220, 220, 220);
        }
    }
    return tex;
}

// Convert BGR (OpenCV) to single-channel float [0,1] for the demo pipeline.
// If your renderer supports RGB, switch TextureXRT<float> to the matching type.
static cv::Mat ToGrayFloat01(const cv::Mat &bgr)
{
    cv::Mat gray, f;
    if (bgr.channels() == 1)
        gray = bgr;
    else
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    gray.convertTo(f, CV_32FC1, 1.0 / 255.0);
    return f;
}

// Flatten an Assimp scene into interleaved arrays for MeshXRT
// vertices: (x,y,z) world/model space
// texcoords: (u,v)
// weights: we’ll use 1.0f per-vertex as a neutral attribute (matches your depth-mesh path’s shape)
static bool LoadAssimpMesh(const std::string &path,
                           std::vector<float> &vertices,
                           std::vector<float> &texcoords,
                           std::vector<float> &weights,
                           std::vector<unsigned int> &indices,
                           aiVector3D *outCenter = nullptr,
                           float *outRadius = nullptr)
{
    Assimp::Importer imp;
    const aiScene *scene = imp.ReadFile(
        path,
        aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_ImproveCacheLocality |
            aiProcess_OptimizeMeshes |
            aiProcess_FlipUVs);
    if (!scene || !scene->HasMeshes())
    {
        std::cerr << "Assimp error: " << imp.GetErrorString() << std::endl;
        return false;
    }

    vertices.clear();
    texcoords.clear();
    weights.clear();
    indices.clear();

    size_t baseVertex = 0;
    Eigen::Vector3f minB(std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max());
    Eigen::Vector3f maxB(-std::numeric_limits<float>::max(),
                         -std::numeric_limits<float>::max(),
                         -std::numeric_limits<float>::max());

    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh *mesh = scene->mMeshes[m];
        // vertices & uvs
        for (unsigned v = 0; v < mesh->mNumVertices; ++v)
        {
            aiVector3D p = mesh->mVertices[v];
            vertices.push_back(p.x);
            vertices.push_back(p.y);
            vertices.push_back(p.z);

            minB = minB.cwiseMin(Eigen::Vector3f(p.x, p.y, p.z));
            maxB = maxB.cwiseMax(Eigen::Vector3f(p.x, p.y, p.z));

            if (mesh->HasTextureCoords(0))
            {
                aiVector3D t = mesh->mTextureCoords[0][v];
                texcoords.push_back(t.x);
                texcoords.push_back(t.y);
            }
            else
            {
                texcoords.push_back(0.0f);
                texcoords.push_back(0.0f);
            }

            weights.push_back(1.0f); // neutral per-vertex weight
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
    }

    if (outCenter || outRadius)
    {
        Eigen::Vector3f c = 0.5f * (minB + maxB);
        if (outCenter)
        {
            outCenter->x = c.x();
            outCenter->y = c.y();
            outCenter->z = c.z();
        }
        if (outRadius)
        {
            float r = (maxB - c).norm();
            *outRadius = r;
        }
    }

    return true;
}

// Try to load the first diffuse texture in the first material.
// Returns empty Mat if not found.
static cv::Mat TryLoadDiffuseTexture(const std::string &modelPath)
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

// Build a simple pinhole camera. Tweak to your liking.
// NOTE: Adjust to your Camera struct (fx, fy, cx, cy, width, height).
static Camera<float> MakeCamera(unsigned w, unsigned h, float fov_deg = 60.0f)
{
    float fx = 0.5f * w / std::tan(0.5f * fov_deg * float(M_PI / 180.0));
    float fy = fx;
    float cx = 0.5f * (w - 1);
    float cy = 0.5f * (h - 1);
    Camera<float> cam(fx, fy, cx, cy, w, h);
    return cam;
}

linalg::Vec3<float> EigenToLinalg(const Eigen::Vector3f &v)
{
    return linalg::Vec3<float>(v.x(), v.y(), v.z());
}

linalg::Mat3<float> EigenToLinalg(const Eigen::Matrix3f &m)
{
    linalg::Mat3<float> lm;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            lm(r, c) = m(r, c);
    return lm;
}

// Construct an SE3 pose from R, t.
// NOTE: If your SE3 is Sophus::SE3f, this matches SE3f(R, t).
static linalg::SE3<float> MakePose(const Eigen::Matrix3f &R, const Eigen::Vector3f &t)
{
    return linalg::SE3<float>(EigenToLinalg(R), EigenToLinalg(t));
}

// ---------- main demo ----------

int main(int argc, char **argv)
{
    // Usage: demo_assimp <xclbin> <device_id> <model_path> [texture_override_path]
    if (argc < 2 || argc > 3)
    {
        std::cout << "please provide: model_path [texture_override_path]" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];
    std::string tex_override = (argc == 2) ? argv[1] : "";

    // Choose render size & camera
    const unsigned int w = 1280;
    const unsigned int h = 720;
    Camera<float> cam = MakeCamera(w, h, 55.0f);

    // Load mesh via Assimp
    std::vector<float> vertices, texcoords, weights;
    std::vector<unsigned int> indices;
    aiVector3D center;
    float radius = 1.0f;
    if (!LoadAssimpMesh(model_path, vertices, texcoords, weights, indices, &center, &radius))
    {
        return 1;
    }
    std::cout << "Model loaded. Vertices: " << (vertices.size() / 3)
              << "  Tris: " << (indices.size() / 3) << std::endl;

    // Load texture (model diffuse or override or checkerboard)
    cv::Mat tex_bgr;
    if (!tex_override.empty())
    {
        tex_bgr = cv::imread(tex_override, cv::IMREAD_COLOR);
    }
    if (tex_bgr.empty())
    {
        tex_bgr = TryLoadDiffuseTexture(model_path);
    }
    if (tex_bgr.empty())
    {
        tex_bgr = MakeCheckerTex(1024, 1024, 32);
    }
    cv::Mat tex_gray_f = ToGrayFloat01(tex_bgr);

    // Init XRT backend
    // if (!InitXRT(xclbin_file, device_index))
    //{
    //    std::cout << "Error initializing xrt backend!" << std::endl;
    //    return 1;
    //}
    // std::cout << "Initializing xrt backend ok" << std::endl;

    // Renderer + device resources
    const int in_lvl = 0;
    const int out_lvl = 0;

    ImageRendererCPU renderer;

    MeshCPU mesh(vertices, texcoords, weights, indices);

    TextureCPU<float> input(w, h, -1.0f);
    TextureCPU<float> output(w, h, -1.0f);

    // Upload texture (resized to render size for simplicity)
    cv::Mat tex_resized;
    cv::resize(tex_gray_f, tex_resized, cv::Size(w, h), 0, 0, cv::INTER_AREA);
    UploadMatToTexture(input, in_lvl, tex_resized);

    // Turntable loop
    const int max_frames = 600; // ~20 seconds at 30 FPS
    std::vector<double> times;
    times.reserve(max_frames);

    cv::namedWindow("RasterizerXRT", cv::WINDOW_AUTOSIZE);

    // Distance so model fits view
    float dist = 2.8f * radius;
    Eigen::Vector3f modelCenter(center.x, center.y, center.z);

    for (int i = 0; i < max_frames; ++i)
    {
        float t = float(i) * 0.016f; // ~60deg/s at 60fps for yaw
        Eigen::Matrix3f R =
            (Eigen::AngleAxisf(0.15f * t, Eigen::Vector3f::UnitX()) *
             Eigen::AngleAxisf(0.6f * t, Eigen::Vector3f::UnitY()))
                .toRotationMatrix();

        // Camera looks at modelCenter from +Z at distance 'dist'
        Eigen::Vector3f camPos = modelCenter + R * Eigen::Vector3f(0, 0, dist);

        // Build world->camera SE3 (view). If your Render expects src->dst pose, adapt accordingly.
        Eigen::Matrix3f Rc = R.transpose(); // looking-at rotation
        Eigen::Vector3f tc = -Rc * camPos;

        linalg::SE3<float> pose_transform = MakePose(Rc, tc);

        auto t0 = std::chrono::high_resolution_clock::now();
        renderer.Render(mesh, pose_transform, cam, in_lvl, out_lvl, input, output);
        auto t1 = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        times.push_back(ms);

        cv::Mat out_f = DownloadTextureToMat(output, out_lvl, CV_32FC1);

        // Pretty up the single-channel output
        cv::Mat out_u8, out_color;
        out_f.convertTo(out_u8, CV_8U, 255.0);
        cv::applyColorMap(out_u8, out_color, cv::COLORMAP_TURBO);

        // Overlay FPS
        double avg = std::accumulate(times.begin(), times.end(), 0.0) / (double)times.size();
        double fps = (avg > 1e-6) ? (1000.0 / avg) : 0.0;
        cv::putText(out_color,
                    "Frame " + std::to_string(i) + "  " + std::to_string(ms) + " ms  (" + std::to_string(fps) + " fps avg)",
                    cv::Point(18, 32), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2, cv::LINE_AA);

        cv::imshow("RasterizerCPU", out_color);
        if (i % 60 == 0)
        {
            SaveDebugImage(out_color, "rasterizerxrt_frame_" + std::to_string(i) + ".png");
        }

        int key = cv::waitKey(1);
        if (key == 27 || key == 'q')
            break;
    }

    // Stats
    std::sort(times.begin(), times.end());
    double sum = std::accumulate(times.begin(), times.end(), 0.0);
    double avg = (times.empty() ? 0.0 : sum / times.size());
    double med = (times.empty() ? 0.0 : times[times.size() / 2]);
    double minv = (times.empty() ? 0.0 : times.front());
    double maxv = (times.empty() ? 0.0 : times.back());

    std::cout << "CPU Rasterizer Performance:\n";
    std::cout << "  Frames:  " << times.size() << "\n";
    std::cout << "  Average: " << avg << " ms\n";
    std::cout << "  Median:  " << med << " ms\n";
    std::cout << "  Min:     " << minv << " ms\n";
    std::cout << "  Max:     " << maxv << " ms\n";

    return 0;
}
