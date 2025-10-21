#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include "core/common.h"
#include "common/test_helpers.h"
#include "../simple_example/model.h"
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

template <typename T>
using Texture = TextureCPU<T>;
using Mesh = MeshCPU;
using ImageRenderer = DiffRendererCPU;

int main(int argc, char **argv)
{
    // Usage: demo_assimp <xclbin> <device_id> <model_path> [texture_override_path]
    if (argc < 2 || argc > 3)
    {
        std::cout << "please provide: model_path [texture_override_path]" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];
    std::string tex_override = (argc == 3) ? argv[2] : "";

    // Choose render size & camera
    const unsigned int width = 1280;
    const unsigned int height = 720;

    // Load mesh via Assimp
    std::vector<Eigen::Vector3f> vertices, normals;
    std::vector<Eigen::Vector2f> texcoords;
    std::vector<unsigned int> indices;
    std::vector<std::string> textures;

    if (!LoadAssimpMesh(model_path, vertices, texcoords, normals, indices, textures))
    {
        return 1;
    }

    std::cout << "Model loaded. Vertices: " << (vertices.size() / 3)
              << "  Tris: " << (indices.size() / 3) << std::endl;

    if (!InitEGL())
    {
        std::cout << "Error initializing gl backend!" << std::endl;
        return 1;
    }

    // Load texture (model diffuse or override or checkerboard)
    cv::Mat diffuse_cv;
    if (!textures.empty())
    {
        diffuse_cv = cv::imread(textures[0], cv::IMREAD_GRAYSCALE);
        diffuse_cv.convertTo(diffuse_cv, CV_32F, 1.0 / 255.0);
    }
    // if (diffuse.empty())
    // {
    //    diffuse = TryLoadDiffuseTexture(model_path);
    // }
    if (diffuse_cv.empty())
    {
        diffuse_cv = MakeCheckerTex(1024, 1024, 32);
    }

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

    ImageRenderer renderer;

    Mesh mesh(vertices, texcoords, normals, indices, diffuse_cv);

    Texture<float> image(width, height, -1.0f);
    Texture<float> depth(width, height, -1.0f);
    Texture<linalg::Vec3<float>> jtra(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    Texture<linalg::Vec3<float>> jrot(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    Texture<linalg::Vec3<float>> jmap(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    Texture<linalg::Vec3<float>> pids(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));

    // Turntable loop
    const int max_frames = 600; // ~20 seconds at 30 FPS
    std::vector<double> times;
    times.reserve(max_frames);

    cv::namedWindow("Rasterizer Demo", cv::WINDOW_AUTOSIZE);

    float dist = -1.5f;
    float fov_deg = 90.0f;

    float fx = 0.5f * width / std::tan(0.5f * fov_deg * float(M_PI / 180.0));
    float fy = fx;
    float cx = 0.5f * (width - 1);
    float cy = 0.5f * (height - 1);

    Camera<float> camera(fx, fy, cx, cy, width, height);

    std::vector<std::string> texture_names;
    texture_names.push_back("image");
    texture_names.push_back("depth");
    texture_names.push_back("jtra");
    texture_names.push_back("jrot");
    texture_names.push_back("jmap");
    texture_names.push_back("pids");

    // for (int i = 0; i < max_frames; ++i)
    int i = 0;
    int toshow = 0;
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

        auto t0 = std::chrono::high_resolution_clock::now();
        // renderer.Render(mesh, transform, camera, in_lvl, out_lvl, output);
        renderer.Render(mesh,
                        transform,
                        camera,
                        in_lvl,
                        out_lvl,
                        image,
                        depth,
                        jtra,
                        jrot,
                        jmap,
                        pids);

        auto t1 = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        times.push_back(ms);

        cv::Mat out_f;
        if (toshow == 0)
            out_f = DownloadTextureToMat(image, out_lvl, CV_32FC1);
        if (toshow == 1)
            out_f = DownloadTextureToMat(depth, out_lvl, CV_32FC1);
        if (toshow == 2)
            out_f = DownloadTextureToMat(jtra, out_lvl, CV_32FC3);
        if (toshow == 3)
            out_f = DownloadTextureToMat(jrot, out_lvl, CV_32FC3);
        if (toshow == 4)
            out_f = DownloadTextureToMat(jmap, out_lvl, CV_32FC3);
        if (toshow == 5)
            out_f = DownloadTextureToMat(pids, out_lvl, CV_32FC3);

        // Pretty up the single-channel output
        cv::Mat out_u8, out_color;
        out_f.convertTo(out_u8, CV_8U, 255.0);
        out_color = out_u8;
        // cv::applyColorMap(out_u8, out_color, cv::COLORMAP_TURBO);

        // Overlay FPS
        double avg = 0.0;
        for (auto time : times)
        {
            avg += time;
        }
        avg /= times.size();
        // double avg = std::accumulate(times.begin(), times.end(), 0.0) / (double)times.size();
        double fps = (avg > 1e-6) ? (1000.0 / avg) : 0.0;
        cv::putText(out_color,
                    "Frame " + std::to_string(i) + "  " + texture_names[toshow] + "  " + std::to_string(ms) + " ms  (" + std::to_string(fps) + " fps avg)",
                    cv::Point(18, 32), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2, cv::LINE_AA);

        cv::imshow("Rasterizer Demo", out_color);
        // if (i % 60 == 0)
        //{
        //     SaveDebugImage(out_color, "rasterizerdemo_frame_" + std::to_string(i) + ".png");
        // }

        int key = cv::waitKey(1);
        if (key == 27 || key == 'q')
            break;
        if (key == 'a')
        {
            toshow++;
            toshow %= 6;
        }
    }

    // Stats
    std::sort(times.begin(), times.end());
    // double sum = std::accumulate(times.begin(), times.end(), 0.0);
    double sum = 0.0;
    for (auto time : times)
    {
        sum += time;
    }
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
