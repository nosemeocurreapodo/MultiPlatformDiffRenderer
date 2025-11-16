#include <opencv2/opencv.hpp>
#include <Eigen/Core>

#define TEST_DATA_DIR "/home/emanuel/workspace/MultiPlatformDiffRenderer/src/tests/data"

#include "tests/common/loaddataset.h"
#include "tests/common/test_helpers.h"
#include "core/mesh_helpers.h"
#include "core/camera.h"

extern "C"
{
    void DepthRenderHLS(float *vertex_buffer_data,
                        unsigned int *ebo_buffer_data,
                        float *out_texture_data,
                        unsigned int vertex_buffer_size,
                        unsigned int ebo_buffer_size,
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

    cv::Mat image_src_cv = ReadMat(image_files[0], false);
    cv::Mat depth_src_cv = ReadMat(depth_files[0], true) * scale;
    linalg::SE3<float> pose_src = poses[0];

    cv::Mat image_dst_cv = ReadMat(image_files[50], false);
    cv::Mat depth_dst_cv = ReadMat(depth_files[50], true) * scale;
    linalg::SE3<float> pose_dst = poses[50];

    // std::vector<Eigen::Vector3f> vertices, normals;
    // std::vector<Eigen::Vector2f> texcoords;
    // std::vector<unsigned int> indices;
    // CreateMesh(depth_src_cpu, cam, 32, vertices, texcoords, normals, indices);

    std::vector<float> vertex;
    std::vector<unsigned int> indices;

    CreateMesh(depth_src_cv, cam, 32,
               vertex,
               indices,
               true,
               true,
               true);

    linalg::SE3<float> pose = pose_dst * pose_src.inverse();

    unsigned int lvl = 1;

    TextureCPU<unsigned char> diffuse_cpu(w, h, 0);
    UploadMatToTexture(diffuse_cpu, 0, image_src_cv);
    auto diffuse_map = diffuse_cpu.MapWrite(0);

    TextureCPU<float> depth_out_cpu(w, h, -1.0f);
    auto depth_map = depth_out_cpu.MapWrite(0);

    DepthRenderHLS(
        vertex.data(),
        indices.data(),
        depth_map.data(),
        vertex.size(), indices.size(),
        w, h, -1.0f, lvl,
        pose.so3().unit_quaternion().x(), pose.so3().unit_quaternion().y(), pose.so3().unit_quaternion().z(), pose.so3().unit_quaternion().w(),
        pose.translation()(0), pose.translation()(1), pose.translation()(2),
        cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3));

    cv::Mat depth_out_cv = DownloadTextureToMat(depth_out_cpu, lvl, CV_32FC1);

    // double depthError = ComputeImageError<float>(depth_dst_CV, output_depthCV, -1.0f);
    SaveDebugImage(depth_out_cv, "depthrenderhls_output.png");
}
