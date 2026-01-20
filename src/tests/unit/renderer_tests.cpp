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
    using JPoseExpRendererT = JPoseExpRendererCPU;
    using JMapExpRendererT = JMapExpRendererCPU;
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
    using JPoseExpRendererT = JPoseExpRendererGL;
    using JMapExpRendererT = JMapExpRendererGL;
    static const char *Name() { return "GL"; }
};
#endif

#ifdef COMPILE_HLS
struct HLSBackendTraits
{
    using MeshT = MeshHLS;
    template <typename T>
    using TextureT = TextureRAM<T>;
    using DepthRendererT = DepthRendererRAM;
    using ImageRendererT = ImageRendererRAM;
    using ResidualRendererT = ResidualRendererRAM;
    using L2RendererT = L2RendererRAM;
    using DIDxyRendererT = DIDxyRendererRAM;
    using JPoseRendererT = JPoseRendererRAM;
    using JMapRendererT = JMapRendererRAM;
    static const char *Name() { return "RAM"; }
};
#endif

template <typename Backend>
class RendererTypedTests : public TwoViewTests
{
protected:
    void SetUp() override
    {
        TwoViewTests::SetUp();
    }
};

TYPED_TEST_SUITE_P(RendererTypedTests);

// Depth renderer
TYPED_TEST_P(RendererTypedTests, DepthRendererBasicFunctionality)
{
    using Traits = TypeParam;
    const int out_lvl = 0;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);
    typename Traits::template TextureT<float> output(this->w_, this->h_, -1.0f);

    typename Traits::DepthRendererT renderer;
    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    ASSERT_NO_THROW(renderer.Render(mesh, pose_transform, this->cam_, out_lvl, output));

    cv::Mat result = DownloadTextureToMat(output, out_lvl);

    cv::Mat mask = (result != -1.0f);
    cv::Scalar mean_val, std_val;
    cv::meanStdDev(result, mean_val, std_val, mask);
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
                EXPECT_LT(depth, 100000.0f) << "Depth value too large at (" << x << "," << y << ")";
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

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);
    typename Traits::template TextureT<ImageType> input(this->w_, this->h_, 0);
    typename Traits::template TextureT<ImageType> output(this->w_, this->h_, 0);

    UploadMatToTexture(input, 0, this->image_src_cv_);

    typename Traits::ImageRendererT renderer;
    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();
    Vec2<float> exposure(0.0, 0.0);

    ASSERT_NO_THROW(renderer.Render(mesh, pose_transform, exposure, this->cam_, in_lvl, out_lvl, input, output));

    cv::Mat result = DownloadTextureToMat(output, out_lvl);
    cv::Scalar mean_val, std_val;
    cv::Mat mask = (result != -1.0f);
    cv::meanStdDev(result, mean_val, std_val, mask);
    EXPECT_GE(mean_val[0], 0.0) << "Mean intensity should be non-negative";
    EXPECT_LE(mean_val[0], 255.0) << "Mean intensity should be reasonable";
}

// DIDxy renderer
TYPED_TEST_P(RendererTypedTests, DIDxyRendererBasicFunctionality)
{
    using Traits = TypeParam;
    const int in_lvl = 0, out_lvl = 0;

    typename Traits::MeshT mesh(this->screen_vertex_, this->screen_indices_, true, true, false);
    typename Traits::template TextureT<ImageType> input(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> output(this->w_, this->h_, Vec3<float>(0.0f, 0.0f, 0.0f));

    UploadMatToTexture(input, 0, this->image_src_cv_);

    typename Traits::DIDxyRendererT renderer;
    ASSERT_NO_THROW(renderer.Render(mesh, in_lvl, out_lvl, input, output));

    cv::Mat result = DownloadTextureToMat(output, out_lvl);

    cv::Mat channels[3];
    cv::split(result, channels);

    cv::Scalar std_dx, std_dy;
    cv::Mat dx_mask = (channels[0] != 0.0f);
    cv::Mat dy_mask = (channels[1] != 0.0f);

    cv::meanStdDev(channels[0], cv::Scalar(), std_dx, dx_mask);
    cv::meanStdDev(channels[1], cv::Scalar(), std_dy, dy_mask);
    EXPECT_GT(std_dx[0], 0.01) << "X gradient should have variation";
    EXPECT_GT(std_dy[0], 0.01) << "Y gradient should have variation";
}

// Jtra renderer depends on DIDxy
TYPED_TEST_P(RendererTypedTests, JPoseRendererBasicFunctionality)
{
    using Traits = TypeParam;
    const int in_lvl = 0, out_lvl = 0;

    typename Traits::MeshT mesh_img(this->screen_vertex_, this->screen_indices_, true, true, false);
    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);

    typename Traits::template TextureT<ImageType> kf_tex(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> dfdxy_tex(this->w_, this->h_, Vec3<float>(0.0f, 0.0f, 0.0f));
    typename Traits::template TextureT<ImageType> image_tex(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> jtra_tex(this->w_, this->h_, Vec3<float>(0.0f, 0.0f, 0.0f));
    typename Traits::template TextureT<Vec3<float>> jrot_tex(this->w_, this->h_, Vec3<float>(0.0f, 0.0f, 0.0f));
    typename Traits::template TextureT<Vec3<float>> jexp_tex(this->w_, this->h_, Vec3<float>(0.0f, 0.0f, 0.0f));

    UploadMatToTexture(kf_tex, 0, this->image_src_cv_);

    typename Traits::DIDxyRendererT didxy_renderer;
    typename Traits::JPoseExpRendererT jpose_renderer;

    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();
    Vec2<float> exposure(0.0, 0.0);

    ASSERT_NO_THROW(didxy_renderer.Render(mesh_img, in_lvl, out_lvl, kf_tex, dfdxy_tex));
    ASSERT_NO_THROW(jpose_renderer.Render(mesh,
                                          pose_transform,
                                          exposure,
                                          this->cam_,
                                          in_lvl, out_lvl,
                                          kf_tex, dfdxy_tex,
                                          image_tex, jtra_tex, jrot_tex, jexp_tex));

    cv::Mat result = DownloadTextureToMat(jtra_tex, out_lvl);

    cv::Mat channels[3];
    cv::split(result, channels);

    cv::Mat mask_0 = (channels[0] != 0.0f);
    cv::Mat mask_1 = (channels[1] != 0.0f);
    cv::Mat mask_2 = (channels[2] != 0.0f);

    cv::Scalar mean_0, mean_1, mean_2, std_0, std_1, std_2;
    cv::meanStdDev(channels[0], mean_0, std_0, mask_0);
    cv::meanStdDev(channels[1], mean_1, std_1, mask_1);
    cv::meanStdDev(channels[2], mean_2, std_2, mask_2);

    EXPECT_LT(std::abs(mean_0[0]), 100000.0) << "Jacobian component " << 0 << " too large";
    EXPECT_LT(std::abs(mean_1[0]), 100000.0) << "Jacobian component " << 1 << " too large";
    EXPECT_LT(std::abs(mean_2[0]), 100000.0) << "Jacobian component " << 2 << " too large";
}

// Jtra renderer depends on DIDxy
TYPED_TEST_P(RendererTypedTests, JMapRendererBasicFunctionality)
{
    using Traits = TypeParam;
    const int in_lvl = 0, out_lvl = 0;

    typename Traits::MeshT mesh_img(this->screen_vertex_, this->screen_indices_, true, true, false);
    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);

    typename Traits::template TextureT<ImageType> kf_tex(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> dfdxy_tex(this->w_, this->h_, Vec3<float>(0.0f, 0.0f, 0.0f));
    typename Traits::template TextureT<ImageType> image_tex(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> jmap_tex(this->w_, this->h_, Vec3<float>(0.0f, 0.0f, 0.0f));
    typename Traits::template TextureT<Vec3<float>> jexp_tex(this->w_, this->h_, Vec3<float>(0.0f, 0.0f, 0.0f));
    typename Traits::template TextureT<Vec3<PidType>> pids_tex(this->w_, this->h_, Vec3<float>(-1, -1, -1));

    UploadMatToTexture(kf_tex, 0, this->image_src_cv_);

    typename Traits::DIDxyRendererT didxy_renderer;
    typename Traits::JMapExpRendererT jmap_renderer;

    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();
    Vec2<float> exposure(0.0, 0.0);

    ASSERT_NO_THROW(didxy_renderer.Render(mesh_img, in_lvl, out_lvl, kf_tex, dfdxy_tex));
    ASSERT_NO_THROW(jmap_renderer.Render(mesh,
                                         pose_transform,
                                         exposure,
                                         this->cam_,
                                         in_lvl, out_lvl,
                                         kf_tex, dfdxy_tex,
                                         image_tex, jmap_tex, jexp_tex, pids_tex));

    cv::Mat result = DownloadTextureToMat(jmap_tex, out_lvl);

    cv::Mat channels[3];
    cv::split(result, channels);

    cv::Mat mask_0 = (channels[0] != 0.0f);
    cv::Mat mask_1 = (channels[1] != 0.0f);
    cv::Mat mask_2 = (channels[2] != 0.0f);

    cv::Scalar mean_0, mean_1, mean_2, std_0, std_1, std_2;
    cv::meanStdDev(channels[0], mean_0, std_0, mask_0);
    cv::meanStdDev(channels[1], mean_1, std_1, mask_1);
    cv::meanStdDev(channels[2], mean_2, std_2, mask_2);

    EXPECT_LT(std::abs(mean_0[0]), 100000.0) << "Jacobian component " << 0 << " too large";
    EXPECT_LT(std::abs(mean_1[0]), 100000.0) << "Jacobian component " << 1 << " too large";
    EXPECT_LT(std::abs(mean_2[0]), 100000.0) << "Jacobian component " << 2 << " too large";
}

// Error handling and edge cases
TYPED_TEST_P(RendererTypedTests, ErrorHandlingAndEdgeCases)
{
    using Traits = TypeParam;
    const int in_lvl = 0, out_lvl = 0;

    std::vector<float> emtpy_vertex;
    std::vector<int> empty_indices;

    typename Traits::MeshT empty_mesh(emtpy_vertex, empty_indices, true, true, true);
    typename Traits::template TextureT<float> output(this->w_, this->h_, -1.0f);

    typename Traits::DepthRendererT renderer;

    ASSERT_NO_THROW(renderer.Render(empty_mesh, SE3<float>(), this->cam_, out_lvl, output));

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);
    ASSERT_NO_THROW(renderer.Render(mesh, SE3<float>(), this->cam_, out_lvl, output));

    if (this->w_ >= 4 && this->h_ >= 4)
    {
        ASSERT_NO_THROW(renderer.Render(mesh, SE3<float>(), this->cam_, 1, output));
        ASSERT_NO_THROW(renderer.Render(mesh, SE3<float>(), this->cam_, 0, output));
    }
}

// Resource/memory management
TYPED_TEST_P(RendererTypedTests, ResourceManagement)
{
    using Traits = TypeParam;
    const int iterations = 10;
    for (int i = 0; i < iterations; ++i)
    {
        typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);
        typename Traits::template TextureT<float> output(this->w_, this->h_, -1.0f);
        typename Traits::DepthRendererT renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3<float>(), this->cam_, 0, output));
    }
    SUCCEED();
}

// Varying texture sizes (generalized framebuffer/resize validation)
TYPED_TEST_P(RendererTypedTests, VaryingTextureSizes)
{
    using Traits = TypeParam;
    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);
    const std::vector<std::pair<int, int>> sizes = {{64, 64}, {128, 128}, {256, 256}};
    for (const auto &size : sizes)
    {
        int w = size.first, h = size.second;

        typename Traits::template TextureT<float> output(w, h, -1.0f);

        typename Traits::DepthRendererT renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3<float>(), this->cam_, 0, output));
        cv::Mat result = DownloadTextureToMat(output, 0);
        EXPECT_EQ(result.cols, w);
        EXPECT_EQ(result.rows, h);
    }
}

REGISTER_TYPED_TEST_SUITE_P(
    RendererTypedTests,
    DepthRendererBasicFunctionality,
    ImageRendererBasicFunctionality,
    DIDxyRendererBasicFunctionality,
    JPoseRendererBasicFunctionality,
    JMapRendererBasicFunctionality,
    ErrorHandlingAndEdgeCases,
    ResourceManagement,
    VaryingTextureSizes);

using TestBackends = ::testing::Types<CPUBackendTraits
#ifdef COMPILE_GL
                                      ,
                                      GLBackendTraits
#endif
#ifdef COMPILE_HLS
                                      ,
                                      HLSBackendTraits
#endif
                                      >;

INSTANTIATE_TYPED_TEST_SUITE_P(AllBackends, RendererTypedTests, TestBackends);
