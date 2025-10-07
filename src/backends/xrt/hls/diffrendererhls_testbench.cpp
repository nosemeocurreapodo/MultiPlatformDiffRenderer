#include <opencv2/opencv.hpp>

#define TEST_DATA_DIR "/home/emanuel/workspace/MultiPlatformRenderer/tests/data"

#include "loaddataset.h"
#include "common/test_helpers.h"
#include "core/types.h"
#include "core/camera.h"
#include "core/common.h"
#include "backends/xrt/hls/diffrendererhls.h"

int main()
{
    LoadDatasetIclNuim dataset(std::string(TEST_DATA_DIR));
    // LoadDatasetTumRgbd dataset;

    std::vector<std::string> image_files = dataset.GetImageFiles();
    std::vector<std::string> depth_files = dataset.GetDepthFiles();
    std::vector<SE3> poses = dataset.GetPoses();
    float depth_factor = dataset.GetDepthFactor();
    Camera cam = dataset.GetCamera();
    unsigned int w = dataset.GetWidth();
    unsigned int h = dataset.GetHeight();

    float scale = 1.0f / depth_factor;

    cv::Mat image_src_cv = ReadMat(image_files[0]);
    cv::Mat depth_src_cv = ReadMat(depth_files[0]) * scale;
    SE3 pose_src = poses[0];

    TextureCPU<float> image_src_cpu(w, h, -1.0f);
    TextureCPU<float> depth_src_cpu(w, h, -1.0f);

    UploadMatToTexture(image_src_cpu, 0, image_src_cv);
    UploadMatToTexture(depth_src_cpu, 0, depth_src_cv);

    cv::Mat image_dst_cv = ReadMat(image_files[50]);
    cv::Mat depth_dst_cv = ReadMat(depth_files[50]) * scale;
    SE3 pose_dst = poses[50];

    std::vector<float> vertices, texcoords, weights;
    std::vector<unsigned int> indices;
    CreateMesh(depth_src_cpu, cam, 32, vertices, texcoords, weights, indices);

    std::vector<float> screen_vertices, screen_texcoords, screen_weights;
    std::vector<unsigned int> screen_indices;
    CreateScreenQuad(screen_vertices, screen_texcoords, screen_weights, screen_indices);

    SE3 pose = pose_dst * pose_src.inverse();

    int lvl = 0;

    auto image_in_map = image_src_cpu.MapRead(0);

    TextureCPU<Vec3> jtra_out_cpu(w, h, Vec3(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3> jrot_out_cpu(w, h, Vec3(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3> jmap_out_cpu(w, h, Vec3(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3> pids_out_cpu(w, h, Vec3(-1.0f, -1.0f, -1.0f));

    auto jtra_out_map = jtra_out_cpu.MapWrite(0);
    auto jrot_out_map = jrot_out_cpu.MapWrite(0);
    auto jmap_out_map = jmap_out_cpu.MapWrite(0);
    auto pids_out_map = pids_out_cpu.MapWrite(0);

    DiffRenderHLS(
        (Scalar *)vertices.data(),
        (Scalar *)texcoords.data(),
        (Scalar *)weights.data(),
        (UInt *)indices.data(),
        (Scalar *)image_in_map.data(),
        (Vec3 *)jtra_out_map.data(),
        (Vec3 *)jrot_out_map.data(),
        (Vec3 *)jmap_out_map.data(),
        (Vec3 *)pids_out_map.data(),
        UInt(vertices.size()), UInt(texcoords.size()), UInt(weights.size()), UInt(indices.size()),
        UInt(w), UInt(h), Scalar(-1), UInt(lvl),
        UInt(w), UInt(h),
        jtra_out_cpu.nodata(), jrot_out_cpu.nodata(), jmap_out_cpu.nodata(), pids_out_cpu.nodata(),
        UInt(lvl),
        Scalar(pose.so3().unit_quaternion().x()), Scalar(pose.so3().unit_quaternion().y()), Scalar(pose.so3().unit_quaternion().z()), Scalar(pose.so3().unit_quaternion().w()),
        Scalar(pose.translation()(0)), Scalar(pose.translation()(1)), Scalar(pose.translation()(2)),
        Scalar(cam.GetParams()(0)), Scalar(cam.GetParams()(1)), Scalar(cam.GetParams()(2)), Scalar(cam.GetParams()(3)));

    cv::Mat jtra_out_cv = DownloadTextureToMat(jtra_out_cpu, lvl, CV_32FC3);
    cv::Mat jrot_out_cv = DownloadTextureToMat(jrot_out_cpu, lvl, CV_32FC3);
    cv::Mat jmap_out_cv = DownloadTextureToMat(jmap_out_cpu, lvl, CV_32FC3);
    cv::Mat pids_out_cv = DownloadTextureToMat(pids_out_cpu, lvl, CV_32FC3);

    // double depthError = ComputeImageError<float>(depth_dst_CV, output_depthCV, -1.0f);
    SaveDebugImage(jtra_out_cv, "imagerenderhls_jtra.png");
    SaveDebugImage(jrot_out_cv, "imagerenderhls_jrot.png");
    SaveDebugImage(jmap_out_cv, "imagerenderhls_jmap.png");
    SaveDebugImage(pids_out_cv, "imagerenderhls_pids.png");
}
