#include <gtest/gtest.h>

#include <opencv2/opencv.hpp>

#include "common/types.h"
#include "common/texturecpu.h"
#include "common/texturegl.h"
#include "common.h"
// #include "src/common/dataMulti.h"

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

TEST_F(DataLoader, TestCPUToFromOpenCV)
{
    int i = 0;
    cv::Mat imageCV = cv::imread(image_files[i], cv::IMREAD_GRAYSCALE);
    cv::Mat depthCV = cv::imread(depth_files[i], cv::IMREAD_GRAYSCALE);
    // SE3f gtPose = poses[i].inverse();

    imageCV.convertTo(imageCV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    depthCV.convertTo(depthCV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depthCV /= dataset.GetDepthFactor();
    depthCV *= 100.0;

    TextureCPU<ImageType> imageCPU(w, h, 1, 0);
    imageCPU.FromCPU((ImageType *)imageCV.data);

    TextureCPU<float> depthCPU(w, h, 1, 0);
    depthCPU.FromCPU((float *)depthCV.data);

    cv::Mat output_imageCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    cv::Mat output_depthCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<float>(), 1));

    imageCPU.ToCPU((ImageType *)output_imageCV.data);
    depthCPU.ToCPU((float *)output_depthCV.data);

    float imageError = ComputeImageError<ImageType>(imageCV, output_imageCV);
    float depthError = ComputeImageError<float>(depthCV, output_depthCV);

    EXPECT_EQ(imageError, 0.0f);
    EXPECT_EQ(depthError, 0.0f);
}

TEST_F(DataLoader, TestGLToFromOpenCV)
{
    ASSERT_TRUE(InitEGL());

    int i = 0;
    cv::Mat imageCV = cv::imread(image_files[i], cv::IMREAD_GRAYSCALE);
    cv::Mat depthCV = cv::imread(depth_files[i], cv::IMREAD_GRAYSCALE);
    // SE3f gtPose = poses[i].inverse();

    imageCV.convertTo(imageCV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    depthCV.convertTo(depthCV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depthCV /= dataset.GetDepthFactor();
    depthCV *= 100.0;

    TextureGL<ImageType> image(w, h, 1, 0);
    image.FromCPU((ImageType *)imageCV.data);

    TextureGL<float> depth(w, h, 1, 0);
    depth.FromCPU((float *)depthCV.data);

    cv::Mat output_imageCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    cv::Mat output_depthCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<float>(), 1));

    image.ToCPU((ImageType *)output_imageCV.data);
    depth.ToCPU((float *)output_depthCV.data);

    float imageError = ComputeImageError<ImageType>(imageCV, output_imageCV);
    float depthError = ComputeImageError<float>(depthCV, output_depthCV);

    EXPECT_EQ(imageError, 0.0f);
    EXPECT_EQ(depthError, 0.0f);
}
