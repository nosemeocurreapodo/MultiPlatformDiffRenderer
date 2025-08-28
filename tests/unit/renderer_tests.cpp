#include <gtest/gtest.h>
#include "../common/test_framework.h"

#ifdef COMPILE_GL
// GL context setup for typed tests
class GLContextEnv : public ::testing::Environment
{
public:
    void SetUp() override { InitEGL(); }
    void TearDown() override {}
};
static ::testing::Environment *const gl_env = ::testing::AddGlobalTestEnvironment(new GLContextEnv());
#endif

// Backend trait structs to unify CPU and GL testing
struct CPUBackendTraits
{
    using MeshT = MeshCPU;
    template <typename T>
    using TextureT = TextureCPU<T>;
    using DepthRendererT = DepthRendererCPU;
    using ImageRendererT = ImageRendererCPU;
    using DIDxyRendererT = DIDxyRendererCPU;
    using JtraRendererT = JtraRendererCPU;
    static const char *Name() { return "CPU"; }
};

#ifdef COMPILE_GL
struct GLBackendTraits
{
    using MeshT = MeshGL;
    template <typename T>
    using TextureT = TextureGL<T>;
    using DepthRendererT = DepthRendererGL;
    using ImageRendererT = ImageRendererGL;
    using DIDxyRendererT = DIDxyRendererGL;
    using JtraRendererT = JtraRendererGL;
    static const char *Name() { return "GL"; }
};
#endif

template <typename Backend>
class RendererTypedTests : public RendererTestBase
{
protected:
    void SetUp() override
    {
        RendererTestBase::SetUp();
    }
};

TYPED_TEST_SUITE_P(RendererTypedTests);

// Depth renderer
TYPED_TEST_P(RendererTypedTests, DepthRendererBasicFunctionality)
{
    using Traits = TypeParam;
    const int in_lvl = 0, out_lvl = 0;

    typename Traits::MeshT mesh(this->vertices_, this->texcoords_, this->weights_);
    typename Traits::template TextureT<float> output(this->w_, this->h_, -1.0f);

    typename Traits::DepthRendererT renderer;
    SE3 pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    ASSERT_NO_THROW(renderer.Render(mesh, pose_transform, this->cam_, in_lvl, out_lvl, output));

    cv::Mat result = this->DownloadTexture(output, out_lvl, CV_32FC1);

    cv::Scalar mean_val, std_val;
    cv::meanStdDev(result, mean_val, std_val);
    EXPECT_GT(mean_val[0], 0.0) << "Mean depth should be positive";
    EXPECT_LT(mean_val[0], 1000.0) << "Mean depth should be reasonable";
    EXPECT_GT(std_val[0], 0.0) << "Depth should have variation";

    int valid_pixels = 0;
    for (int y = 0; y < result.rows; ++y)
    {
        for (int x = 0; x < result.cols; ++x)
        {
            float depth = result.at<float>(y, x);
            if (depth > 0.0f)
            {
                valid_pixels++;
                EXPECT_LT(depth, 1000.0f) << "Depth value too large at (" << x << "," << y << ")";
            }
        }
    }
    EXPECT_GT(valid_pixels, result.total() * 0.1) << "Too few valid pixels rendered";
}

// Image renderer
TYPED_TEST_P(RendererTypedTests, ImageRendererBasicFunctionality)
{
    using Traits = TypeParam;
    const int in_lvl = 0, out_lvl = 0;

    typename Traits::MeshT mesh(this->vertices_, this->texcoords_, this->weights_);
    typename Traits::template TextureT<float> input(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<float> output(this->w_, this->h_, -1.0f);

    this->UploadMatToTexture(input, 0, this->image_src_cv_);

    typename Traits::ImageRendererT renderer;
    SE3 pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    ASSERT_NO_THROW(renderer.Render(mesh, pose_transform, this->cam_, in_lvl, out_lvl, input, output));

    cv::Mat result = this->DownloadTexture(output, out_lvl, CV_32FC1);
    cv::Scalar mean_val, std_val;
    cv::meanStdDev(result, mean_val, std_val);
    EXPECT_GE(mean_val[0], 0.0) << "Mean intensity should be non-negative";
    EXPECT_LE(mean_val[0], 255.0) << "Mean intensity should be reasonable";
}

// DIDxy renderer
TYPED_TEST_P(RendererTypedTests, DIDxyRendererBasicFunctionality)
{
    using Traits = TypeParam;
    const int in_lvl = 0, out_lvl = 0;

    std::vector<float> quad_pos, quad_uv, quad_weights;
    this->CreateScreenQuad(quad_pos, quad_uv, quad_weights);

    typename Traits::MeshT mesh(quad_pos, quad_uv, quad_weights);
    typename Traits::template TextureT<float> input(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<Vec3> output(this->w_, this->h_, Vec3(0.0f, 0.0f, 0.0f));

    this->UploadMatToTexture(input, 0, this->image_src_cv_);

    typename Traits::DIDxyRendererT renderer;
    ASSERT_NO_THROW(renderer.Render(mesh, SE3(), this->cam_, in_lvl, out_lvl, input, output));

    cv::Mat result = this->DownloadTexture(output, out_lvl, CV_32FC3);

    cv::Mat channels[3];
    cv::split(result, channels);

    cv::Scalar std_dx, std_dy;
    cv::meanStdDev(channels[0], cv::Scalar(), std_dx);
    cv::meanStdDev(channels[1], cv::Scalar(), std_dy);
    EXPECT_GT(std_dx[0], 0.01) << "X gradient should have variation";
    EXPECT_GT(std_dy[0], 0.01) << "Y gradient should have variation";
}

// Jtra renderer depends on DIDxy
TYPED_TEST_P(RendererTypedTests, JtraRendererBasicFunctionality)
{
    using Traits = TypeParam;
    const int in_lvl = 0, out_lvl = 0;

    std::vector<float> quad_pos, quad_uv, quad_weights;
    this->CreateScreenQuad(quad_pos, quad_uv, quad_weights);

    typename Traits::MeshT mesh_img(quad_pos, quad_uv, quad_weights);
    typename Traits::MeshT mesh(this->vertices_, this->texcoords_, this->weights_);

    typename Traits::template TextureT<float> input(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<Vec3> didxy(this->w_, this->h_, Vec3(0.0f, 0.0f, 0.0f));
    typename Traits::template TextureT<Vec3> jtra(this->w_, this->h_, Vec3(0.0f, 0.0f, 0.0f));

    this->UploadMatToTexture(input, 0, this->image_dst_cv_);

    typename Traits::DIDxyRendererT didxy_renderer;
    typename Traits::JtraRendererT jtra_renderer;

    SE3 pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    ASSERT_NO_THROW(didxy_renderer.Render(mesh_img, SE3(), this->cam_, in_lvl, out_lvl, input, didxy));
    ASSERT_NO_THROW(jtra_renderer.Render(mesh, pose_transform, this->cam_, in_lvl, out_lvl, didxy, jtra));

    cv::Mat result = this->DownloadTexture(jtra, out_lvl, CV_32FC3);
    cv::Scalar mean_jtra = cv::mean(result);
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_LT(std::abs(mean_jtra[i]), 1000.0) << "Jacobian component " << i << " too large";
    }
}

// Error handling and edge cases
TYPED_TEST_P(RendererTypedTests, ErrorHandlingAndEdgeCases)
{
    using Traits = TypeParam;
    const int in_lvl = 0, out_lvl = 0;

    std::vector<float> empty_vertices, empty_texcoords, empty_weights;
    typename Traits::MeshT empty_mesh(empty_vertices, empty_texcoords, empty_weights);
    typename Traits::template TextureT<float> output(this->w_, this->h_, -1.0f);

    typename Traits::DepthRendererT renderer;

    ASSERT_NO_THROW(renderer.Render(empty_mesh, SE3(), this->cam_, in_lvl, out_lvl, output));

    typename Traits::MeshT mesh(this->vertices_, this->texcoords_, this->weights_);
    ASSERT_NO_THROW(renderer.Render(mesh, SE3(), this->cam_, in_lvl, out_lvl, output));

    if (this->w_ >= 4 && this->h_ >= 4)
    {
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), this->cam_, 0, 1, output));
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), this->cam_, 1, 0, output));
    }
}

// Resource/memory management
TYPED_TEST_P(RendererTypedTests, ResourceManagement)
{
    using Traits = TypeParam;
    const int iterations = 10;
    for (int i = 0; i < iterations; ++i)
    {
        typename Traits::MeshT mesh(this->vertices_, this->texcoords_, this->weights_);
        typename Traits::template TextureT<float> output(this->w_, this->h_, -1.0f);
        typename Traits::DepthRendererT renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), this->cam_, 0, 0, output));
    }
    SUCCEED();
}

// Numerical precision & determinism
TYPED_TEST_P(RendererTypedTests, NumericalPrecisionDeterminism)
{
    using Traits = TypeParam;
    const int in_lvl = 0, out_lvl = 0;
    const int iterations = 5;

    typename Traits::MeshT mesh(this->vertices_, this->texcoords_, this->weights_);

    typename Traits::DepthRendererT renderer;
    SE3 pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    std::vector<cv::Mat> results;
    results.reserve(iterations);
    for (int i = 0; i < iterations; ++i)
    {
        typename Traits::template TextureT<float> output(this->w_, this->h_, -1.0f);
        renderer.Render(mesh, pose_transform, this->cam_, in_lvl, out_lvl, output);
        results.push_back(this->DownloadTexture(output, out_lvl, CV_32FC1));
    }

    for (int i = 1; i < iterations; ++i)
    {
        double error = this->template ComputeL2Error<float>(results[0], results[i], -1.0f);
        EXPECT_LT(error, 1e-6) << "Results should be deterministic, iteration " << i;
    }
}

// Varying texture sizes (generalized framebuffer/resize validation)
TYPED_TEST_P(RendererTypedTests, VaryingTextureSizes)
{
    using Traits = TypeParam;
    typename Traits::MeshT mesh(this->vertices_, this->texcoords_, this->weights_);
    const std::vector<std::pair<int, int>> sizes = {{64, 64}, {128, 128}, {256, 256}};
    for (const auto &size : sizes)
    {
        int w = size.first, h = size.second;

        typename Traits::template TextureT<float> output(w, h, -1.0f);

        typename Traits::DepthRendererT renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), this->cam_, 0, 0, output));
        cv::Mat result = this->DownloadTexture(output, 0, CV_32FC1);
        EXPECT_EQ(result.cols, w);
        EXPECT_EQ(result.rows, h);
    }
}

REGISTER_TYPED_TEST_SUITE_P(
    RendererTypedTests,
    DepthRendererBasicFunctionality,
    ImageRendererBasicFunctionality,
    DIDxyRendererBasicFunctionality,
    JtraRendererBasicFunctionality,
    ErrorHandlingAndEdgeCases,
    ResourceManagement,
    NumericalPrecisionDeterminism,
    VaryingTextureSizes);

using TestBackends = ::testing::Types<CPUBackendTraits
#ifdef COMPILE_GL
                                      ,
                                      GLBackendTraits
#endif
                                      >;

INSTANTIATE_TYPED_TEST_SUITE_P(AllBackends, RendererTypedTests, TestBackends);
