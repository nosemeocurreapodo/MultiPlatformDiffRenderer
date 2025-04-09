#include <gtest/gtest.h>

#include <opencv2/opencv.hpp>

#include "common.h"
#include "common/types.h"
#include "common/texturecpu.h"
#include "common/texturegl.h"
#include "common/buffercpu.h"
#include "common/buffergl.h"
#include "common/meshcpu.h"
#include "common/meshgl.h"
#include "renderercpu.h"
#include "renderergl.h"

class DataLoader : public ::testing::Test
{
protected:
    void SetUp() override
    {
        image_files = dataset.GetImageFiles();
        depth_files = dataset.GetDepthFiles();
        poses = dataset.GetPoses();
        timestamps = dataset.GetTimestamps();
        cam = dataset.GetCamera();
        w = dataset.GetWidth();
        h = dataset.GetHeight();
    }

    // load_dataset_tum_rgbd dataset;
    LoadDatasetIclNuim dataset;

    std::vector<std::string> image_files;
    std::vector<std::string> depth_files;
    std::vector<SE3> poses;
    std::vector<double> timestamps;
    CameraType cam;
    int w;
    int h;
};

TEST_F(DataLoader, TestCPURenderDepth)
{
    int i = 0;
    cv::Mat imageCV = cv::imread(image_files[i], cv::IMREAD_GRAYSCALE);
    cv::Mat depthCV = cv::imread(depth_files[i], cv::IMREAD_GRAYSCALE);
    SE3 gtPose = poses[i].inverse();

    imageCV.convertTo(imageCV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    depthCV.convertTo(depthCV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depthCV /= dataset.GetDepthFactor();
    depthCV *= 100.0;

    std::vector<Vec2> tex_coords = UniformTexCoords(32, 32);
    std::vector<float> texcoords;
    std::vector<float> vertices;
    std::vector<float> weights;
    for (Vec2 tex_coord : tex_coords)
    {
        Vec3 ray = cam.PixToRay(tex_coord);
        float depth = depthCV.at<float>(int(tex_coord(1)*h), int(tex_coord(0)*w));
        Vec3 vertex = ray * depth;
        vertices.push_back(vertex(0));
        vertices.push_back(vertex(1));
        vertices.push_back(vertex(2));
        texcoords.push_back(tex_coord(0));
        texcoords.push_back(tex_coord(1));
        weights.push_back(1.0f);
    }

    MeshCPU mesh(vertices, texcoords, weights);
    DepthRendererCPU depth_renderer;
    TextureCPU<float> depth(w, h, 1, -1);

    depth_renderer.Render(mesh, gtPose, cam, depth, 0);

    cv::Mat output_depthCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<float>(), 1));

    depth.ToCPU((float *)output_depthCV.data);

    float depthError = ComputeImageError<float>(depthCV, output_depthCV);

    EXPECT_EQ(depthError, 0.0f);
}
/*
TEST_F(DataLoader, TestGLRenderDepth)
{
    ASSERT_TRUE(InitEGL());

    int i = 0;
    cv::Mat imageCV = cv::imread(image_files[i], cv::IMREAD_GRAYSCALE);
    cv::Mat depthCV = cv::imread(depth_files[i], cv::IMREAD_GRAYSCALE);
    SE3 gtPose = poses[i].inverse();

    imageCV.convertTo(imageCV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    depthCV.convertTo(depthCV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depthCV /= dataset.GetDepthFactor();
    depthCV *= 100.0;

    std::vector<Vec2> tex_coords = UniformTexCoords(w, h);
    std::vector<float> texcoords;
    std::vector<float> vertices;
    std::vector<float> weights;
    for (Vec2 tex_coord : tex_coords)
    {
        Vec3 ray = cam.PixToRay(tex_coord);
        float depth = depthCV.at<float>(tex_coord(1), tex_coord(0));
        Vec3 vertex = ray * depth;
        vertices.push_back(vertex(0));
        vertices.push_back(vertex(1));
        vertices.push_back(vertex(2));
        texcoords.push_back(tex_coord(0));
        texcoords.push_back(tex_coord(1));
        weights.push_back(1.0f);
    }

    MeshGL mesh(vertices, texcoords, weights);
    DepthRendererGL depth_renderer;
    TextureGL<float> depth(w, h, 1, -1);

    depth_renderer.Render(mesh, gtPose, cam, depth, 0);

    cv::Mat output_depthCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<float>(), 1));

    depth.ToCPU((float *)output_depthCV.data);

    float depthError = ComputeImageError<float>(depthCV, output_depthCV);

    EXPECT_EQ(depthError, 0.0f);
}
*/