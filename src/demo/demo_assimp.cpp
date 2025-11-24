#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include "tests/common/test_helpers.h"
#include "core/mesh_helpers.h"
#include "core/types.h"
#include "linalg/converters.h"
#include "model.h"

#ifdef COMPILE_CPU
// #include "backends/cpu/devicecpu.h"
#include "backends/cpu/buffercpu.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/meshcpu.h"
#include "backends/cpu/renderercpu.h"
#endif

#ifdef COMPILE_GL
#include "backends/gl/devicegl_glad.h"
#include "backends/gl/texturegl.h"
#include "backends/gl/meshgl.h"
#include "backends/gl/renderergl.h"
#endif

#ifdef COMPILE_GLES2
#include "backends/gles2/devicegles2_glad.h"
#include "backends/gles2/texturegles2.h"
#include "backends/gles2/meshgles2.h"
#include "backends/gles2/renderergles2.h"
#endif

#ifdef COMPILE_XRT
#include "backends/xrt/devicexrt.h"
#include "backends/xrt/texturexrt.h"
#include "backends/xrt/meshxrt.h"
#include "backends/xrt/rendererxrt.h"
#endif

int main(int argc, char **argv)
{
// Usage: demo_assimp <xclbin> <device_id> <model_path> [texture_override_path]
#ifndef COMPILE_XRT
    if (argc != 2)
    {
        std::cout << "please provide: model_path" << std::endl;
        return 1;
    }
#else
    if (argc != 4)
    {
        std::cout << "please provide: model_path xclbin device_id" << std::endl;
        return 1;
    }
#endif

    std::string model_path = argv[1];

    // Choose render size & camera
    const unsigned int width = 640;
    const unsigned int height = 480;

    // Load mesh via Assimp
    std::vector<float> vertex;
    std::vector<int> indices;
    std::vector<std::string> textures;
    bool has_positions, has_texcoords, has_normals;

    if (!LoadAssimpMesh(model_path, vertex, indices, textures, has_positions, has_texcoords, has_normals))
    {
        return 1;
    }

    std::cout << "Model loaded. Vertices: " << (vertex.size() / 3)
              << "  Tris: " << (indices.size() / 3) << std::endl;

    std::vector<float> screen_vertex;
    std::vector<int> screen_indices;
    CreateScreenQuad(screen_vertex, screen_indices);

    std::vector<std::string> backend_names;

#ifdef COMPILE_CPU
    backend_names.push_back("cpu");
#endif

#ifdef COMPILE_GL
    if (!InitEGL())
    {
        std::cout << "Error initializing gl backend!" << std::endl;
        return 1;
    }
    else
    {
        std::cout << "Initializing gl backend!" << std::endl;
    }
    backend_names.push_back("gl");
#endif

#ifdef COMPILE_GLES2
    if (!InitEGL_GLES2())
    {
        std::cout << "Error initializing gles2 backend!" << std::endl;
        return 1;
    }
    else
    {
        std::cout << "Initializing gles2 backend!" << std::endl;
    }
    backend_names.push_back("gles2");
#endif

#ifdef COMPILE_XRT
    std::string xclbin_file = argv[2];
    int device_index = atoi(argv[3]);

    std::cout << "loading xclbin: " << xclbin_file << std::endl;
    std::cout << "loading device: " << device_index << std::endl;

    if (!InitXRT(xclbin_file, device_index))
    {
        std::cout << "Error initializing xrt backend!" << std::endl;
        return 1;
    }
    else
    {
        std::cout << "Initializing xrt backend!" << std::endl;
    }
    backend_names.push_back("xrt");
#endif

    // Load texture (model diffuse or override or checkerboard)
    cv::Mat diffuse_cv;
    if (!textures.empty())
    {
        diffuse_cv = cv::imread(textures[0], cv::IMREAD_GRAYSCALE);
    }
    // if (diffuse.empty())
    // {
    //    diffuse = TryLoadDiffuseTexture(model_path);
    // }
    if (diffuse_cv.empty())
    {
        diffuse_cv = MakeCheckerTex(1024, 1024, 32);
    }

    // Renderer + device resources
    const int in_lvl = 2;
    const int out_lvl = 0;

#ifdef COMPILE_CPU
    DiffRendererCPU renderercpu;
    DIDxyRendererCPU didxyrenderercpu;

    MeshCPU meshcpu(vertex, indices, has_positions, has_texcoords, has_normals);

    TextureCPU<ImageType> diffusecpu(diffuse_cv.cols, diffuse_cv.rows, 0);
    UploadMatToTexture(diffusecpu, 0, diffuse_cv);

    TextureCPU<Vec3<float>> didxycpu(diffuse_cv.cols, diffuse_cv.rows, Vec3<float>(0, 0, 0));
    MeshCPU meshcpu_screen(screen_vertex, screen_indices, true, true, false);
    didxyrenderercpu.Render(meshcpu_screen, in_lvl, in_lvl, diffusecpu, didxycpu);

    TextureCPU<ImageType> imagecpu(width, height, 0);
    TextureCPU<float> depthcpu(width, height, -1.0f);
    TextureCPU<Vec3<float>> jtracpu(width, height, Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3<float>> jrotcpu(width, height, Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3<float>> jmapcpu(width, height, Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3<PidType>> pidscpu(width, height, Vec3<PidType>(-1, -1, -1));
#endif

#ifdef COMPILE_GL
    DiffRendererGL renderergl;
    DIDxyRendererGL didxyrenderergl;

    MeshGL meshgl(vertex, indices, has_positions, has_texcoords, has_normals);

    TextureGL<ImageType> diffusegl(diffuse_cv.cols, diffuse_cv.rows, 0);
    UploadMatToTexture(diffusegl, 0, diffuse_cv);

    TextureGL<Vec3<float>> didxygl(diffuse_cv.cols, diffuse_cv.rows, Vec3<float>(0, 0, 0));
    MeshGL meshgl_screen(screen_vertex, screen_indices, true, true, false);
    didxyrenderergl.Render(meshgl_screen, in_lvl, in_lvl, diffusegl, didxygl);

    TextureGL<ImageType> imagegl(width, height, 0);
    TextureGL<float> depthgl(width, height, -1.0f);
    TextureGL<Vec3<float>> jtragl(width, height, Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<Vec3<float>> jrotgl(width, height, Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<Vec3<float>> jmapgl(width, height, Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<Vec3<PidType>> pidsgl(width, height, Vec3<PidType>(-1, -1, -1));
#endif

#ifdef COMPILE_GLES2
    ImageRendererGLES2 renderergles2;

    MeshGLES2 meshgles2(vertex, indices, has_positions, has_texcoords, has_normals);

    TextureGLES2<ImageType> diffusegles(diffuse_cv.cols, diffuse_cv.rows, 0);
    UploadMatToTexture(diffusegles, 0, diffuse_cv);

    TextureGLES2<ImageType> imagegles2(width, height, 0);
    TextureGLES2<float> depthgles2(width, height, -1.0f);
    TextureGLES2<Vec3<float>> jtragles2(width, height, Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGLES2<Vec3<float>> jrotgles2(width, height, Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGLES2<Vec3<float>> jmapgles2(width, height, Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGLES2<Vec3<PidType>> pidsgles2(width, height, Vec3<PidType>(-1, -1, -1));
#endif

#ifdef COMPILE_XRT
    ImageRendererXRT rendererxrt;

    MeshXRT meshxrt(vertex, indices,
                    has_positions, has_texcoords, has_normals,
                    rendererxrt.kernel_.group_id(0), rendererxrt.kernel_.group_id(1));

    TextureXRT<ImageType> diffusexrt(diffuse_cv.cols, diffuse_cv.rows, 0, rendererxrt.kernel_.group_id(2));
    UploadMatToTexture(diffusexrt, 0, diffuse_cv);

    TextureXRT<ImageType> imagexrt(width, height, 0, rendererxrt.kernel_.group_id(6));
    // TextureXRT<linalg::Vec3<float>> jtraxrt(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    // TextureXRT<linalg::Vec3<float>> jrotxrt(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    // TextureXRT<linalg::Vec3<float>> jmapxrt(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    // TextureXRT<linalg::Vec3<float>> pidsxrt(width, height, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
#endif

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

    PinholeCamera<float> camera(fx, fy, cx, cy, width, height);

    std::vector<std::string> output_names;
    output_names.push_back("image");
    output_names.push_back("depth");
    output_names.push_back("jtra");
    output_names.push_back("jrot");
    output_names.push_back("jmap");
    output_names.push_back("pids");

    // for (int i = 0; i < max_frames; ++i)
    int i = 0;
    int toshow = 0;
    int backend = 0;
    while (true)
    {
        i++;
        float r = float(i) * M_PI / 1000.0f; // ~60deg/s at 60fps for yaw
        if (r > M_PI * 2.0f)
            i = 0;
        Eigen::Matrix3f R =
            Eigen::AngleAxisf(r, Eigen::Vector3f::UnitY())
                //     Eigen::AngleAxisf(M_PI / 2.0f, Eigen::Vector3f::UnitX())*/
                //        /*(Eigen::AngleAxisf(0.6f * t, Eigen::Vector3f::UnitY()))*/
                .toRotationMatrix();

        Eigen::Vector3f camPos = R.transpose() * Eigen::Vector3f(0, 0, dist);

        Eigen::Matrix4f view_e_;
        view_e_.block<3, 3>(0, 0) = R.transpose();
        view_e_.block<3, 1>(0, 3) = -R * camPos;
        view_e_.row(3) = Eigen::Vector4f(0, 0, 0, 1);

        Mat4<float> v_ = EigenToLinalg(view_e_);

        SE3<float> transform(v_);

        auto t0 = std::chrono::high_resolution_clock::now();
#ifdef COMPILE_CPU
        if (backend_names[backend] == "cpu")
            // renderercpu.Render(meshcpu, transform, camera, in_lvl, out_lvl, imagecpu);
            renderercpu.Render(meshcpu,
                               transform,
                               camera,
                               in_lvl,
                               out_lvl,
                               diffusecpu,
                               didxycpu,
                               imagecpu,
                               depthcpu,
                               jtracpu,
                               jrotcpu,
                               jmapcpu,
                               pidscpu);
#endif
#ifdef COMPILE_GL
        if (backend_names[backend] == "gl")
            // renderergl.Render(meshgl, transform, camera, in_lvl, out_lvl, imagegl);
            renderergl.Render(meshgl,
                              transform,
                              camera,
                              in_lvl,
                              out_lvl,
                              diffusegl,
                              didxygl,
                              imagegl,
                              depthgl,
                              jtragl,
                              jrotgl,
                              jmapgl,
                              pidsgl);
#endif

#ifdef COMPILE_GLES2
        if (backend_names[backend] == "gles2")
            renderergles2.Render(meshgles2, transform, camera, in_lvl, out_lvl, diffusegles2, imagegles2);

#endif

#ifdef COMPILE_XRT
        if (backend_names[backend] == "xrt")
            rendererxrt.Render(meshxrt, transform, camera, in_lvl, out_lvl, diffusexrt, imagexrt);

#endif

        auto t1 = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        times.push_back(ms);

        cv::Mat out_f;
        if (output_names[toshow] == "image")
        {
#ifdef COMPILE_CPU
            if (backend_names[backend] == "cpu")
                out_f = DownloadTextureToMat(imagecpu, out_lvl);
#endif
#ifdef COMPILE_GL
            if (backend_names[backend] == "gl")
                out_f = DownloadTextureToMat(imagegl, out_lvl);
#endif
#ifdef COMPILE_GLES2
            if (backend_names[backend] == "gles2")
                out_f = DownloadTextureToMat(imagegles2, out_lvl);
#endif
#ifdef COMPILE_XRT
            if (backend_names[backend] == "xrt")
                out_f = DownloadTextureToMat(imagexrt, out_lvl);
#endif
        }

        if (output_names[toshow] == "depth")
        {
#ifdef COMPILE_CPU
            if (backend_names[backend] == "cpu")
                out_f = DownloadTextureToMat(depthcpu, out_lvl);
#endif
#ifdef COMPILE_GL
            if (backend_names[backend] == "gl")
                out_f = DownloadTextureToMat(depthgl, out_lvl);
#endif
#ifdef COMPILE_GLES2
            if (backend_names[backend] == "gles2")
                out_f = DownloadTextureToMat(depthgles2, out_lvl);
#endif
            // #ifdef COMPILE_XRT
            //             if (backend_names[backend] == "xrt")
            //                out_f = DownloadTextureToMat(depthxrt, out_lvl);
            // #endif
        }

        if (output_names[toshow] == "jtra")
        {
#ifdef COMPILE_CPU
            if (backend_names[backend] == "cpu")
                out_f = DownloadTextureToMat(jtracpu, out_lvl);
#endif
#ifdef COMPILE_GL
            if (backend_names[backend] == "gl")
                out_f = DownloadTextureToMat(jtragl, out_lvl);
#endif
#ifdef COMPILE_GLES2
            if (backend_names[backend] == "gles2")
                out_f = DownloadTextureToMat(jtragles2, out_lvl);
#endif
            // #ifdef COMPILE_XRT
            //             if (backend_names[backend] == "xrt")
            //                 out_f = DownloadTextureToMat(jtraxrt, out_lvl);
            // #endif
        }

        if (output_names[toshow] == "jrot")
        {
#ifdef COMPILE_CPU
            if (backend_names[backend] == "cpu")
                out_f = DownloadTextureToMat(jrotcpu, out_lvl);
#endif
#ifdef COMPILE_GL
            if (backend_names[backend] == "gl")
                out_f = DownloadTextureToMat(jrotgl, out_lvl);
#endif
#ifdef COMPILE_GLES2
            if (backend_names[backend] == "gles2")
                out_f = DownloadTextureToMat(jrotgles2, out_lvl);
#endif
            // #ifdef COMPILE_XRT
            //             if (backend_names[backend] == "xrt")
            //                 out_f = DownloadTextureToMat(jrotxrt, out_lvl);
            // #endif
        }

        if (output_names[toshow] == "jmap")
        {
#ifdef COMPILE_CPU
            if (backend_names[backend] == "cpu")
                out_f = DownloadTextureToMat(jmapcpu, out_lvl);
#endif
#ifdef COMPILE_GL
            if (backend_names[backend] == "gl")
                out_f = DownloadTextureToMat(jmapgl, out_lvl);
#endif
#ifdef COMPILE_GLES2
            if (backend_names[backend] == "gles2")
                out_f = DownloadTextureToMat(jmapgles2, out_lvl);
#endif
            // #ifdef COMPILE_XRT
            //             if (backend_names[backend] == "xrt")
            //                 out_f = DownloadTextureToMat(jmapxrt, out_lvl);
            // #endif
        }

        if (output_names[toshow] == "pids")
        {
#ifdef COMPILE_CPU
            if (backend_names[backend] == "cpu")
                out_f = DownloadTextureToMat(pidscpu, out_lvl);
#endif
#ifdef COMPILE_GL
            if (backend_names[backend] == "gl")
                out_f = DownloadTextureToMat(pidsgl, out_lvl);
#endif
#ifdef COMPILE_GLES2
            if (backend_names[backend] == "gles2")
                out_f = DownloadTextureToMat(pidsgles2, out_lvl);
#endif
            // #ifdef COMPILE_XRT
            //             if (backend_names[backend] == "xrt")
            //                 out_f = DownloadTextureToMat(pidsxrt, out_lvl);
            // #endif
        }

        // Pretty up the single-channel output
        cv::Mat out_norm, out_u8, out_color;
        cv::normalize(out_f, out_norm, 0, 255, cv::NORM_MINMAX);
        // normalized.convertTo(normalized, CV_8UC1);
        out_norm.convertTo(out_u8, CV_8U);
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
                    backend_names[backend] + "  " + output_names[toshow] + " frame " + std::to_string(i) + "  " + std::to_string(ms) + " ms  (" + std::to_string(fps) + " fps avg)",
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
            toshow %= output_names.size();
        }
        if (key == 'z')
        {
            backend++;
            backend %= backend_names.size();
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
