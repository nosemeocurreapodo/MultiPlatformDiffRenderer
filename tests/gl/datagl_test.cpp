#include <gtest/gtest.h>

#include <opencv2/opencv.hpp>

#include "common/types.h"
#include "gl/texturegl.h"
#include "gl/buffergl.h"
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

TEST_F(DataLoader, TestGLToFromBuffer)
{
    ASSERT_TRUE(InitEGL());

    int src = 0;

    cv::Mat image_src_CV = cv::imread(image_files[src], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_src_CV = cv::imread(depth_files[src], cv::IMREAD_GRAYSCALE);
    SE3 pose_src = poses[src].inverse();

    image_src_CV.convertTo(image_src_CV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    depth_src_CV.convertTo(depth_src_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_src_CV /= dataset.GetDepthFactor();
    depth_src_CV *= 100.0;

    std::vector<Vec2> tex_coords = UniformTexCoords(32, 32);
    std::vector<float> texcoords;
    std::vector<float> vertices;
    std::vector<float> weights;
    for (Vec2 tex_coord : tex_coords)
    {
        Vec2 img_coord = Vec2(tex_coord(0) * w, tex_coord(1) * h);
        float depth = depth_src_CV.at<float>(int(img_coord(1)), int(img_coord(0)));
        // float depth = VerticallySmoothDepth(tex_coord, 0.1f, 10.0f);
        if (depth <= 0.0f)
            continue;
        Vec3 ray = cam.PixToRay(tex_coord);
        Vec3 vertex = ray * depth;
        vertices.push_back(vertex(0));
        vertices.push_back(vertex(1));
        vertices.push_back(vertex(2));
        texcoords.push_back(tex_coord(0));
        texcoords.push_back(tex_coord(1));
        weights.push_back(1.0f);
    }

    BufferGL<float> pos_buffer(vertices);
    BufferGL<float> tex_buffer(texcoords);
    BufferGL<float> wei_buffer(weights);

    std::vector<float> pos_cpu(vertices.size());
    std::vector<float> tex_cpu(texcoords.size());
    std::vector<float> wei_cpu(weights.size());

    pos_buffer.ToCPU((float *)pos_cpu.data());
    tex_buffer.ToCPU((float *)tex_cpu.data());
    wei_buffer.ToCPU((float *)wei_cpu.data());

    for(int i = 0; i < vertices.size(); i++)
    {
        EXPECT_EQ(pos_cpu[i], vertices[i]);
    }
    for(int i = 0; i < texcoords.size(); i++)
    {
        EXPECT_EQ(tex_cpu[i], texcoords[i]);
    }
    for(int i = 0; i < weights.size(); i++)
    {
        EXPECT_EQ(wei_cpu[i], weights[i]);
    }
}
