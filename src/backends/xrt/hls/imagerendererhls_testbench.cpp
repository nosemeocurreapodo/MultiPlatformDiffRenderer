#include <opencv2/opencv.hpp>

#define TEST_DATA_DIR "/home/emanuel/workspace/MultiPlatformDiffRenderer/src/tests/data"

#include "tests/common/loaddataset.h"
#include "tests/common/test_helpers.h"
// #include "core/types.h"
#include "core/camera.h"
#include "core/mesh_helpers.h"

extern "C"
{
    void ImageRenderHLS(float *vertex_buffer_data,
                        unsigned int *ebo_buffer_data,
                        float *diffuse_texture_data,
                        float *depth_texture_data,
                        float *out_texture_data,
                        unsigned int vertex_buffer_size,
                        unsigned int ebo_buffer_size,
                        unsigned int diffuse_texture_width,
                        unsigned int diffuse_texture_height,
                        float diffuse_nodata_value,
                        unsigned int diffuse_lvl,
                        unsigned int out_texture_width,
                        unsigned int out_texture_height,
                        float out_nodata_value,
                        unsigned int out_lvl,
                        float q_x, float q_y, float q_z, float q_w,
                        float t_x, float t_y, float t_z,
                        float fx, float fy, float cx, float cy);
}

int main()
{
    LoadDatasetIclNuim dataset(std::string(TEST_DATA_DIR));
    // LoadDatasetTumRgbd dataset;

    std::vector<std::string> image_files = dataset.GetImageFiles();
    std::vector<std::string> depth_files = dataset.GetDepthFiles();
    std::vector<linalg::SE3<float>> poses = dataset.GetPoses();
    float depth_factor = dataset.GetDepthFactor();
    Camera<float> cam = dataset.GetCamera();
    unsigned int w = dataset.GetWidth();
    unsigned int h = dataset.GetHeight();

    float scale = 1.0f / depth_factor;

    cv::Mat image_src_cv = ReadMat(image_files[0]);
    cv::Mat depth_src_cv = ReadMat(depth_files[0]) * scale;
    linalg::SE3<float> pose_src = poses[0];

    cv::Mat image_dst_cv = ReadMat(image_files[50]);
    cv::Mat depth_dst_cv = ReadMat(depth_files[50]) * scale;
    linalg::SE3<float> pose_dst = poses[50];

    std::vector<float> vertex;
    std::vector<unsigned int> indices;
    CreateMesh(depth_src_cv, cam, 32, vertex, indices);

    std::vector<float> screen_vertex;
    std::vector<unsigned int> screen_indices;
    CreateScreenQuad(screen_vertex, screen_indices);

    linalg::SE3<float> pose = pose_dst * pose_src.inverse();

    unsigned int lvl = 1;

    TextureCPU<float> diffuse_cpu(w, h, -1.0f);
    UploadMatToTexture(diffuse_cpu, 0, image_src_cv);
    auto diffuse_map = diffuse_cpu.MapWrite(0);

    TextureCPU<float> depth_out_cpu(w, h, -1.0f);
    auto depth_out_map = depth_out_cpu.MapWrite(0);

    TextureCPU<float> image_out_cpu(w, h, -1.0f);
    auto image_out_map = image_out_cpu.MapWrite(0);

    ImageRenderHLS(
        vertex.data(),
        indices.data(),
        diffuse_map.data(),
        depth_out_map.data(),
        image_out_map.data(),
        vertex.size(), indices.size(),
        w, h, -1.0f, lvl,
        w, h, -1.0f, lvl,
        pose.so3().unit_quaternion().x(), pose.so3().unit_quaternion().y(), pose.so3().unit_quaternion().z(), pose.so3().unit_quaternion().w(),
        pose.translation()(0), pose.translation()(1), pose.translation()(2),
        cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3));

    cv::Mat image_out_cv = DownloadTextureToMat(image_out_cpu, lvl, CV_32FC1);

    // double depthError = ComputeImageError<float>(depth_dst_CV, output_depthCV, -1.0f);
    SaveDebugImage(image_out_cv, "imagerenderhls_output.png");
}
