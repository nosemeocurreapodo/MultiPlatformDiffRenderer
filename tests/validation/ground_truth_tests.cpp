#include <gtest/gtest.h>
#include "../common/test_framework.h"

#ifdef COMPILE_GL

// GL context setup
class GLContextEnv : public ::testing::Environment
{
public:
    void SetUp() override { InitEGL(); }
    void TearDown() override {}
};
static ::testing::Environment *const gl_env =
    ::testing::AddGlobalTestEnvironment(new GLContextEnv());

#endif
// Backend trait structs to unify CPU and GL testing
struct CPUBackendTraits
{
    using MeshT = MeshCPU;
    template <typename T>
    using TextureT = TextureCPU<T>;
    using DepthRendererT = DepthRendererCPU;
    using ImageRendererT = ImageRendererCPU;
    using ResidualRendererT = ResidualRendererCPU;
    using L2RendererT = L2RendererCPU;
    using DIDxyRendererT = DIDxyRendererCPU;
    using JPoseRendererT = JPoseRendererCPU;
    using JMapRendererT = JMapRendererCPU;
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
    using ResidualRendererT = ResidualRendererGL;
    using L2RendererT = L2RendererGL;
    using DIDxyRendererT = DIDxyRendererGL;
    using JPoseRendererT = JPoseRendererGL;
    using JMapRendererT = JMapRendererGL;
    static const char *Name() { return "GL"; }
};
#endif

template <typename Backend>
class GroundTruthTests : public TwoViewTests
{
protected:
    ValidationThresholds thresholds_;
    void SetUp() override
    {
        TwoViewTests::SetUp();
    }
};

TYPED_TEST_SUITE_P(GroundTruthTests);

// Test depth renderer against ground truth
TYPED_TEST_P(GroundTruthTests, DepthGroundTruthValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertices_, this->texcoords_, this->weights_, this->indices_);

    typename Traits::template TextureT<float> output(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<float> ground_truth(this->w_, this->h_, 0.0f);

    this->UploadMatToTexture(ground_truth, 0, this->depth_dst_cv_);

    typename Traits::DepthRendererT renderer;
    SE3 pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    for (int lvl = 0; lvl < output.levels(); lvl++)
    {
        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, pose_transform, this->cam_, lvl, output);
        // Performance validation
        double duration = timer.Stop();
        // EXPECT_LT(duration, 1000.0) << "Rendering should complete within 1 second";

        double rmse = this->RMSE(output, ground_truth, lvl);

        if (lvl < 3)
            EXPECT_LT(rmse, this->thresholds_.gt_max_depth_error) << "RMSE error: " << rmse;
    }

    cv::Mat result = this->DownloadTexture(output, 0, CV_32FC1);

    // cv::Mat mask = (result != 0.0f);
    cv::Mat diff = result - this->depth_dst_cv_;
    // cv::Mat masked_diff;
    // diff.copyTo(masked_diff, mask);

    // cv::Mat diff;
    // cv::absdiff(result, this->depth_dst_cv_, diff);

    // Basic validation against expected properties
    // cv::Mat mask = (result != -1.0f);
    // cv::Scalar mean_val, std_val;
    // cv::meanStdDev(result, mean_val, std_val, mask);

    // EXPECT_GT(mean_val[0], 0.0) << "Mean depth should be positive";
    // EXPECT_LT(mean_val[0], 100.0) << "Mean depth should be reasonable";
    // EXPECT_GT(std_val[0], 0.0) << "Depth should have variation";

    this->SaveDebugImage(diff, std::string(typeid(typename Traits::ImageRendererT).name()) + "_depth_ground_truth.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";
}

// Test image renderer against ground truth
TYPED_TEST_P(GroundTruthTests, ImageGroundTruthValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertices_, this->texcoords_, this->weights_, this->indices_);

    typename Traits::template TextureT<float> input(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<float> output(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<float> ground_truth(this->w_, this->h_, 0.0f);

    this->UploadMatToTexture(ground_truth, 0, this->image_dst_cv_);
    this->UploadMatToTexture(input, 0, this->image_src_cv_);

    typename Traits::ImageRendererT renderer;
    SE3 pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    for (int lvl = 0; lvl < output.levels(); lvl++)
    {
        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, pose_transform, this->cam_, lvl, lvl, input, output);
        // Performance validation
        double duration = timer.Stop();
        // EXPECT_LT(duration, 1000.0) << "Rendering should complete within 1 second";

        double rmse = this->RMSE(output, ground_truth, lvl);

        if (lvl < 3)
            EXPECT_LT(rmse, this->thresholds_.gt_max_image_error) << "RMSE error: " << rmse;
    }

    cv::Mat result = this->DownloadTexture(output, 0, CV_32FC1);
    // cv::Mat mask = (result != 0.0f);
    cv::Mat diff = result - this->image_dst_cv_;
    // cv::Mat masked_diff;
    // diff.copyTo(masked_diff, mask);

    // cv::Mat diff;
    // cv::absdiff(result, this->image_dst_cv_, diff);

    // Basic validation against expected properties
    // cv::Mat mask = (result != -1.0f);
    // cv::Scalar mean_val, std_val;
    // cv::meanStdDev(result, mean_val, std_val, mask);

    // EXPECT_GT(mean_val[0], 0.0) << "Mean depth should be positive";
    // EXPECT_LT(mean_val[0], 100.0) << "Mean depth should be reasonable";
    // EXPECT_GT(std_val[0], 0.0) << "Depth should have variation";

    this->SaveDebugImage(diff, std::string(typeid(typename Traits::ImageRendererT).name()) + "_image_ground_truth.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";}
}

REGISTER_TYPED_TEST_SUITE_P(
    GroundTruthTests,
    DepthGroundTruthValidation,
    ImageGroundTruthValidation);

using TestBackends = ::testing::Types<CPUBackendTraits
#ifdef COMPILE_GL
                                      ,
                                      GLBackendTraits
#endif
                                      >;

INSTANTIATE_TYPED_TEST_SUITE_P(AllBackends, GroundTruthTests, TestBackends);
