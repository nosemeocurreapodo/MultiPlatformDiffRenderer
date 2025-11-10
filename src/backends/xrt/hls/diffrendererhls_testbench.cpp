#include <opencv2/opencv.hpp>

#define TEST_DATA_DIR "/home/emanuel/workspace/MultiPlatformDiffRenderer/src/tests/data"

#include "tests/common/loaddataset.h"
#include "tests/common/test_helpers.h"
// #include "core/types.h"
#include "core/camera.h"
#include "core/mesh_helpers.h"
#include <ap_int.h>

extern "C"
{
    void DiffRenderHLS(float *vertex_buffer_data,
                       unsigned int *ebo_buffer_data,
                       ap_uint<8> *f_texture_data_ch1,
                       ap_uint<8> *f_texture_data_ch2,
                       ap_uint<8> *f_texture_data_ch3,
                       ap_uint<8> *f_texture_data_ch4,
                       ap_uint<8> *image_texture_data,
                       float *depth_texture_data,
                       linalg::Vec3<float> *jtra_texture_data,
                       linalg::Vec3<float> *jrot_texture_data,
                       linalg::Vec3<float> *jmap_texture_data,
                       linalg::Vec3<float> *pids_texture_data,
                       unsigned int vertex_buffer_size,
                       unsigned int ebo_buffer_size,
                       unsigned int f_texture_width,
                       unsigned int f_texture_height,
                       unsigned char f_nodata_value,
                       unsigned int in_lvl,
                       unsigned int out_texture_width,
                       unsigned int out_texture_height,
                       unsigned char image_nodata_value,
                       float depth_nodata_value,
                       linalg::Vec3<float> jtra_nodata_value,
                       linalg::Vec3<float> jrot_nodata_value,
                       linalg::Vec3<float> jmap_nodata_value,
                       linalg::Vec3<float> pids_nodata_value,
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

    cv::Mat image_src_cv = ReadMat(image_files[0], false);
    cv::Mat depth_src_cv = ReadMat(depth_files[0], true) * scale;
    linalg::SE3<float> pose_src = poses[0];

    cv::Mat image_dst_cv = ReadMat(image_files[50], false);
    cv::Mat depth_dst_cv = ReadMat(depth_files[50], true) * scale;
    linalg::SE3<float> pose_dst = poses[50];

    std::vector<float> vertex;
    std::vector<unsigned int> indices;
    CreateMesh(depth_src_cv, cam, 32, vertex, indices);

    std::vector<float> screen_vertex;
    std::vector<unsigned int> screen_indices;
    CreateScreenQuad(screen_vertex, screen_indices);

    linalg::SE3<float> pose = pose_dst * pose_src.inverse();

    int in_lvl = 0;
    int out_lvl = 0;

    TextureCPU<unsigned char> image_src_cpu(w, h, 0);
    UploadMatToTexture(image_src_cpu, 0, image_src_cv);

    TextureCPU<unsigned char> image_out_cpu(w, h, 0);
    TextureCPU<float> depth_out_cpu(w, h, -1.0f);
    TextureCPU<linalg::Vec3<float>> jtra_out_cpu(w, h, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> jrot_out_cpu(w, h, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> jmap_out_cpu(w, h, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> pids_out_cpu(w, h, linalg::Vec3<float>(-1.0f, -1.0f, -1.0f));

    auto image_in_map = image_src_cpu.MapWrite(0);
    auto image_out_map = image_out_cpu.MapWrite(0);
    auto depth_out_map = depth_out_cpu.MapWrite(0);
    auto jtra_out_map = jtra_out_cpu.MapWrite(0);
    auto jrot_out_map = jrot_out_cpu.MapWrite(0);
    auto jmap_out_map = jmap_out_cpu.MapWrite(0);
    auto pids_out_map = pids_out_cpu.MapWrite(0);

    DiffRenderHLS(
        vertex.data(),
        indices.data(),
        (ap_uint<8> *)image_in_map.data(),
        (ap_uint<8> *)image_in_map.data(),
        (ap_uint<8> *)image_in_map.data(),
        (ap_uint<8> *)image_in_map.data(),
        (ap_uint<8> *)image_out_map.data(),
        depth_out_map.data(),
        jtra_out_map.data(),
        jrot_out_map.data(),
        jmap_out_map.data(),
        pids_out_map.data(),
        vertex.size(), indices.size(),
        w, h, 0, in_lvl,
        w, h,
        image_out_cpu.nodata(), depth_out_cpu.nodata(), jtra_out_cpu.nodata(), jrot_out_cpu.nodata(), jmap_out_cpu.nodata(), pids_out_cpu.nodata(),
        out_lvl,
        pose.so3().unit_quaternion().x(), pose.so3().unit_quaternion().y(), pose.so3().unit_quaternion().z(), pose.so3().unit_quaternion().w(),
        pose.translation()(0), pose.translation()(1), pose.translation()(2),
        cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3));

    cv::Mat image_out_cv = DownloadTextureToMat(image_out_cpu, out_lvl, CV_8UC1);
    cv::Mat depth_out_cv = DownloadTextureToMat(depth_out_cpu, out_lvl, CV_32FC1);
    cv::Mat jtra_out_cv = DownloadTextureToMat(jtra_out_cpu, out_lvl, CV_32FC3);
    cv::Mat jrot_out_cv = DownloadTextureToMat(jrot_out_cpu, out_lvl, CV_32FC3);
    cv::Mat jmap_out_cv = DownloadTextureToMat(jmap_out_cpu, out_lvl, CV_32FC3);
    cv::Mat pids_out_cv = DownloadTextureToMat(pids_out_cpu, out_lvl, CV_32FC3);

    cv::normalize(image_out_cv, image_out_cv, 0, 255, cv::NORM_MINMAX);
    image_out_cv.convertTo(image_out_cv, CV_8U);

    cv::normalize(depth_out_cv, depth_out_cv, 0, 255, cv::NORM_MINMAX);
    depth_out_cv.convertTo(depth_out_cv, CV_8U);

    cv::normalize(jtra_out_cv, jtra_out_cv, 0, 255, cv::NORM_MINMAX);
    jtra_out_cv.convertTo(jtra_out_cv, CV_8U);

    cv::normalize(jrot_out_cv, jrot_out_cv, 0, 255, cv::NORM_MINMAX);
    jrot_out_cv.convertTo(jrot_out_cv, CV_8U);

    cv::normalize(jmap_out_cv, jmap_out_cv, 0, 255, cv::NORM_MINMAX);
    jmap_out_cv.convertTo(jmap_out_cv, CV_8U);

    cv::normalize(pids_out_cv, pids_out_cv, 0, 255, cv::NORM_MINMAX);
    pids_out_cv.convertTo(pids_out_cv, CV_8U);

    // double depthError = ComputeImageError<float>(depth_dst_CV, output_depthCV, -1.0f);
    SaveDebugImage(image_out_cv, "imagerenderhls_image.png");
    SaveDebugImage(depth_out_cv, "imagerenderhls_depth.png");
    SaveDebugImage(jtra_out_cv, "imagerenderhls_jtra.png");
    SaveDebugImage(jrot_out_cv, "imagerenderhls_jrot.png");
    SaveDebugImage(jmap_out_cv, "imagerenderhls_jmap.png");
    SaveDebugImage(pids_out_cv, "imagerenderhls_pids.png");
}
