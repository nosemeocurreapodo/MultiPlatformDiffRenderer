#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include "common/test_helpers.h"
#include "model.h"
#include "linalg/converters.h"

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
using Texture = TextureGL<T>;
using Mesh = MeshGL;
using Renderer = ImageRendererGL;

// template <typename T>
// using Texture = TextureGL<T>;
// using Mesh = MeshGL;
// using Renderer = ImageRendererGL;

int main()
{
    std::vector<float> vertex;
    std::vector<unsigned int> indices;
    std::vector<std::string> textures;
    bool has_positions, has_texcoords, has_normals;

    // std::vector<float> vertices;
    // std::vector<float> normals;
    // std::vector<float> texcoords;
    // std::vector<unsigned int> indices;

    //std::string mesh_path = "/workspaces/MultiPlatformDiffRenderer/src/tests/data/cyborg/cyborg.obj";
    std::string mesh_path = "/workspaces/MultiPlatformDiffRenderer/src/tests/data/planet/planet.obj";
    // std::string mesh_path = "/workspaces/MultiPlatformDiffRenderer/tests/data/bunny/bun_zipper_res4.ply";

    if (!LoadAssimpMesh(mesh_path,
                        vertex,
                        indices,
                        textures,
                        has_positions,
                        has_texcoords,
                        has_normals))
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

    // Texture<float> in_texture(width, height, -1.0);
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

    // UploadMatToTexture(in_texture, 0, diffuse);

    // Mesh mesh(vertices, normals, texcoords, indices);
    Mesh mesh(vertex, indices, diffuse, has_positions, has_texcoords, has_normals);

    Renderer renderer;

    cv::namedWindow("Simple Example", cv::WINDOW_AUTOSIZE);

    // render loop
    // -----------
    float dist = -1.5f;

    float fov_deg = 90.0f;

    float fx = 0.5f * width / std::tan(0.5f * fov_deg * float(M_PI / 180.0));
    float fy = fx;
    float cx = 0.5f * (width - 1);
    float cy = 0.5f * (height - 1);

    Camera<float> camera1(fx, fy, cx, cy, width, height);

    // linalg::Mat4<float> p_ = camera1.GetProjectiveMatrix(0.1f, 100.0f);

    // linalg::SE3<float> view_l_;
    // view_l_.translation() = linalg::Vec3<float>(0.0f, 0.0f, dist);
    // linalg::Mat4<float> v_ = view_l_.matrix();

    // linalg::SE3<float> model_l_;
    // linalg::Mat4<float> m_ = model_l_.matrix();

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

        linalg::Mat4<float> v_ = EigenToLinalg(view_e_);

        linalg::SE3<float> transform(v_);

        // renderer.Render(mesh, p_, v_, m_, 0, 0, in_texture, out_texture);
        renderer.Render(mesh, transform, camera1, 0, 0, out_texture);

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
