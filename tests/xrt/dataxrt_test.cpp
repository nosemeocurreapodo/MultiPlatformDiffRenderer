#include <gtest/gtest.h>

#include <opencv2/opencv.hpp>

#include "common/types.h"
#include "common/texturexrt.h"
#include "common/bufferxrt.h"
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

TEST_F(DataLoader, TestXRTToFromBuffer)
{
    ASSERT_TRUE(InitXRT());

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

    BufferXRT<float> pos_buffer(vertices);
    BufferXRT<float> tex_buffer(texcoords);
    BufferXRT<float> wei_buffer(weights);

    std::vector<float> pos_cpu(vertices.size());
    std::vector<float> tex_cpu(texcoords.size());
    std::vector<float> wei_cpu(weights.size());

    pos_buffer.ToCPU((float *)pos_cpu.data());
    tex_buffer.ToCPU((float *)tex_cpu.data());
    wei_buffer.ToCPU((float *)wei_cpu.data());

    for (int i = 0; i < vertices.size(); i++)
    {
        EXPECT_EQ(pos_cpu[i], vertices[i]);
    }
    for (int i = 0; i < texcoords.size(); i++)
    {
        EXPECT_EQ(tex_cpu[i], texcoords[i]);
    }
    for (int i = 0; i < weights.size(); i++)
    {
        EXPECT_EQ(wei_cpu[i], weights[i]);
    }
}
