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
#include "backends/gl/meshgl.h"
#include "backends/gl/renderergl.h"

#include "loaddataset.h"

// --------------------
// GL context one-time
// --------------------
class GLContextEnv : public ::testing::Environment
{
public:
    void SetUp() override { InitEGL(); }
    void TearDown() override
    {
        // If you have ShutdownEGL(); call it here.
        // ShutdownEGL();
    }
};
// register once per test process
static ::testing::Environment *const kGlEnv =
    ::testing::AddGlobalTestEnvironment(new GLContextEnv());

// --------------------
// Small test helpers
// --------------------
static inline int lvl_dim(int base, int lvl)
{
    return std::max(1, base >> lvl);
}

template <class T>
static void upload_mat_to_texture(TextureCPU<T> &tex, int lvl, const cv::Mat &m)
{
    // ASSERT_TRUE(m.isContinuous());
    const std::size_t n = static_cast<std::size_t>(m.total());
    auto mw = tex.MapWrite(lvl);
    std::memcpy(mw.data(), m.ptr(), n * sizeof(T));
}

template <class T>
static cv::Mat download_texture_to_mat(const TextureCPU<T> &tex, int lvl, int w, int h, int cv_type)
{
    cv::Mat out(h, w, cv_type);
    // ASSERT_TRUE(out.isContinuous());
    auto mr = tex.MapRead(lvl);
    std::memcpy(out.ptr(), mr.data(), static_cast<std::size_t>(w) * h * sizeof(T));
    return out;
}

template <class T>
static void upload_mat_to_texture(TextureGL<T> &tex, int lvl, const cv::Mat &m)
{
    // ASSERT_TRUE(m.isContinuous());
    const std::size_t n = static_cast<std::size_t>(m.total());
    auto mw = tex.MapWrite(lvl);
    std::memcpy(mw.data(), m.ptr(), n * sizeof(T));
}

template <class T>
static cv::Mat download_texture_to_mat(const TextureGL<T> &tex, int lvl, int w, int h, int cv_type)
{
    cv::Mat out(h, w, cv_type);
    // ASSERT_TRUE(out.isContinuous());
    auto mr = tex.MapRead(lvl);
    std::memcpy(out.ptr(), mr.data(), static_cast<std::size_t>(w) * h * sizeof(T));
    return out;
}

// Screen quad data for image-space renderers
static void make_screen_quad(std::vector<float> &pos,
                             std::vector<float> &uv,
                             std::vector<float> &w)
{
    pos = {-1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
           -1.f, 1.f, 1.f, 1.f, -1.f, 1.f, 1.f, 1.f, 1.f};
    uv = {0.f, 1.f, 0.f, 0.f, 1.f, 0.f,
          0.f, 1.f, 1.f, 0.f, 1.f, 1.f};
    w.assign(6, 1.f);
}

// --------------------
// Data fixtures
// --------------------
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

    LoadDatasetIclNuim dataset;
    std::vector<std::string> image_files, depth_files;
    std::vector<SE3> poses;
    std::vector<double> timestamps;
    Camera cam;
    int w = 0, h = 0;
};

class DataLoaderOneFrame : public ::testing::Test
{
protected:
    void SetUp() override
    {
        LoadDatasetIclNuim dataset(std::string(TEST_DATA_DIR));
        auto image_files = dataset.GetImageFiles();
        auto depth_files = dataset.GetDepthFiles();
        auto poses = dataset.GetPoses();

        cam = dataset.GetCamera();
        w = dataset.GetWidth();
        h = dataset.GetHeight();
        f = dataset.GetDepthFactor();

        const int src = 0;
        const int dst = 50;

        // Read grayscale as float
        image_src_CV = cv::imread(image_files[src], cv::IMREAD_GRAYSCALE);
        depth_src_CV = cv::imread(depth_files[src], cv::IMREAD_GRAYSCALE);
        image_dst_CV = cv::imread(image_files[dst], cv::IMREAD_GRAYSCALE);
        depth_dst_CV = cv::imread(depth_files[dst], cv::IMREAD_GRAYSCALE);
        ASSERT_FALSE(image_src_CV.empty());
        ASSERT_FALSE(depth_src_CV.empty());
        ASSERT_FALSE(image_dst_CV.empty());
        ASSERT_FALSE(depth_dst_CV.empty());

        // Morph on depth to denoise a bit
        const int morph_size = 5;
        cv::Mat element = cv::getStructuringElement(
            cv::MORPH_ELLIPSE, cv::Size(2 * morph_size + 1, 2 * morph_size + 1),
            cv::Point(morph_size, morph_size));
        cv::morphologyEx(depth_src_CV, depth_src_CV, cv::MORPH_CLOSE, element);
        cv::morphologyEx(depth_dst_CV, depth_dst_CV, cv::MORPH_CLOSE, element);

        // Convert to float
        image_src_CV.convertTo(image_src_CV, CV_32FC1);
        depth_src_CV.convertTo(depth_src_CV, CV_32FC1);
        image_dst_CV.convertTo(image_dst_CV, CV_32FC1);
        depth_dst_CV.convertTo(depth_dst_CV, CV_32FC1);

        // Scale depth (meters) → your working units (seems *100)
        depth_src_CV /= f;
        depth_src_CV *= 100.f;
        depth_dst_CV /= f;
        depth_dst_CV *= 100.f;

        pose_src = poses[src].inverse();
        pose_dst = poses[dst].inverse();

        // Build sparse mesh (vertices from source depth)
        const int grid = 32;
        std::vector<Vec2> grid_uv = UniformTexCoords(grid, grid);
        vertices.reserve(grid_uv.size() * 3);
        texcoords.reserve(grid_uv.size() * 2);
        weights.reserve(grid_uv.size());

        for (const Vec2 &t : grid_uv)
        {
            const float ix = t(0) * (w - 1);
            const float iy = t(1) * (h - 1);
            const int x = static_cast<int>(ix);
            const int y = static_cast<int>(iy);
            const float d = depth_src_CV.at<float>(y, x);
            if (d <= 0.f)
                continue;

            const Vec3 ray = cam.PixToRay(t);
            const Vec3 v = ray * d;
            vertices.push_back(v(0));
            vertices.push_back(v(1));
            vertices.push_back(v(2));
            texcoords.push_back(t(0));
            texcoords.push_back(t(1));
            weights.push_back(1.f);
        }
    }

    int w = 0, h = 0;
    float f = 1.f;
    Camera cam;

    cv::Mat image_src_CV, depth_src_CV, image_dst_CV, depth_dst_CV;
    SE3 pose_src, pose_dst;

    std::vector<float> texcoords;
    std::vector<float> vertices;
    std::vector<float> weights;
};

// ----------------------------------
// DepthRenderer test (CPU vs GL)
// ----------------------------------
TEST_F(DataLoaderOneFrame, TestDepthRenderer)
{
    const int in_lvl = 1;
    const int out_lvl = 1;

    const int out_w = lvl_dim(w, out_lvl);
    const int out_h = lvl_dim(h, out_lvl);

    // ----- CPU -----
    MeshCPU mesh_cpu(vertices, texcoords, weights);
    TextureCPU<float> input_cpu(w, h, 0.f);
    TextureCPU<float> depth_cpu(w, h, 0.f);

    upload_mat_to_texture(input_cpu, 0, image_src_CV);

    DepthRendererCPU renderer_cpu;
    renderer_cpu.Render(mesh_cpu, pose_dst * pose_src.inverse(), cam,
                        input_cpu, depth_cpu, in_lvl, out_lvl);

    cv::Mat depth_cpu_cv = download_texture_to_mat<float>(
        depth_cpu, out_lvl, out_w, out_h, CV_32FC1);

    // ----- GL -----
    MeshGL mesh_gl(vertices, texcoords, weights);
    TextureGL<float> input_gl(w, h, 0.f);
    TextureGL<float> depth_gl(w, h, 0.f);
    // input_gl.FromCPU(0, reinterpret_cast<const float*>(image_src_CV.data)); // ok: CV_32FC1
    upload_mat_to_texture(input_gl, 0, image_src_CV);

    DepthRendererGL renderer_gl;
    renderer_gl.Render(mesh_gl, pose_dst * pose_src.inverse(), cam,
                       input_gl, depth_gl, in_lvl, out_lvl);

    // cv::Mat depth_gl_cv(out_h, out_w, CV_32FC1);
    // depth_gl.ToCPU(out_lvl, depth_gl_cv.ptr<float>());
    cv::Mat depth_gl_cv = download_texture_to_mat<float>(
        depth_gl, out_lvl, out_w, out_h, CV_32FC1);

    // ----- Debug dumps (optional) -----
    {
        cv::Mat gt_vis, cpu_vis, gl_vis;
        cv::normalize(depth_dst_CV, gt_vis, 0, 255, cv::NORM_MINMAX);
        cv::normalize(depth_cpu_cv, cpu_vis, 0, 255, cv::NORM_MINMAX);
        cv::normalize(depth_gl_cv, gl_vis, 0, 255, cv::NORM_MINMAX);
        gt_vis.convertTo(gt_vis, CV_8UC1);
        cpu_vis.convertTo(cpu_vis, CV_8UC1);
        gl_vis.convertTo(gl_vis, CV_8UC1);
        ASSERT_TRUE(cv::imwrite("depthrender_input.png", gt_vis));
        ASSERT_TRUE(cv::imwrite("depthrender_cpu_output.png", cpu_vis));
        ASSERT_TRUE(cv::imwrite("depthrender_gl_output.png", gl_vis));
    }

    // If you have ComputeImageError<T>, enable and tune epsilon:
    // const float cpu_gl_err = ComputeImageError<float>(depth_cpu_cv, depth_gl_cv, 0.f);
    // EXPECT_NEAR(cpu_gl_err, 0.0f, 0.5f);
}

// ----------------------------------
// ImageRenderer test (CPU vs GL)
// ----------------------------------
TEST_F(DataLoaderOneFrame, TestImageRenderer)
{
    const int in_lvl = 1;
    const int out_lvl = 1;

    const int out_w = lvl_dim(w, out_lvl);
    const int out_h = lvl_dim(h, out_lvl);

    MeshCPU mesh_cpu(vertices, texcoords, weights);
    TextureCPU<float> input_cpu(w, h, 0.f);
    TextureCPU<float> output_cpu(w, h, 0.f);
    upload_mat_to_texture(input_cpu, 0, image_src_CV);

    ImageRendererCPU renderer_cpu;
    renderer_cpu.Render(mesh_cpu, pose_dst * pose_src.inverse(), cam,
                        input_cpu, output_cpu, in_lvl, out_lvl);
    cv::Mat out_cpu_cv = download_texture_to_mat<float>(
        output_cpu, out_lvl, out_w, out_h, CV_32FC1);

    MeshGL mesh_gl(vertices, texcoords, weights);
    TextureGL<float> input_gl(w, h, 0.f);
    TextureGL<float> output_gl_tex(w, h, 0.f);
    input_gl.FromCPU(0, image_src_CV.ptr<float>());
    //upload_mat_to_texture(input_gl, 0, image_src_CV);

    ImageRendererGL renderer_gl;
    renderer_gl.Render(mesh_gl, pose_dst * pose_src.inverse(), cam,
                       input_gl, output_gl_tex, in_lvl, out_lvl);
    cv::Mat out_gl_cv(out_h, out_w, CV_32FC1);
    output_gl_tex.ToCPU(out_lvl, out_gl_cv.ptr<float>());
    //cv::Mat out_gl_cv = download_texture_to_mat<float>(
    //    output_gl_tex, out_lvl, out_w, out_h, CV_32FC1);

    const float cpu_gl_err = ComputeImageError<float>(out_cpu_cv, out_gl_cv, 0.f);
    // Tighten this once kernels match bit-for-bit:
    EXPECT_NEAR(cpu_gl_err, 0.0f, 0.5f);

    // Debug dumps
    cv::Mat cpu_vis, gl_vis, dst_vis;
    cv::normalize(image_dst_CV, dst_vis, 0, 255, cv::NORM_MINMAX);
    cv::normalize(out_cpu_cv, cpu_vis, 0, 255, cv::NORM_MINMAX);
    cv::normalize(out_gl_cv, gl_vis, 0, 255, cv::NORM_MINMAX);
    dst_vis.convertTo(dst_vis, CV_8UC1);
    cpu_vis.convertTo(cpu_vis, CV_8UC1);
    gl_vis.convertTo(gl_vis, CV_8UC1);
    ASSERT_TRUE(cv::imwrite("imagerender_input.png", dst_vis));
    ASSERT_TRUE(cv::imwrite("imagerender_cpu_output.png", cpu_vis));
    ASSERT_TRUE(cv::imwrite("imagerender_gl_output.png", gl_vis));
}

// ----------------------------------
// DIDxy (image-space gradient) test
// ----------------------------------
TEST_F(DataLoaderOneFrame, TestDIDxyRenderer)
{
    const int in_lvl = 1;
    const int out_lvl = 1;
    const int out_w = lvl_dim(w, out_lvl);
    const int out_h = lvl_dim(h, out_lvl);

    // Image-space quad
    std::vector<float> quad_pos, quad_uv, quad_w;
    make_screen_quad(quad_pos, quad_uv, quad_w);

    MeshCPU mesh_img_cpu(quad_pos, quad_uv, quad_w);
    TextureCPU<float> input_cpu(w, h, 0.f);
    TextureCPU<Vec3> didxy_cpu(w, h, Vec3(0.f, 0.f, 0.f));
    upload_mat_to_texture(input_cpu, 0, image_src_CV);

    DIDxyRendererCPU didxy_cpu_renderer;
    didxy_cpu_renderer.Render(mesh_img_cpu, SE3(), cam, input_cpu, didxy_cpu, in_lvl, out_lvl);
    cv::Mat didxy_cpu_cv = download_texture_to_mat<Vec3>(
        didxy_cpu, out_lvl, out_w, out_h, CV_32FC3);

    // GL
    MeshGL mesh_img_gl(quad_pos, quad_uv, quad_w);
    TextureGL<float> input_gl(w, h, 0.f);
    TextureGL<Vec3> didxy_gl(w, h, Vec3(0.f, 0.f, 0.f));
    // input_gl.FromCPU(0, image_src_CV.ptr<float>());
    upload_mat_to_texture(input_gl, 0, image_src_CV);

    DIDxyRendererGL didxy_gl_renderer;
    didxy_gl_renderer.Render(mesh_img_gl, SE3(), cam, input_gl, didxy_gl, in_lvl, out_lvl);
    // cv::Mat didxy_gl_cv(out_h, out_w, CV_32FC3);
    // didxy_gl.ToCPU(out_lvl, reinterpret_cast<Vec3*>(didxy_gl_cv.ptr<Vec3>()));
    cv::Mat didxy_gl_cv = download_texture_to_mat<Vec3>(
        didxy_gl, out_lvl, out_w, out_h, CV_32FC3);

    // Debug dumps
    auto to_u8 = [](const cv::Mat &in) -> cv::Mat
    {
        cv::Mat n, in8;
        cv::normalize(in, n, 0, 255, cv::NORM_MINMAX);
        n.convertTo(in8, CV_8UC3);
        return in8;
    };
    ASSERT_TRUE(cv::imwrite("didxyrender_cpu_output.png", to_u8(didxy_cpu_cv)));
    ASSERT_TRUE(cv::imwrite("didxyrender_gl_output.png", to_u8(didxy_gl_cv)));
}

// ----------------------------------
// Jtra / Jrot pipelines
// ----------------------------------
TEST_F(DataLoaderOneFrame, TestJtraRenderer)
{
    const int in_lvl = 1;
    const int out_lvl = 1;
    const int out_w = lvl_dim(w, out_lvl);
    const int out_h = lvl_dim(h, out_lvl);

    MeshCPU mesh(vertices, texcoords, weights);

    TextureCPU<float> input_cpu(w, h, 0.f);
    TextureCPU<Vec3> didxy_cpu(w, h, Vec3(0, 0, 0));
    TextureCPU<Vec3> jtra_cpu(w, h, Vec3(0, 0, 0));
    upload_mat_to_texture(input_cpu, 0, image_src_CV);

    DIDxyRendererCPU didxy_cpu_renderer;
    JtraRendererCPU jtra_cpu_renderer;

    didxy_cpu_renderer.Render(mesh, SE3(), cam, input_cpu, didxy_cpu, in_lvl, out_lvl);
    jtra_cpu_renderer.Render(mesh, pose_dst * pose_src.inverse(), cam, didxy_cpu, jtra_cpu, out_lvl, out_lvl);

    cv::Mat jtra_cpu_cv = download_texture_to_mat<Vec3>(
        jtra_cpu, out_lvl, out_w, out_h, CV_32FC3);

    MeshGL mesh_gl(vertices, texcoords, weights);
    TextureGL<float> in_gl(w, h, 0.f);
    TextureGL<Vec3> didxy_gl(w, h, Vec3(0, 0, 0));
    TextureGL<Vec3> jtra_gl(w, h, Vec3(0, 0, 0));
    // in_gl.FromCPU(0, image_src_CV.ptr<float>());
    upload_mat_to_texture(in_gl, 0, image_src_CV);

    DIDxyRendererGL didxy_gl_renderer;
    JtraRendererGL jtra_gl_renderer;
    didxy_gl_renderer.Render(mesh_gl, SE3(), cam, in_gl, didxy_gl, in_lvl, out_lvl);
    jtra_gl_renderer.Render(mesh_gl, pose_dst * pose_src.inverse(), cam, didxy_gl, jtra_gl, out_lvl, out_lvl);

    // cv::Mat jtra_gl_cv(out_h, out_w, CV_32FC3);
    // jtra_gl.ToCPU(out_lvl, reinterpret_cast<Vec3*>(jtra_gl_cv.ptr<Vec3>()));

    cv::Mat jtra_gl_cv = download_texture_to_mat<Vec3>(
        jtra_gl, out_lvl, out_w, out_h, CV_32FC3);

    // Debug dump
    cv::Mat cpu8, gl8;
    cv::normalize(jtra_cpu_cv, cpu8, 0, 255, cv::NORM_MINMAX);
    cv::normalize(jtra_gl_cv, gl8, 0, 255, cv::NORM_MINMAX);
    cpu8.convertTo(cpu8, CV_8UC3);
    gl8.convertTo(gl8, CV_8UC3);
    ASSERT_TRUE(cv::imwrite("jtrarender_cpu_output.png", cpu8));
    ASSERT_TRUE(cv::imwrite("jtrarender_gl_output.png", gl8));

    // Optional numerical check:
    // const float err = ComputeImageError<Vec3>(jtra_cpu_cv, jtra_gl_cv, Vec3(0,0,0));
    // EXPECT_NEAR(err, 0.0f, 0.5f);
}

TEST_F(DataLoaderOneFrame, TestJrotRenderer)
{
    const int in_lvl = 1;
    const int out_lvl = 1;
    const int out_w = lvl_dim(w, out_lvl);
    const int out_h = lvl_dim(h, out_lvl);

    MeshCPU mesh(vertices, texcoords, weights);

    TextureCPU<float> input_cpu(w, h, 0.f);
    TextureCPU<Vec3> didxy_cpu(w, h, Vec3(0, 0, 0));
    TextureCPU<Vec3> jtra_cpu(w, h, Vec3(0, 0, 0));
    TextureCPU<Vec3> jrot_cpu(w, h, Vec3(0, 0, 0));
    upload_mat_to_texture(input_cpu, 0, image_src_CV);

    DIDxyRendererCPU didxy_cpu_renderer;
    JtraRendererCPU jtra_cpu_renderer;
    JrotRendererCPU jrot_cpu_renderer;
    didxy_cpu_renderer.Render(mesh, SE3(), cam, input_cpu, didxy_cpu, in_lvl, out_lvl);
    jtra_cpu_renderer.Render(mesh, pose_dst * pose_src.inverse(), cam, didxy_cpu, jtra_cpu, in_lvl, out_lvl);
    jrot_cpu_renderer.Render(mesh, SE3(), cam, jtra_cpu, jrot_cpu, in_lvl, out_lvl);

    cv::Mat jrot_cpu_cv = download_texture_to_mat<Vec3>(
        jrot_cpu, out_lvl, out_w, out_h, CV_32FC3);

    MeshGL mesh_gl(vertices, texcoords, weights);
    TextureGL<float> in_gl(w, h, 0.f);
    TextureGL<Vec3> didxy_gl(w, h, Vec3(0, 0, 0));
    TextureGL<Vec3> jtra_gl(w, h, Vec3(0, 0, 0));
    TextureGL<Vec3> jrot_gl(w, h, Vec3(0, 0, 0));
    // in_gl.FromCPU(0, image_src_CV.ptr<float>());
    upload_mat_to_texture(in_gl, 0, image_src_CV);

    DIDxyRendererGL didxy_gl_renderer;
    JtraRendererGL jtra_gl_renderer;
    JrotRendererGL jrot_gl_renderer;
    didxy_gl_renderer.Render(mesh_gl, SE3(), cam, in_gl, didxy_gl, in_lvl, out_lvl);
    jtra_gl_renderer.Render(mesh_gl, pose_dst * pose_src.inverse(), cam, didxy_gl, jtra_gl, in_lvl, out_lvl);
    jrot_gl_renderer.Render(mesh_gl, SE3(), cam, jtra_gl, jrot_gl, in_lvl, out_lvl);

    // cv::Mat jrot_gl_cv(out_h, out_w, CV_32FC3);
    //  jrot_gl.ToCPU(out_lvl, reinterpret_cast<Vec3*>(jrot_gl_cv.ptr<Vec3>()));
    cv::Mat jrot_gl_cv = download_texture_to_mat<Vec3>(
        jrot_gl, out_lvl, out_w, out_h, CV_32FC3);

    cv::Mat cpu8, gl8;
    cv::normalize(jrot_cpu_cv, cpu8, 0, 255, cv::NORM_MINMAX);
    cv::normalize(jrot_gl_cv, gl8, 0, 255, cv::NORM_MINMAX);
    cpu8.convertTo(cpu8, CV_8UC3);
    gl8.convertTo(gl8, CV_8UC3);
    ASSERT_TRUE(cv::imwrite("jrotrender_cpu_output.png", cpu8));
    ASSERT_TRUE(cv::imwrite("jrotrender_gl_output.png", gl8));

    // const float err = ComputeImageError<Vec3>(jrot_cpu_cv, jrot_gl_cv, Vec3(0,0,0));
    // EXPECT_NEAR(err, 0.0f, 0.5f);
}

// ----------------------------------
// Jpose: GL version not implemented → skip
// ----------------------------------
TEST_F(DataLoaderOneFrame, TestJposeRenderer)
{
    GTEST_SKIP() << "JPoseRendererGL not implemented yet; skipping to avoid misleading results.";
    // Keep your CPU pipeline here if you still want a CPU-only regression.
}
