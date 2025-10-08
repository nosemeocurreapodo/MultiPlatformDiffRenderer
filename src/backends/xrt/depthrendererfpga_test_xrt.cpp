#include <chrono>

#include <opencv2/opencv.hpp>

#include "loaddataset.h"
#include "core/format_converters.h"
#include "core/types.h"
#include "backends/cpu/buffercpu.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/meshcpu.h"
#include "backends/xrt/devicexrt.h"
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
    std::vector<SE3> poses = dataset.GetPoses();
    float depth_factor = dataset.GetDepthFactor();
    // std::vector<double> timestamps = dataset.GetTimestamps();
    Camera cam = dataset.GetCamera();
    unsigned int w = dataset.GetWidth();
    unsigned int h = dataset.GetHeight();

    float scale = 1.0f / depth_factor;

    cv::Mat image_src_cv = ReadMat(image_files[0]);
    cv::Mat depth_src_cv = ReadMat(depth_files[0]) * scale;
    SE3 pose_dst = poses[0];

    TextureCPU<float> image_src_cpu(w, h, -1.0f);
    TextureCPU<float> depth_src_cpu(w, h, -1.0f);

    UploadMatToTexture(image_src_cpu, 0, image_src_cv);
    UploadMatToTexture(depth_src_cpu, 0, depth_src_cv);

    std::vector<float> vertices, texcoords, weights;
    std::vector<unsigned int> indices;
    CreateMesh(depth_src_cpu, cam, 32, vertices, texcoords, weights, indices);

    std::vector<float> screen_vertices, screen_texcoords, screen_weights;
    std::vector<unsigned int> screen_indices;
    CreateScreenQuad(screen_vertices, screen_texcoords, screen_weights, screen_indices);

    if (!InitXRT(xclbin_file, device_index))
    {
        std::cout << "Error initializing xrt backend!" << std::endl;
        return 1;
    }

    const int iterations = 10;
    const int in_lvl = 0, out_lvl = 0;

    MeshXRT mesh(vertices_, texcoords_, weights_, indices_);
    TextureXRT<float> input(w_, h_, 0.0f);
    TextureXRT<float> output(w_, h_, 0.0f);
    UploadMatToTextureCPU(input, 0, image_src_cv_);

    DepthRendererXRT renderer;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();

    std::vector<double> times;
    times.reserve(iterations);

    for (int i = 0; i < iterations; ++i)
    {
        timer_.Start();
        renderer.Render(mesh, pose_transform, cam_, input, output, in_lvl, out_lvl);
        double time_ms = timer_.Stop();
        times.push_back(time_ms);
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

    // Performance assertions
    EXPECT_LT(avg_time, 100.0) << "CPU depth rendering too slow";

    return 0;
}
