#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

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
    std::vector<float> vertex;
    std::vector<unsigned int> indices;
    std::vector<std::string> textures;
    bool has_positions, has_texcoords, has_normals;

    if (!LoadAssimpMesh(model_path, vertex, indices, textures, has_positions, has_texcoords, has_normals))
    {
        return 1;
    }

    std::cout << "Model loaded. Vertices: " << (vertex.size() / 3)
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

    DiffRendererCPU renderercpu;
    DiffRendererGL renderergl;

    MeshCPU meshcpu(vertex, indices, diffuse_cv, has_positions, has_texcoords, has_normals);
    MeshGL meshgl(vertex, indices, diffuse_cv, has_positions, has_texcoords, has_normals);

    TextureCPU<float> imagecpu(width, height, -1.0f);
    TextureCPU<float> depthcpu(width, height, -1.0f);
    TextureCPU<linalg::Vec3<float>> jtracpu(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> jrotcpu(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> jmapcpu(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> pidscpu(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));

    TextureGL<float> imagegl(width, height, -1.0f);
    TextureGL<float> depthgl(width, height, -1.0f);
    TextureGL<linalg::Vec3<float>> jtragl(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<linalg::Vec3<float>> jrotgl(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<linalg::Vec3<float>> jmapgl(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<linalg::Vec3<float>> pidsgl(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));

    // Turntable loop
    const int max_frames = 600; // ~20 seconds at 30 FPS
    std::vector<double> times;
    times.reserve(max_frames);

    cv::namedWindow("Rasterizer Demo", cv::WINDOW_AUTOSIZE);

    float dist = -5.5f;
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

    std::vector<std::string> backend_names;
    backend_names.push_back("cpu");
    backend_names.push_back("gl");

    // for (int i = 0; i < max_frames; ++i)
    int i = 0;
    int toshow = 0;
    int backend = 0;
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
        if (backend == 0)
            // renderercpu.Render(meshcpu, transform, camera, in_lvl, out_lvl, imagecpu);
            renderercpu.Render(meshcpu,
                               transform,
                               camera,
                               in_lvl,
                               out_lvl,
                               imagecpu,
                               depthcpu,
                               jtracpu,
                               jrotcpu,
                               jmapcpu,
                               pidscpu);
        if (backend == 1)
            // renderergl.Render(meshgl, transform, camera, in_lvl, out_lvl, imagegl);
            renderergl.Render(meshgl,
                              transform,
                              camera,
                              in_lvl,
                              out_lvl,
                              imagegl,
                              depthgl,
                              jtragl,
                              jrotgl,
                              jmapgl,
                              pidsgl);

        auto t1 = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        times.push_back(ms);

        cv::Mat out_f;
        if (toshow == 0)
            if (backend == 0)
                out_f = DownloadTextureToMat(imagecpu, out_lvl, CV_32FC1);
            else
                out_f = DownloadTextureToMat(imagegl, out_lvl, CV_32FC1);
        if (toshow == 1)
            if (backend == 0)
                out_f = DownloadTextureToMat(depthcpu, out_lvl, CV_32FC1);
            else
                out_f = DownloadTextureToMat(depthgl, out_lvl, CV_32FC1);
        if (toshow == 2)
            if (backend == 0)
                out_f = DownloadTextureToMat(jtracpu, out_lvl, CV_32FC3);
            else
                out_f = DownloadTextureToMat(jtragl, out_lvl, CV_32FC3);
        if (toshow == 3)
            if (backend == 0)
                out_f = DownloadTextureToMat(jrotcpu, out_lvl, CV_32FC3);
            else
                out_f = DownloadTextureToMat(jrotgl, out_lvl, CV_32FC3);
        if (toshow == 4)
            if (backend == 0)
                out_f = DownloadTextureToMat(jmapcpu, out_lvl, CV_32FC3);
            else
                out_f = DownloadTextureToMat(jmapgl, out_lvl, CV_32FC3);
        if (toshow == 5)
            if (backend == 0)
                out_f = DownloadTextureToMat(pidscpu, out_lvl, CV_32FC3);
            else
                out_f = DownloadTextureToMat(pidsgl, out_lvl, CV_32FC3);

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
                    backend_names[backend] + "  " + texture_names[toshow] + " frame " + std::to_string(i) + "  " + std::to_string(ms) + " ms  (" + std::to_string(fps) + " fps avg)",
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
        if (key == 'z')
        {
            backend++;
            backend %= 2;
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
