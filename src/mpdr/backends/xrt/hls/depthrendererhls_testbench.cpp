#include <opencv2/opencv.hpp>
#include <Eigen/Core>

#define TEST_DATA_DIR "/home/emanuel/workspace/mesh_vo/MultiPlatformDiffRenderer/tests/data"

#include "tests/common/loaddataset.h"
#include "mpdr/common/helpers.h"
#include "mpdr/common/mesh_helpers.h"
#include "mpdr/common/camera.h"
#include "mpdr/backends/cpu/meshcpu.h"

extern "C"
{
    void DepthRenderHLS(float *vertex_buffer_data,
                        int *ebo_buffer_data,
                        float *out_texture_data,
                        int out_texture_offset,
                        unsigned int vertex_buffer_size,
                        unsigned int ebo_buffer_size,
                        unsigned int out_texture_width,
                        unsigned int out_texture_height,
                        float out_nodata_value,
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

    TextureCPU<float> depth_src_cpu(w, h, 0.0f);
    TextureCPU<float> depth_dst_cpu(w, h, 0.0f);

    cv::Mat depth_src_cv = cv::imread(depth_files[0], cv::IMREAD_GRAYSCALE);
    depth_src_cv.convertTo(depth_src_cv, CV_32FC1);
    depth_src_cv = depth_src_cv / depth_factor;
    UploadMatToTexture(depth_src_cpu, 0, depth_src_cv);
    linalg::SE3<float> pose_src = poses[0];

    cv::Mat depth_dst_cv = cv::imread(depth_files[50], cv::IMREAD_GRAYSCALE);
    depth_dst_cv.convertTo(depth_dst_cv, CV_32FC1);
    depth_dst_cv = depth_dst_cv / depth_factor;
    UploadMatToTexture(depth_dst_cpu, 0, depth_dst_cv);
    linalg::SE3<float> pose_dst = poses[50];

    MeshCPU mesh_cpu = CreateMesh<MeshCPU>((float *)depth_src_cv.data,
               cam,
               depth_src_cv.cols,
               depth_src_cv.rows,
               24,
               10.0);

    linalg::SE3<float> pose = pose_dst * pose_src.inverse();

    unsigned int lvl = 1;

    auto vertex_buff_map = mesh_cpu.vertex_buffer_.MapWrite();
    auto ebo_buff_map = mesh_cpu.ebo_buffer_.MapWrite();

    TextureCPU<float> depth_out_cpu(w, h, 0.0f);
    auto depth_map = depth_out_cpu.MapWrite(0);
    Level level = depth_out_cpu.level(lvl);

    DepthRenderHLS(
        vertex_buff_map.data(),
        ebo_buff_map.data(),
        depth_map.data(),
        level.offset,
        vertex_buff_map.size(), ebo_buff_map.size(),
        depth_out_cpu.width(lvl), depth_out_cpu.height(lvl), 0.0f,
        pose.so3().unit_quaternion().x(), pose.so3().unit_quaternion().y(), pose.so3().unit_quaternion().z(), pose.so3().unit_quaternion().w(),
        pose.translation()(0), pose.translation()(1), pose.translation()(2),
        cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3));

    double error = RMSE(depth_dst_cpu, depth_out_cpu, lvl);

    std::cout << "Depth render HLS RMSE: " << error << " m" << std::endl;

    cv::Mat depth_out_cv = DownloadTextureToMat(depth_out_cpu, lvl);
    SaveDebugImage(depth_out_cv, "depthrenderhls_output.png");

    cv::Mat depth_ref_cv = DownloadTextureToMat(depth_dst_cpu, lvl);
    SaveDebugImage(depth_ref_cv, "depthrenderhls_reference.png");

}
