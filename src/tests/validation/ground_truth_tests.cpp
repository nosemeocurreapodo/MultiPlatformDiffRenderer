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

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);

    typename Traits::template TextureT<float> output(this->w_, this->h_, -1.0f);
    typename Traits::template TextureT<float> ground_truth(this->w_, this->h_, -1.0f);

    UploadMatToTexture(ground_truth, 0, this->depth_dst_cv_);

    typename Traits::DepthRendererT renderer;
    linalg::SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    for (int lvl = 0; lvl < output.levels(); lvl++)
    {
        if (lvl > 0)
            continue;

        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, pose_transform, this->cam_, lvl, output);
        // Performance validation
        double duration = timer.Stop();
        // EXPECT_LT(duration, 1000.0) << "Rendering should complete within 1 second";

        double rmse = RMSE(output, ground_truth, lvl);

        std::cout << "DepthGroundTruth RMSE " << lvl << " " << rmse << std::endl;
        EXPECT_LT(rmse, this->thresholds_.gt_max_depth_error) << "RMSE error: " << rmse;
    }

    cv::Mat result = DownloadTextureToMat(output, 0);

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

    SaveDebugImage(diff, std::string(typeid(typename Traits::ImageRendererT).name()) + "_depth_ground_truth.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";
}

// Test depth renderer against ground truth
TYPED_TEST_P(GroundTruthTests, DepthReferenceValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);

    typename Traits::template TextureT<float> input_depth(this->w_, this->h_, -1.0f);
    typename Traits::template TextureT<float> output(this->w_, this->h_, -1.0f);
    typename Traits::template TextureT<float> reference(this->w_, this->h_, -1.0f);

    UploadMatToTexture(input_depth, 0, this->depth_src_cv_);

    typename Traits::DepthRendererT renderer;
    linalg::SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    for (int lvl = 0; lvl < output.levels(); lvl++)
    {
        if (lvl > 0)
            continue;

        DepthRendererRef(input_depth,
                         pose_transform,
                         this->cam_,
                         lvl,
                         reference);

        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, pose_transform, this->cam_, lvl, output);
        // Performance validation
        double duration = timer.Stop();
        // EXPECT_LT(duration, 1000.0) << "Rendering should complete within 1 second";

        double rmse = RMSE(output, reference, lvl);

        std::cout << "DepthReference RMSE " << lvl << " " << rmse << std::endl;
        EXPECT_LT(rmse, this->thresholds_.ref_max_depth_error) << "RMSE error: " << rmse;
    }

    cv::Mat result = DownloadTextureToMat(output, 0);
    cv::Mat ref = DownloadTextureToMat(reference, 0);

    // cv::Mat mask = (result != 0.0f);
    cv::Mat diff = result - ref;
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

    SaveDebugImage(diff, std::string(typeid(typename Traits::ImageRendererT).name()) + "_depth_reference.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";
}

// Test image renderer against ground truth
TYPED_TEST_P(GroundTruthTests, ImageGroundTruthValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);

    typename Traits::template TextureT<ImageType> input(this->w_, this->h_, 0);
    typename Traits::template TextureT<ImageType> output(this->w_, this->h_, 0);
    typename Traits::template TextureT<ImageType> ground_truth(this->w_, this->h_, 0);

    UploadMatToTexture(ground_truth, 0, this->image_dst_cv_);
    UploadMatToTexture(input, 0, this->image_src_cv_);

    typename Traits::ImageRendererT renderer;
    linalg::SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    for (int lvl = 0; lvl < output.levels(); lvl++)
    {
        if (lvl > 0)
            continue;

        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, pose_transform, this->cam_, lvl, lvl, input, output);
        // Performance validation
        double duration = timer.Stop();
        // EXPECT_LT(duration, 1000.0) << "Rendering should complete within 1 second";

        double rmse = RMSE(output, ground_truth, lvl);

        std::cout << "ImageGroundTruth RMSE " << lvl << " " << rmse << std::endl;
        EXPECT_LT(rmse, this->thresholds_.gt_max_image_error) << "RMSE error: " << rmse;
    }

    cv::Mat result = DownloadTextureToMat(output, 0);
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

    SaveDebugImage(diff, std::string(typeid(typename Traits::ImageRendererT).name()) + "_image_ground_truth.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";}
}

// Test image renderer against ground truth
TYPED_TEST_P(GroundTruthTests, ImageReferenceValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);

    typename Traits::template TextureT<float> input_depth(this->w_, this->h_, -1.0f);
    typename Traits::template TextureT<ImageType> input_image(this->w_, this->h_, 0);

    typename Traits::template TextureT<ImageType> output(this->w_, this->h_, 0);
    typename Traits::template TextureT<ImageType> reference(this->w_, this->h_, 0);

    UploadMatToTexture(input_depth, 0, this->depth_src_cv_);
    UploadMatToTexture(input_image, 0, this->image_src_cv_);

    typename Traits::ImageRendererT renderer;
    linalg::SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    for (int lvl = 0; lvl < output.levels(); lvl++)
    {
        if (lvl > 0)
            continue;

        ImageRendererRef(input_depth,
                         input_image,
                         pose_transform,
                         this->cam_,
                         lvl,
                         reference);

        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, pose_transform, this->cam_, lvl, lvl, input_image, output);
        // Performance validation
        double duration = timer.Stop();
        // EXPECT_LT(duration, 1000.0) << "Rendering should complete within 1 second";

        double rmse = RMSE(output, reference, lvl);

        std::cout << "ImageReference RMSE " << lvl << " " << rmse << std::endl;
        EXPECT_LT(rmse, this->thresholds_.ref_max_image_error) << "RMSE error: " << rmse;
    }

    cv::Mat result = DownloadTextureToMat(output, 0);
    cv::Mat ref = DownloadTextureToMat(reference, 0);
    // cv::Mat mask = (result != 0.0f);
    cv::Mat diff = result - ref;
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

    SaveDebugImage(diff, std::string(typeid(typename Traits::ImageRendererT).name()) + "_image_reference.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";}
}

REGISTER_TYPED_TEST_SUITE_P(
    GroundTruthTests,
    DepthGroundTruthValidation,
    DepthReferenceValidation,
    ImageGroundTruthValidation,
    ImageReferenceValidation);

using TestBackends = ::testing::Types<CPUBackendTraits
                                      // #ifdef COMPILE_GL
                                      //                                       ,
                                      //                                       GLBackendTraits
                                      // #endif
                                      >;

INSTANTIATE_TYPED_TEST_SUITE_P(AllBackends, GroundTruthTests, TestBackends);
