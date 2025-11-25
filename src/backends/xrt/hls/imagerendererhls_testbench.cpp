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
                        ap_uint<8> *diffuse_texture_data_ch1,
                        ap_uint<8> *diffuse_texture_data_ch2,
                        ap_uint<8> *diffuse_texture_data_ch3,
                        ap_uint<8> *diffuse_texture_data_ch4,
                        ap_uint<8> *out_texture_data,
                        unsigned int vertex_buffer_size,
                        unsigned int ebo_buffer_size,
                        unsigned int diffuse_texture_width,
                        unsigned int diffuse_texture_height,
                        ImageType diffuse_nodata_value,
                        unsigned int diffuse_lvl,
                        unsigned int out_texture_width,
                        unsigned int out_texture_height,
                        ImageType out_nodata_value,
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
    PinholeCamera<float> cam = dataset.GetCamera();
    unsigned int w = dataset.GetWidth();
    unsigned int h = dataset.GetHeight();

    float scale = 1.0f / depth_factor;

    cv::Mat image_src_cv = cv::imread(image_files[0], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_src_cv = cv::imread(depth_files[0], cv::IMREAD_GRAYSCALE);
    depth_src_cv.convertTo(depth_src_cv, CV_32FC1);
    depth_src_cv = depth_src_cv * scale;
    linalg::SE3<float> pose_src = poses[0];

    cv::Mat image_dst_cv = cv::imread(image_files[50], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_dst_cv = cv::imread(depth_files[50], cv::IMREAD_GRAYSCALE);
    depth_dst_cv.convertTo(depth_dst_cv, CV_32FC1);
    depth_dst_cv = depth_dst_cv * scale;
    linalg::SE3<float> pose_dst = poses[50];

    std::vector<float> vertex;
    std::vector<int> indices;
    CreateMesh(depth_src_cv, cam, 32, vertex, indices);

    std::vector<float> screen_vertex;
    std::vector<int> screen_indices;
    CreateScreenQuad(screen_vertex, screen_indices);

    linalg::SE3<float> pose = pose_dst * pose_src.inverse();

    unsigned int lvl = 0;

    TextureCPU<ImageType> diffuse_cpu(w, h, 0);
    UploadMatToTexture(diffuse_cpu, 0, image_src_cv);
    auto diffuse_map = diffuse_cpu.MapWrite(0);

    TextureCPU<ImageType> image_out_cpu(w, h, 0);
    auto image_out_map = image_out_cpu.MapWrite(0);

    ImageRenderHLS(
        vertex.data(),
        indices.data(),
        (ap_uint<8> *)diffuse_map.data(),
        (ap_uint<8> *)diffuse_map.data(),
        (ap_uint<8> *)diffuse_map.data(),
        (ap_uint<8> *)diffuse_map.data(),
        (ap_uint<8> *)image_out_map.data(),
        vertex.size(), indices.size(),
        w, h, 0, lvl,
        w, h, 0, lvl,
        pose.so3().unit_quaternion().x(), pose.so3().unit_quaternion().y(), pose.so3().unit_quaternion().z(), pose.so3().unit_quaternion().w(),
        pose.translation()(0), pose.translation()(1), pose.translation()(2),
        cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3));

    cv::Mat image_out_cv = DownloadTextureToMat(image_out_cpu, lvl);

    // double depthError = ComputeImageError<float>(depth_dst_CV, output_depthCV, -1.0f);
    SaveDebugImage(image_out_cv, "imagerenderhls_output.png");
}
