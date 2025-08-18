#include <gtest/gtest.h>

#include <opencv2/opencv.hpp>

#include "core/format_converters.h"
#include "core/types.h"

#include "backends/cpu/texturecpu.h"
#include "backends/cpu/buffercpu.h"
#include "backends/cpu/meshcpu.h"
#include "backends/cpu/renderercpu.h"

#include "backends/gl/devicegl_glad.h"
#include "backends/gl/texturegl.h"
// #include "backends/gl/buffergl.h"
#include "backends/gl/meshgl.h"
#include "backends/gl/renderergl.h"
#include "loaddataset.h"

class GLContextEnv : public ::testing::Environment
{
public:
    void SetUp() override { InitEGL(); }
    // void TearDown() override { ShutdownEGL(); }
};

class DataLoader : public ::testing::Test
{
protected:
    DataLoader() : dataset(std::string(TEST_DATA_DIR)) {}

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
    Camera cam;
    int w;
    int h;
};

class DataLoaderOneFrame : public ::testing::Test
{
protected:
    DataLoaderOneFrame() {}

    void SetUp() override
    {
        // LoadDatasetTumRgbd dataset;
        LoadDatasetIclNuim dataset(std::string(TEST_DATA_DIR));

        std::vector<std::string> image_files = dataset.GetImageFiles();
        std::vector<std::string> depth_files = dataset.GetDepthFiles();
        std::vector<SE3> poses = dataset.GetPoses();
        std::vector<double> timestamps = dataset.GetTimestamps();

        cam = dataset.GetCamera();
        w = dataset.GetWidth();
        h = dataset.GetHeight();
        f = dataset.GetDepthFactor();

        InitEGL();

        int src = 0;
        int dst = 50;

        int operation = cv::MORPH_CLOSE;
        int morph_size = 5;
        int morph_elem = cv::MORPH_ELLIPSE;

        cv::Mat element = cv::getStructuringElement(morph_elem,
                                                    cv::Size(2 * morph_size + 1, 2 * morph_size + 1),
                                                    cv::Point(morph_size, morph_size));

        image_src_CV = cv::imread(image_files[src], cv::IMREAD_GRAYSCALE);
        depth_src_CV = cv::imread(depth_files[src], cv::IMREAD_GRAYSCALE);
        cv::morphologyEx(depth_src_CV, depth_src_CV, operation, element);
        pose_src = poses[src].inverse();

        image_dst_CV = cv::imread(image_files[dst], cv::IMREAD_GRAYSCALE);
        depth_dst_CV = cv::imread(depth_files[dst], cv::IMREAD_GRAYSCALE);
        cv::morphologyEx(depth_dst_CV, depth_dst_CV, operation, element);
        pose_dst = poses[dst].inverse();

        image_src_CV.convertTo(image_src_CV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
        depth_src_CV.convertTo(depth_src_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
        depth_src_CV /= dataset.GetDepthFactor();
        depth_src_CV *= 100.0;

        image_dst_CV.convertTo(image_dst_CV, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
        depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
        depth_dst_CV /= dataset.GetDepthFactor();
        depth_dst_CV *= 100.0;

        std::vector<Vec2> tex_coords = UniformTexCoords(32, 32);
        for (Vec2 tex_coord : tex_coords)
        {
            Vec2 img_coord = Vec2(tex_coord(0) * (w - 1), tex_coord(1) * (h - 1));
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
    }

    int w, h;
    float f;
    Camera cam;

    cv::Mat image_src_CV;
    cv::Mat depth_src_CV;
    SE3 pose_src;

    cv::Mat image_dst_CV;
    cv::Mat depth_dst_CV;
    SE3 pose_dst;

    std::vector<float> texcoords;
    std::vector<float> vertices;
    std::vector<float> weights;
};

TEST_F(DataLoaderOneFrame, TestDepthRenderer)
{
    int in_lvl = 1;
    int out_lvl = 1;

    int out_w = w / std::pow(2, out_lvl);
    int out_h = h / std::pow(2, out_lvl);

    MeshCPU mesh_cpu(vertices, texcoords, weights);
    TextureCPU<ImageType> input_cpu(w, h, 0);
    TextureCPU<float> depth_cpu(w, h, 0);
    input_cpu.FromCPU(0, (ImageType *)image_src_CV.data);

    DepthRendererCPU renderer_cpu;
    renderer_cpu.Render(mesh_cpu, pose_dst * pose_src.inverse(), cam, input_cpu, depth_cpu, in_lvl, out_lvl);

    cv::Mat output_cpu_CV = cv::Mat(out_h, out_w, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_cpu.ToCPU(out_lvl, (float *)output_cpu_CV.data);
    // float cpu_error = ComputeImageError<float>(depth_dst_CV, output_cpu_CV, depth_cpu.nodata());

    MeshGL mesh_gl(vertices, texcoords, weights);
    TextureGL<ImageType> input_gl(w, h, 0);
    TextureGL<float> depth_gl(w, h, 0);
    input_gl.FromCPU(0, (ImageType *)image_src_CV.data);

    DepthRendererGL renderer_gl;
    renderer_gl.Render(mesh_gl, pose_dst * pose_src.inverse(), cam, input_gl, depth_gl, in_lvl, out_lvl);

    cv::Mat output_gl_CV = cv::Mat(out_h, out_w, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_gl.ToCPU(out_lvl, (float *)output_gl_CV.data);
    // float gl_error = ComputeImageError<float>(depth_dst_CV, output_gl_CV, depth_gl.nodata());

    // float cpu_gl_error = ComputeImageError<float>(output_cpu_CV, output_gl_CV, depth_gl.nodata());

    cv::normalize(depth_dst_CV, depth_dst_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_cpu_CV, output_cpu_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_gl_CV, output_gl_CV, 0, 255, cv::NORM_MINMAX);
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_cpu_CV.convertTo(output_cpu_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_gl_CV.convertTo(output_gl_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    cv::imwrite("depthrender_input.png", depth_dst_CV);
    cv::imwrite("depthrenderercpu_output.png", output_cpu_CV);
    cv::imwrite("depthrenderergl_output.png", output_gl_CV);

    // EXPECT_NEAR(cpu_error, 0.0f, 0.5f);
    // EXPECT_NEAR(gl_error, 0.0f, 0.5f);
    // EXPECT_NEAR(cpu_gl_error, 0.0f, 0.5f);
}

TEST_F(DataLoaderOneFrame, TestImageRenderer)
{
    int in_lvl = 1;
    int out_lvl = 1;

    int out_w = w / std::pow(2, out_lvl);
    int out_h = h / std::pow(2, out_lvl);

    MeshCPU mesh_cpu(vertices, texcoords, weights);
    TextureCPU<ImageType> input_cpu(w, h, 0);
    TextureCPU<ImageType> output_cpu(w, h, 0);
    input_cpu.FromCPU(0, (ImageType *)image_src_CV.data);
    ImageRendererCPU renderer_cpu;
    renderer_cpu.Render(mesh_cpu, pose_dst * pose_src.inverse(), cam, input_cpu, output_cpu, in_lvl, out_lvl);
    cv::Mat output_cpu_CV = cv::Mat(out_h, out_w, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    output_cpu.ToCPU(out_lvl, (ImageType *)output_cpu_CV.data);
    //float cpu_error = ComputeImageError<ImageType>(image_dst_CV, output_cpu_CV, output_cpu.nodata());

    MeshGL mesh_gl(vertices, texcoords, weights);
    TextureGL<ImageType> input_gl(w, h, 0);
    TextureGL<ImageType> output_gl(w, h, 0);
    input_gl.FromCPU(0, (ImageType *)image_src_CV.data);
    ImageRendererGL renderer_gl;
    renderer_gl.Render(mesh_gl, pose_dst * pose_src.inverse(), cam, input_gl, output_gl, in_lvl, out_lvl);
    cv::Mat output_gl_CV = cv::Mat(out_h, out_w, GetOpenCVFormat(GetTypeIndex<ImageType>(), 1));
    output_gl.ToCPU(out_lvl, (ImageType *)output_gl_CV.data);
    //float gl_error = ComputeImageError<ImageType>(image_dst_CV, output_gl_CV, output_gl.nodata());

    float cpu_gl_error = ComputeImageError<ImageType>(output_cpu_CV, output_gl_CV, output_gl.nodata());

    cv::normalize(image_dst_CV, image_dst_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_cpu_CV, output_cpu_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_gl_CV, output_gl_CV, 0, 255, cv::NORM_MINMAX);
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_cpu_CV.convertTo(output_cpu_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_gl_CV.convertTo(output_gl_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    cv::imwrite("imagerender_input.png", image_dst_CV);
    cv::imwrite("imagerenderercpu_output.png", output_cpu_CV);
    cv::imwrite("imagerenderergl_output.png", output_gl_CV);

    //EXPECT_NEAR(cpu_error, 0.0f, 60.0f);
    //EXPECT_NEAR(gl_error, 0.0f, 60.0f);
    EXPECT_NEAR(cpu_gl_error, 0.0f, 0.5f);
}

TEST_F(DataLoaderOneFrame, TestDIDxyRenderer)
{
    int in_lvl = 1;
    int out_lvl = 1;

    int out_w = w / std::pow(2, out_lvl);
    int out_h = h / std::pow(2, out_lvl);

    MeshCPU mesh_cpu(vertices, texcoords, weights);
    TextureCPU<ImageType> input_cpu(w, h, 0);
    TextureCPU<Vec3> output_cpu(w, h, Vec3(0.0f, 0.0f, 0.0f));
    input_cpu.FromCPU(0, (ImageType *)image_src_CV.data);
    DIDxyRendererCPU renderer_cpu;
    renderer_cpu.Render(mesh_cpu, SE3(), cam, input_cpu, output_cpu, in_lvl, out_lvl);
    cv::Mat output_cpu_CV = cv::Mat(out_h, out_w, GetOpenCVFormat(GetTypeIndex<float>(), 3));
    output_cpu.ToCPU(out_lvl, (Vec3 *)output_cpu_CV.data);
    // float cpu_error = ComputeImageError<Vec2>(depth_dst_CV, output_cpu_CV, output_cpu.nodata());

    MeshGL mesh_gl(vertices, texcoords, weights);
    TextureGL<ImageType> input_gl(w, h, 0);
    TextureGL<Vec3> output_gl(w, h, Vec3(0.0f, 0.0f, 0.0f));
    input_gl.FromCPU(0, (ImageType *)image_src_CV.data);
    DIDxyRendererGL renderer_gl;
    renderer_gl.Render(mesh_gl, SE3(), cam, input_gl, output_gl, in_lvl, out_lvl);
    cv::Mat output_gl_CV = cv::Mat(out_h, out_w, GetOpenCVFormat(GetTypeIndex<float>(), 3));
    output_gl.ToCPU(out_lvl, (Vec3 *)output_gl_CV.data);
    // float gl_error = ComputeImageError<Vec2>(depth_dst_CV, output_gl_CV, output_gl.nodata());

    // float cpu_gl_error = ComputeImageError<float>(output_cpu_CV, output_gl_CV, output_gl.nodata());

    // cv::normalize(depth_dst_CV, depth_dst_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_cpu_CV, output_cpu_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_gl_CV, output_gl_CV, 0, 255, cv::NORM_MINMAX);
    // depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_cpu_CV.convertTo(output_cpu_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 3));
    output_gl_CV.convertTo(output_gl_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 3));
    // cv::imwrite("depthrender_input.png", depth_dst_CV);
    cv::imwrite("didxyrenderercpu_output.png", output_cpu_CV);
    cv::imwrite("didxyrenderergl_output.png", output_gl_CV);

    // EXPECT_NEAR(cpu_error, 0.0f, 0.5f);
    // EXPECT_NEAR(gl_error, 0.0f, 0.5f);
    // EXPECT_NEAR(cpu_gl_error, 0.0f, 0.5f);
}

TEST_F(DataLoaderOneFrame, TestJtraRenderer)
{
    int in_lvl = 1;
    int out_lvl = 1;

    int out_w = w / std::pow(2, out_lvl);
    int out_h = h / std::pow(2, out_lvl);

    MeshCPU mesh_cpu(vertices, texcoords, weights);
    TextureCPU<ImageType> input_cpu(w, h, 0);
    TextureCPU<Vec3> didxy_cpu(w, h, Vec3(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3> jtra_cpu(w, h, Vec3(0.0f, 0.0f, 0.0f));
    input_cpu.FromCPU(0, (ImageType *)image_src_CV.data);

    DIDxyRendererCPU didxy_renderer_cpu;
    JtraRendererCPU jtra_renderer_cpu;

    didxy_renderer_cpu.Render(mesh_cpu, SE3(), cam, input_cpu, didxy_cpu, in_lvl, out_lvl);
    jtra_renderer_cpu.Render(mesh_cpu, pose_dst * pose_src.inverse(), cam, didxy_cpu, jtra_cpu, out_lvl, out_lvl);

    cv::Mat output_cpu_CV = cv::Mat(out_h, out_w, GetOpenCVFormat(GetTypeIndex<float>(), 3));
    jtra_cpu.ToCPU(out_lvl, (Vec3 *)output_cpu_CV.data);
    // float cpu_error = ComputeImageError<Vec2>(depth_dst_CV, output_cpu_CV, output_cpu.nodata());

    MeshGL mesh_gl(vertices, texcoords, weights);
    TextureGL<ImageType> input_gl(w, h, 0);
    TextureGL<Vec3> didxy_gl(w, h, Vec3(0.0f, 0.0f, 0.0f));
    TextureGL<Vec3> jtra_gl(w, h, Vec3(0.0f, 0.0f, 0.0f));
    input_gl.FromCPU(0, (ImageType *)image_src_CV.data);

    DIDxyRendererGL didxy_renderer_gl;
    JtraRendererGL jtra_renderer_gl;

    didxy_renderer_gl.Render(mesh_gl, SE3(), cam, input_gl, didxy_gl, in_lvl, out_lvl);
    jtra_renderer_gl.Render(mesh_gl, pose_dst * pose_src.inverse(), cam, didxy_gl, jtra_gl, out_lvl, out_lvl);

    cv::Mat output_gl_CV = cv::Mat(out_h, out_w, GetOpenCVFormat(GetTypeIndex<float>(), 3));
    jtra_gl.ToCPU(out_lvl, (Vec3 *)output_gl_CV.data);
    // float gl_error = ComputeImageError<Vec2>(depth_dst_CV, output_gl_CV, output_gl.nodata());

    // float cpu_gl_error = ComputeImageError<float>(output_cpu_CV, output_gl_CV, output_gl.nodata());

    // cv::normalize(depth_dst_CV, depth_dst_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_cpu_CV, output_cpu_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_gl_CV, output_gl_CV, 0, 255, cv::NORM_MINMAX);
    // depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_cpu_CV.convertTo(output_cpu_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 3));
    output_gl_CV.convertTo(output_gl_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 3));
    // cv::imwrite("depthrender_input.png", depth_dst_CV);
    cv::imwrite("jtrarenderercpu_output.png", output_cpu_CV);
    cv::imwrite("jtrarenderergl_output.png", output_gl_CV);

    // EXPECT_NEAR(cpu_error, 0.0f, 0.5f);
    // EXPECT_NEAR(gl_error, 0.0f, 0.5f);
    // EXPECT_NEAR(cpu_gl_error, 0.0f, 0.5f);
}

TEST_F(DataLoaderOneFrame, TestJrotRenderer)
{
    int in_lvl = 1;
    int out_lvl = 1;

    int out_w = w / std::pow(2, out_lvl);
    int out_h = h / std::pow(2, out_lvl);

    MeshCPU mesh_cpu(vertices, texcoords, weights);
    TextureCPU<ImageType> input_cpu(w, h, 0);
    TextureCPU<Vec3> didxy_cpu(w, h, Vec3(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3> jtra_cpu(w, h, Vec3(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3> jrot_cpu(w, h, Vec3(0.0f, 0.0f, 0.0f));
    input_cpu.FromCPU(0, (ImageType *)image_src_CV.data);

    DIDxyRendererCPU didxy_renderer_cpu;
    JtraRendererCPU jtra_renderer_cpu;
    JrotRendererCPU jrot_renderer_cpu;

    didxy_renderer_cpu.Render(mesh_cpu, SE3(), cam, input_cpu, didxy_cpu, in_lvl, out_lvl);
    jtra_renderer_cpu.Render(mesh_cpu, pose_dst * pose_src.inverse(), cam, didxy_cpu, jtra_cpu, in_lvl, out_lvl);
    jtra_renderer_cpu.Render(mesh_cpu, SE3(), cam, jtra_cpu, jrot_cpu, in_lvl, out_lvl);

    cv::Mat output_cpu_CV = cv::Mat(out_h, out_w, GetOpenCVFormat(GetTypeIndex<float>(), 3));
    jrot_cpu.ToCPU(out_lvl, (Vec3 *)output_cpu_CV.data);
    // float cpu_error = ComputeImageError<Vec2>(depth_dst_CV, output_cpu_CV, output_cpu.nodata());

    MeshGL mesh_gl(vertices, texcoords, weights);
    TextureGL<ImageType> input_gl(w, h, 0);
    TextureGL<Vec3> didxy_gl(w, h, Vec3(0.0f, 0.0f, 0.0f));
    TextureGL<Vec3> jtra_gl(w, h, Vec3(0.0f, 0.0f, 0.0f));
    TextureGL<Vec3> jrot_gl(w, h, Vec3(0.0f, 0.0f, 0.0f));
    input_gl.FromCPU(0, (ImageType *)image_src_CV.data);

    DIDxyRendererGL didxy_renderer_gl;
    JtraRendererGL jtra_renderer_gl;
    JrotRendererGL jrot_renderer_gl;

    didxy_renderer_gl.Render(mesh_gl, SE3(), cam, input_gl, didxy_gl, in_lvl, out_lvl);
    jtra_renderer_gl.Render(mesh_gl, pose_dst * pose_src.inverse(), cam, didxy_gl, jtra_gl, in_lvl, out_lvl);
    jrot_renderer_gl.Render(mesh_gl, SE3(), cam, jtra_gl, jrot_gl, in_lvl, out_lvl);

    cv::Mat output_gl_CV = cv::Mat(out_h, out_w, GetOpenCVFormat(GetTypeIndex<float>(), 3));
    jrot_gl.ToCPU(out_lvl, (Vec3 *)output_gl_CV.data);
    // float gl_error = ComputeImageError<Vec2>(depth_dst_CV, output_gl_CV, output_gl.nodata());

    // float cpu_gl_error = ComputeImageError<float>(output_cpu_CV, output_gl_CV, output_gl.nodata());

    // cv::normalize(depth_dst_CV, depth_dst_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_cpu_CV, output_cpu_CV, 0, 255, cv::NORM_MINMAX);
    cv::normalize(output_gl_CV, output_gl_CV, 0, 255, cv::NORM_MINMAX);
    // depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_cpu_CV.convertTo(output_cpu_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 3));
    output_gl_CV.convertTo(output_gl_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 3));
    // cv::imwrite("depthrender_input.png", depth_dst_CV);
    cv::imwrite("jrotrenderercpu_output.png", output_cpu_CV);
    cv::imwrite("jrotrenderergl_output.png", output_gl_CV);

    // EXPECT_NEAR(cpu_error, 0.0f, 0.5f);
    // EXPECT_NEAR(gl_error, 0.0f, 0.5f);
    // EXPECT_NEAR(cpu_gl_error, 0.0f, 0.5f);
}
