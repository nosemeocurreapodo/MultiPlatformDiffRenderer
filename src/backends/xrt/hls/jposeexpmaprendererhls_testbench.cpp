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
    void JPoseExpMapRenderHLS(float *vertex_buffer_data,
                              int *ebo_buffer_data,
                              ap_uint<8> *kf_texture_data,
                              ap_uint<8> *f_texture_data,
                              ap_uint<8> *dfdxy_texture_data,
                              ap_uint<8> *jtra_texture_data,
                              ap_uint<8> *jrot_texture_data,
                              ap_uint<8> *jexp_texture_data,
                              ap_uint<8> *jmap_texture_data,
                              ap_uint<8> *pids_texture_data,
                              ap_uint<8> *r_texture_data,
                              unsigned int vertex_buffer_size,
                              unsigned int ebo_buffer_size,
                              unsigned int in_texture_width,
                              unsigned int in_texture_height,
                              unsigned int out_texture_width,
                              unsigned int out_texture_height,
                              ImageType kf_nodata_value,
                              ImageType f_nodata_value,
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
    TextureCPU<Vec3<float>> didxy_src_cpu(w, h, Vec3<float>(0, 0, 0));
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
    CreateMesh(depth_src_cpu, cam, 32, vertex, indices, true, true, false);

    linalg::SE3<float> pose = pose_dst * pose_src.inverse();

    unsigned int lvl = 0;

    auto kf_map = image_src_cpu.MapWrite(lvl);
    auto f_map = image_dst_cpu.MapWrite(lvl);
    auto didxy_map = didxy_src_cpu.MapWrite(lvl);

    TextureCPU<Vec3<float>> jtra_texture_cpu(w, h, Vec3<float>(0, 0, 0));
    TextureCPU<Vec3<float>> jrot_texture_cpu(w, h, Vec3<float>(0, 0, 0));
    TextureCPU<Vec3<float>> jexp_texture_cpu(w, h, Vec3<float>(0, 0, 0));
    TextureCPU<Vec3<float>> jmap_texture_cpu(w, h, Vec3<float>(0, 0, 0));
    TextureCPU<Vec3<float>> pids_texture_cpu(w, h, Vec3<float>(0, 0, 0));
    TextureCPU<float> r_texture_cpu(w, h, 0);

    auto jtra_map = jtra_texture_cpu.MapWrite(lvl);
    auto jrot_map = jrot_texture_cpu.MapWrite(lvl);
    auto jexp_map = jexp_texture_cpu.MapWrite(lvl);
    auto jmap_map = jmap_texture_cpu.MapWrite(lvl);
    auto pids_map = pids_texture_cpu.MapWrite(lvl);
    auto r_map = r_texture_cpu.MapWrite(lvl);

    JPoseExpMapRenderHLS(
        vertex.data(),
        indices.data(),
        (ap_uint<8> *)kf_map.data(),
        (ap_uint<8> *)f_map.data(),
        (ap_uint<8> *)didxy_map.data(),
        (ap_uint<8> *)jtra_map.data(),
        (ap_uint<8> *)jrot_map.data(),
        (ap_uint<8> *)jexp_map.data(),
        (ap_uint<8> *)jmap_map.data(),
        (ap_uint<8> *)pids_map.data(),
        (ap_uint<8> *)r_map.data(),
        vertex.size(), indices.size(),
        image_src_cpu.width(lvl), image_src_cpu.height(lvl),
        image_src_cpu.width(lvl), image_src_cpu.height(lvl),
        image_src_cpu.nodata(),
        image_dst_cpu.nodata(),
        pose.so3().unit_quaternion().x(), pose.so3().unit_quaternion().y(), pose.so3().unit_quaternion().z(), pose.so3().unit_quaternion().w(),
        pose.translation()(0), pose.translation()(1), pose.translation()(2),
        cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3),
        exposure(0), exposure(1));

    cv::Mat jtra_out_cv = DownloadTextureToMat(jtra_texture_cpu, lvl);
    SaveDebugImage(jtra_out_cv, "jposeexpmaprenderhls_jtra.png");
    cv::Mat jrot_out_cv = DownloadTextureToMat(jrot_texture_cpu, lvl);
    SaveDebugImage(jrot_out_cv, "jposeexpmaprenderhls_jrot.png");
    cv::Mat jexp_out_cv = DownloadTextureToMat(jexp_texture_cpu, lvl);
    SaveDebugImage(jexp_out_cv, "jposeexpmaprenderhls_jexp.png");
    cv::Mat jmap_out_cv = DownloadTextureToMat(jmap_texture_cpu, lvl);
    SaveDebugImage(jmap_out_cv, "jposeexpmaprenderhls_jmap.png");
    cv::Mat pids_out_cv = DownloadTextureToMat(pids_texture_cpu, lvl);
    SaveDebugImage(pids_out_cv, "jposeexpmaprenderhls_pids.png");
    cv::Mat r_out_cv = DownloadTextureToMat(r_texture_cpu, lvl);
    SaveDebugImage(r_out_cv, "jposeexpmaprenderhls_r.png");
}
