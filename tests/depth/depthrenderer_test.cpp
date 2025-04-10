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

    // LoadDatasetTumRgbd dataset;
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
    int src = 0;
    int dst = 50;

    int operation = cv::MORPH_CLOSE;
    int morph_size = 5;
    int morph_elem = cv::MORPH_ELLIPSE;

    cv::Mat element = cv::getStructuringElement(morph_elem,
                                                cv::Size(2 * morph_size + 1, 2 * morph_size + 1),
                                                cv::Point(morph_size, morph_size));

    cv::Mat image_src_CV = cv::imread(image_files[src], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_src_CV = cv::imread(depth_files[src], cv::IMREAD_GRAYSCALE);
    cv::morphologyEx(depth_src_CV, depth_src_CV, operation, element);
    SE3 pose_src = poses[src].inverse();

    cv::Mat image_dst_CV = cv::imread(image_files[dst], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_dst_CV = cv::imread(depth_files[dst], cv::IMREAD_GRAYSCALE);
    cv::morphologyEx(depth_dst_CV, depth_dst_CV, operation, element);
    SE3 pose_dst = poses[dst].inverse();

    image_src_CV.convertTo(image_src_CV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    depth_src_CV.convertTo(depth_src_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_src_CV /= dataset.GetDepthFactor();
    depth_src_CV *= 100.0;

    image_dst_CV.convertTo(image_dst_CV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_dst_CV /= dataset.GetDepthFactor();
    depth_dst_CV *= 100.0;

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

    MeshCPU mesh(vertices, texcoords, weights);
    DepthRendererCPU renderer;
    TextureCPU<ImageType> image(w, h, 1, 0);
    TextureCPU<float> depth(w, h, 1, -1);
    image.FromCPU((ImageType *)image_src_CV.data);

    renderer.Render(mesh, pose_dst * pose_src.inverse(), cam, image, depth, 0);

    cv::Mat output_depthCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<float>(), 1));

    depth.ToCPU((float *)output_depthCV.data);

    float depthError = ComputeImageError<float>(depth_dst_CV, output_depthCV);

    cv::normalize(depth_dst_CV, depth_dst_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_depthCV, output_depthCV, 0, 255, cv::NORM_MINMAX);
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_depthCV.convertTo(output_depthCV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    cv::imwrite("rendercpudepth_input.png", depth_dst_CV);
    cv::imwrite("rendercpudepth_output.png", output_depthCV);

    EXPECT_EQ(depthError, 0.0f);
}

TEST_F(DataLoader, TestCPURenderImage)
{
    int src = 0;
    int dst = 50;

    cv::Mat image_src_CV = cv::imread(image_files[src], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_src_CV = cv::imread(depth_files[src], cv::IMREAD_GRAYSCALE);
    SE3 pose_src = poses[src].inverse();

    cv::Mat image_dst_CV = cv::imread(image_files[dst], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_dst_CV = cv::imread(depth_files[dst], cv::IMREAD_GRAYSCALE);
    SE3 pose_dst = poses[dst].inverse();

    image_src_CV.convertTo(image_src_CV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    depth_src_CV.convertTo(depth_src_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_src_CV /= dataset.GetDepthFactor();
    depth_src_CV *= 100.0;

    image_dst_CV.convertTo(image_dst_CV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_dst_CV /= dataset.GetDepthFactor();
    depth_dst_CV *= 100.0;

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

    MeshCPU mesh(vertices, texcoords, weights);
    ImageRendererCPU renderer;
    TextureCPU<ImageType> image(w, h, 1, 0);
    TextureCPU<ImageType> output(w, h, 1, -1);
    image.FromCPU((ImageType *)image_src_CV.data);

    renderer.Render(mesh, pose_dst * pose_src.inverse(), cam, image, output, 0);

    cv::Mat output_imageCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));

    output.ToCPU((ImageType *)output_imageCV.data);

    float depthError = ComputeImageError<ImageType>(image_dst_CV, output_imageCV);
    /*
    cv::normalize(image_dst_CV, image_dst_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_imageCV, output_imageCV, 0, 255, cv::NORM_MINMAX);
    image_dst_CV.convertTo(image_dst_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_imageCV.convertTo(output_imageCV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    cv::imwrite("rendercpuimage_input.png", image_dst_CV);
    cv::imwrite("rendercpuimage_output.png", output_imageCV);
    */
    EXPECT_EQ(depthError, 0.0f);
}

TEST_F(DataLoader, TestGLRenderDepth)
{
    ASSERT_TRUE(InitEGL());

    int src = 0;
    int dst = 50;

    int operation = cv::MORPH_CLOSE;
    int morph_size = 5;
    int morph_elem = cv::MORPH_ELLIPSE;

    cv::Mat element = cv::getStructuringElement(morph_elem,
                                                cv::Size(2 * morph_size + 1, 2 * morph_size + 1),
                                                cv::Point(morph_size, morph_size));

    cv::Mat image_src_CV = cv::imread(image_files[src], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_src_CV = cv::imread(depth_files[src], cv::IMREAD_GRAYSCALE);
    cv::morphologyEx(depth_src_CV, depth_src_CV, operation, element);
    SE3 pose_src = poses[src].inverse();

    cv::Mat image_dst_CV = cv::imread(image_files[dst], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_dst_CV = cv::imread(depth_files[dst], cv::IMREAD_GRAYSCALE);
    cv::morphologyEx(depth_dst_CV, depth_dst_CV, operation, element);
    SE3 pose_dst = poses[dst].inverse();

    image_src_CV.convertTo(image_src_CV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    depth_src_CV.convertTo(depth_src_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_src_CV /= dataset.GetDepthFactor();
    depth_src_CV *= 100.0;

    image_dst_CV.convertTo(image_dst_CV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_dst_CV /= dataset.GetDepthFactor();
    depth_dst_CV *= 100.0;

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

    MeshGL mesh(vertices, texcoords, weights);
    DepthRendererGL renderer;
    TextureGL<ImageType> image(w, h, 1, 0);
    TextureGL<float> depth(w, h, 1, -1);
    image.FromCPU((ImageType *)image_src_CV.data);

    renderer.Render(mesh, pose_dst * pose_src.inverse(), cam, image, depth, 0);

    cv::Mat output_depthCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<float>(), 1));

    depth.ToCPU((float *)output_depthCV.data);

    float depthError = ComputeImageError<float>(depth_dst_CV, output_depthCV);

    cv::normalize(depth_dst_CV, depth_dst_CV, 0, 255, cv::NORM_MINMAX);
    cv::threshold(output_depthCV, output_depthCV, 2.0, 2.0, cv::THRESH_TRUNC);
    cv::normalize(output_depthCV, output_depthCV, 0, 255, cv::NORM_MINMAX);
    //output_depthCV = output_depthCV * 255.0f;
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_depthCV.convertTo(output_depthCV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    cv::imwrite("rendergldepth_input.png", depth_dst_CV);
    cv::imwrite("rendergldepth_output.png", output_depthCV);

    EXPECT_EQ(depthError, 0.0f);
}