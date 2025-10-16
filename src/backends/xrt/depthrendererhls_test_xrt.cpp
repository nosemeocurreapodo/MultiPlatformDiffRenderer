#include <chrono>

#include <opencv2/opencv.hpp>

#include "loaddataset.h"
#include "core/common.h"
#include "common/test_helpers.h"
#include "core/format_converters.h"
// #include "core/types.h"
//  #include "backends/cpu/buffercpu.h"
//  #include "backends/cpu/texturecpu.h"
//  #include "backends/cpu/meshcpu.h"
#include "backends/xrt/devicexrt.h"
#include "backends/xrt/bufferxrt.h"
#include "backends/xrt/texturexrt.h"
#include "backends/xrt/meshxrt.h"
#include "backends/xrt/rendererxrt.h"

int main(int argc, char **argv)
{
    if (argc < 4 || argc > 4)
    {
        std::cout << "please provide: xclbin_file device_id dataset_path" << std::endl;
        return 1;
    }

    // Read settings
    std::string xclbin_file = argv[1];
    int device_index = atoi(argv[2]);
    std::string dataset_path = argv[3];

    LoadDatasetIclNuim dataset(dataset_path);
    // LoadDatasetTumRgbd dataset;

    std::vector<std::string> image_files = dataset.GetImageFiles();
    std::vector<std::string> depth_files = dataset.GetDepthFiles();
    std::vector<linalg::SE3<float>> poses = dataset.GetPoses();
    float depth_factor = dataset.GetDepthFactor();
    // std::vector<double> timestamps = dataset.GetTimestamps();
    Camera<float> cam = dataset.GetCamera();
    unsigned int w = dataset.GetWidth();
    unsigned int h = dataset.GetHeight();

    float scale = 1.0f / depth_factor;

    cv::Mat image_src_cv = ReadMat(image_files[0]);
    cv::Mat depth_src_cv = ReadMat(depth_files[0]) * scale;
    linalg::SE3<float> pose_src = poses[0];

    // TextureXRT<float> image_src_cpu(w, h, -1.0f);
    TextureCPU<float> depth_src_cpu(w, h, -1.0f);

    // UploadMatToTexture(image_src_cpu, 0, image_src_cv);
    UploadMatToTexture(depth_src_cpu, 0, depth_src_cv);

    std::vector<float> vertices, texcoords, weights;
    std::vector<unsigned int> indices;
    CreateMesh(depth_src_cpu, cam, 32, vertices, texcoords, weights, indices);

    // std::vector<float> screen_vertices, screen_texcoords, screen_weights;
    // std::vector<unsigned int> screen_indices;
    // CreateScreenQuad(screen_vertices, screen_texcoords, screen_weights, screen_indices);

    if (!InitXRT(xclbin_file, device_index))
    {
        std::cout << "Error initializing xrt backend!" << std::endl;
        return 1;
    }

    std::cout << "Initializing xrt backend ok" << std::endl;

    const int iterations = 100;
    const int out_lvl = 1;

    DepthRendererXRT renderer;
    // TestRendererXRT renderer;

    MeshXRT mesh(vertices, texcoords, weights, indices,
                 renderer.kernel_.group_id(0), renderer.kernel_.group_id(1), renderer.kernel_.group_id(2), renderer.kernel_.group_id(3));

    TextureXRT<float> output(w, h, -1.0f, renderer.kernel_.group_id(4));
    // UploadMatToTexture(output, 0, image_src_cv);

    std::vector<double> times;
    times.reserve(iterations);

    for (int i = 0; i < iterations; ++i)
    {
        std::cout << "Iteration " << i << std::endl;

        // cv::Mat image_dst_cv = ReadMat(image_files[1]);
        // cv::Mat depth_dst_cv = ReadMat(depth_files[1]) * scale;
        linalg::SE3<float> pose_dst = poses[i];

        linalg::SE3<float> pose_transform = pose_dst * pose_src.inverse();

        auto t0 = std::chrono::high_resolution_clock::now();
        renderer.Render(mesh, pose_transform, cam, out_lvl, output);
        auto t1 = std::chrono::high_resolution_clock::now();
        double time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        times.push_back(time_ms);

        cv::Mat depth_out_cv = DownloadTextureToMat(output, out_lvl, CV_32FC1);

        // double depthError = ComputeImageError<float>(depth_dst_CV, output_depthCV, -1.0f);
        SaveDebugImage(depth_out_cv, "depthrenderhls_output_" + std::to_string(i) + ".png");
    }

    // Calculate statistics
    double total_time = 0.0;
    for (double t : times)
        total_time += t;
    double avg_time = total_time / iterations;

    std::sort(times.begin(), times.end());
    double median_time = times[iterations / 2];
    double min_time = times[0];
    double max_time = times[iterations - 1];

    std::cout << "CPU Depth Renderer Performance:\n";
    std::cout << "  Average: " << avg_time << " ms\n";
    std::cout << "  Median:  " << median_time << " ms\n";
    std::cout << "  Min:     " << min_time << " ms\n";
    std::cout << "  Max:     " << max_time << " ms\n";

    return 0;
}
