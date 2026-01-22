#include <opencv2/opencv.hpp>

#define TEST_DATA_DIR "/home/emanuel/workspace/mesh_vo/MultiPlatformDiffRenderer/src/tests/data"

#include "tests/common/loaddataset.h"
#include "tests/common/test_helpers.h"
// #include "core/types.h"
#include "core/camera.h"
#include "core/mesh_helpers.h"
#include <ap_int.h>

extern "C"
{
    void ImageRenderHLS(float *vertex_buffer_data,
                        int *ebo_buffer_data,
                        ImageType *diffuse_texture_data_ch1,
                        ImageType *diffuse_texture_data_ch2,
                        ImageType *diffuse_texture_data_ch3,
                        ImageType *diffuse_texture_data_ch4,
                        int diffuse_texture_offset,
                        ImageType *out_texture_data,
                        int out_texture_offset,
                        unsigned int vertex_buffer_size,
                        unsigned int ebo_buffer_size,
                        unsigned int diffuse_texture_width,
                        unsigned int diffuse_texture_height,
                        ImageType diffuse_nodata_value,
                        unsigned int out_texture_width,
                        unsigned int out_texture_height,
                        ImageType out_nodata_value,
                        float q_x, float q_y, float q_z, float q_w,
                        float t_x, float t_y, float t_z,
                        float fx, float fy, float cx, float cy,
                        float exp_a, float exp_b);
}

int main()
{
    LoadDatasetIclNuim dataset(std::string(TEST_DATA_DIR));
    // LoadDatasetTumRgbd dataset;

    std::vector<std::string> image_files = dataset.GetImageFiles();
    std::vector<std::string> depth_files = dataset.GetDepthFiles();
    std::vector<linalg::SE3<float>> poses = dataset.GetPoses();
    float depth_factor = dataset.GetDepthFactor();
    PinholeCamera<float> cam = dataset.GetCamera();
    unsigned int w = dataset.GetWidth();
    unsigned int h = dataset.GetHeight();

    linalg::Vec2<float> exposure(0.0f, 0.0f);

    TextureCPU<ImageType> image_src_cpu(w, h, 0);
    TextureCPU<float> depth_src_cpu(w, h, 0);
    TextureCPU<ImageType> image_dst_cpu(w, h, 0);

    cv::Mat image_src_cv = cv::imread(image_files[0], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_src_cv = cv::imread(depth_files[0], cv::IMREAD_GRAYSCALE);
    depth_src_cv.convertTo(depth_src_cv, CV_32FC1);
    depth_src_cv = depth_src_cv / depth_factor;
    linalg::SE3<float> pose_src = poses[0];

    cv::Mat image_dst_cv = cv::imread(image_files[50], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_dst_cv = cv::imread(depth_files[50], cv::IMREAD_GRAYSCALE);
    depth_dst_cv.convertTo(depth_dst_cv, CV_32FC1);
    depth_dst_cv = depth_dst_cv / depth_factor;
    linalg::SE3<float> pose_dst = poses[50];

    UploadMatToTexture(depth_src_cpu, 0, depth_src_cv);
    UploadMatToTexture(image_src_cpu, 0, image_src_cv);
    UploadMatToTexture(image_dst_cpu, 0, image_dst_cv);

    std::vector<float> vertex;
    std::vector<int> indices;
    CreateMesh(depth_src_cpu, cam, 32, vertex, indices, true, false, false);

    linalg::SE3<float> pose = pose_dst * pose_src.inverse();

    unsigned int lvl = 0;

    auto diffuse_map = image_src_cpu.MapWrite(0);
    Level diffuse_lvl = image_src_cpu.level(lvl);

    TextureCPU<ImageType> image_out_cpu(w, h, 0);
    auto image_out_map = image_out_cpu.MapWrite(0);
    Level image_out_lvl = image_out_cpu.level(lvl);

    ImageRenderHLS(
        vertex.data(),
        indices.data(),
        (ImageType *)diffuse_map.data(),
        (ImageType *)diffuse_map.data(),
        (ImageType *)diffuse_map.data(),
        (ImageType *)diffuse_map.data(),
        diffuse_lvl.offset,
        (ImageType *)image_out_map.data(),
        image_out_lvl.offset,
        vertex.size(), indices.size(),
        image_src_cpu.width(lvl), image_src_cpu.height(lvl), 0,
        image_out_cpu.width(lvl), image_out_cpu.height(lvl), 0,
        pose.so3().unit_quaternion().x(), pose.so3().unit_quaternion().y(), pose.so3().unit_quaternion().z(), pose.so3().unit_quaternion().w(),
        pose.translation()(0), pose.translation()(1), pose.translation()(2),
        cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3),
        exposure(0), exposure(1));

    cv::Mat image_out_cv = DownloadTextureToMat(image_out_cpu, lvl);
    SaveDebugImage(image_out_cv, "imagerenderhls_output.png");

    cv::Mat image_reference_cv = DownloadTextureToMat(image_dst_cpu, lvl);
    SaveDebugImage(image_reference_cv, "imagerenderhls_reference.png");
}
