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
    using JPoseFDRendererT = JPoseFDRendererCPU;
    using JPoseExpRendererT = JPoseExpRendererCPU;
    using JMapFDRendererT = JMapFDRendererCPU;
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
    using ResidualRendererT = ResidualRendererGL;
    using DIDxyRendererT = DIDxyRendererGL;
    using JPoseExpRendererT = JPoseExpRendererGL;
    using JMapExpRendererT = JMapExpRendererGL;
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

    typename Traits::template TextureT<float> output(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<float> ground_truth(this->w_, this->h_, 0.0f);

    UploadMatToTexture(ground_truth, 0, this->depth_dst_cv_);

    typename Traits::DepthRendererT renderer;
    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    for (int lvl = 4; lvl >= 0; lvl--)
    {
        // if (lvl > 0)
        //     continue;

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

    SaveDebugImage(diff, std::string(typeid(typename Traits::DepthRendererT).name()) + "_depth_ground_truth.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";
}

// Test depth renderer against ground truth
TYPED_TEST_P(GroundTruthTests, DepthReferenceValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);

    typename Traits::template TextureT<float> input_depth(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<float> output(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<float> reference(this->w_, this->h_, 0.0f);

    UploadMatToTexture(input_depth, 0, this->depth_src_cv_);

    typename Traits::DepthRendererT renderer;
    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    for (int lvl = 4; lvl >= 0; lvl--)
    {
        // if (lvl > 0)
        //     continue;

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

    SaveDebugImage(diff, std::string(typeid(typename Traits::DepthRendererT).name()) + "_depth_reference.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";
}

// Test depth renderer against ground truth
TYPED_TEST_P(GroundTruthTests, DepthReferenceGTValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);

    typename Traits::template TextureT<float> input_depth(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<float> output(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<float> gt_depth(this->w_, this->h_, 0.0f);

    UploadMatToTexture(input_depth, 0, this->depth_src_cv_);
    UploadMatToTexture(gt_depth, 0, this->depth_dst_cv_);

    typename Traits::DepthRendererT renderer;
    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();

    for (int lvl = 4; lvl >= 0; lvl--)
    {
        // if (lvl > 0)
        //     continue;

        DepthRendererRef(input_depth,
                         pose_transform,
                         this->cam_,
                         lvl,
                         output);

        double rmse = RMSE(output, gt_depth, lvl);

        std::cout << "DepthReferenceGT RMSE " << lvl << " " << rmse << std::endl;
        EXPECT_LT(rmse, this->thresholds_.ref_gt_max_depth_error) << "RMSE error: " << rmse;
    }

    cv::Mat result = DownloadTextureToMat(output, 0);
    cv::Mat ref = DownloadTextureToMat(gt_depth, 0);

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

    SaveDebugImage(diff, std::string(typeid(typename Traits::DepthRendererT).name()) + "_depth_reference_gt.png");

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
    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();
    Vec2<float> exposure(0.0, 0.0);

    for (int lvl = 4; lvl >= 0; lvl--)
    {
        // if (lvl > 0)
        //     continue;

        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, pose_transform, exposure, this->cam_, lvl, lvl, input, output);
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

    typename Traits::template TextureT<float> input_depth(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<ImageType> input_image(this->w_, this->h_, 0);

    typename Traits::template TextureT<ImageType> output(this->w_, this->h_, 0);
    typename Traits::template TextureT<ImageType> reference(this->w_, this->h_, 0);

    UploadMatToTexture(input_depth, 0, this->depth_src_cv_);
    UploadMatToTexture(input_image, 0, this->image_src_cv_);

    typename Traits::ImageRendererT renderer;
    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();
    Vec2<float> exposure(0.0, 0.0);

    for (int lvl = 4; lvl >= 0; lvl--)
    {
        // if (lvl > 0)
        //     continue;

        ImageRendererRef(input_depth,
                         input_image,
                         pose_transform,
                         this->cam_,
                         lvl,
                         reference);

        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, pose_transform, exposure, this->cam_, lvl, lvl, input_image, output);
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


// Test image renderer against ground truth
TYPED_TEST_P(GroundTruthTests, ImageReferenceGTValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);

    typename Traits::template TextureT<float> input_depth(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<ImageType> input_image(this->w_, this->h_, 0);

    typename Traits::template TextureT<ImageType> output_image(this->w_, this->h_, 0);
    typename Traits::template TextureT<ImageType> reference(this->w_, this->h_, 0);

    UploadMatToTexture(input_depth, 0, this->depth_src_cv_);
    UploadMatToTexture(input_image, 0, this->image_src_cv_);
    UploadMatToTexture(output_image, 0, this->image_dst_cv_);

    typename Traits::ImageRendererT renderer;
    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();
    Vec2<float> exposure(0.0, 0.0);

    for (int lvl = 4; lvl >= 0; lvl--)
    {
        // if (lvl > 0)
        //     continue;

        ImageRendererRef(input_depth,
                         input_image,
                         pose_transform,
                         this->cam_,
                         lvl,
                         reference);

        double rmse = RMSE(output_image, reference, lvl);

        std::cout << "ImageReference GT RMSE " << lvl << " " << rmse << std::endl;
        EXPECT_LT(rmse, this->thresholds_.ref_gt_max_image_error) << "RMSE error: " << rmse;
    }

    cv::Mat result = DownloadTextureToMat(output_image, 0);
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

    SaveDebugImage(diff, std::string(typeid(typename Traits::ImageRendererT).name()) + "_image_reference_gt.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";}
}

// Test residual renderer against itself
// here the idea is that if the pose is the identity
// the projected image should be equal to itself
TYPED_TEST_P(GroundTruthTests, ResidualValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);

    typename Traits::template TextureT<float> input_depth(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<ImageType> input_image(this->w_, this->h_, 0);

    typename Traits::template TextureT<ImageType> output(this->w_, this->h_, 0);
    typename Traits::template TextureT<ImageType> reference(this->w_, this->h_, -1);

    UploadMatToTexture(input_depth, 0, this->depth_src_cv_);
    UploadMatToTexture(input_image, 0, this->image_src_cv_);

    typename Traits::ResidualRendererT renderer;
    SE3<float> pose_transform = SE3<float>();
    Vec2<float> exposure(0.0, 0.0);

    for (int lvl = 2; lvl >= 0; lvl--)
    {
        // if (lvl > 0)
        //     continue;

        reference.fill(lvl, 0);

        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, pose_transform, exposure, this->cam_, lvl, lvl, input_image, input_image, output);
        // Performance validation
        double duration = timer.Stop();
        // EXPECT_LT(duration, 1000.0) << "Rendering should complete within 1 second";

        double rmse = RMSE(output, reference, lvl);

        std::cout << "Residual RMSE " << lvl << " " << rmse << std::endl;
        EXPECT_LT(rmse, this->thresholds_.ref_max_residual_error) << "RMSE error: " << rmse;
    }

    cv::Mat result = DownloadTextureToMat(output, 0);

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

    SaveDebugImage(result, std::string(typeid(typename Traits::ResidualRendererT).name()) + "_residual.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";}
}

// Test image renderer against ground truth
TYPED_TEST_P(GroundTruthTests, JPoseReferenceValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);
    typename Traits::MeshT mesh_sceen(this->screen_vertex_, this->screen_indices_, true, true, false);

    typename Traits::template TextureT<float> kf_depth(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<ImageType> kf_image(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> kf_didxy(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<ImageType> f_image(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> f_didxy(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));

    typename Traits::template TextureT<Vec3<float>> reference_jtra(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<Vec3<float>> reference_jrot(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<float> reference_r(this->w_, this->h_, 0);

    typename Traits::template TextureT<Vec3<float>> output_jtra(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<Vec3<float>> output_jrot(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<Vec3<float>> output_jexp(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<float> output_r(this->w_, this->h_, 0);

    UploadMatToTexture(kf_depth, 0, this->depth_src_cv_);
    UploadMatToTexture(kf_image, 0, this->image_src_cv_);

    UploadMatToTexture(f_image, 0, this->image_dst_cv_);

    typename Traits::DIDxyRendererT didxy_renderer;
    typename Traits::JPoseExpRendererT jpose_renderer;
    typename Traits::JPoseFDRendererT jposefd_renderer;

    // didxy_renderer.Render(mesh_sceen, 0, 0, f_image, f_didxy);
    // f_didxy.generate_mipmaps(0);

    // didxy_renderer.Render(mesh_sceen, 0, 0, kf_image, kf_didxy);
    // kf_didxy.generate_mipmaps(0);

    for (int lvl = 0; lvl < output_jtra.levels(); lvl++)
    {
        didxy_renderer.Render(mesh_sceen, lvl, lvl, f_image, f_didxy);
        didxy_renderer.Render(mesh_sceen, lvl, lvl, kf_image, kf_didxy);
    }

    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();
    Vec2<float> exposure(0.0, 0.0);

    int lvl = 1;
    // for (int lvl = 4; lvl >= 0; lvl--)
    {
        // if (lvl > 0)
        //     continue;

        PerformanceTimer timer;
        timer.Start();

        jpose_renderer.Render(mesh,
                              pose_transform,
                              exposure,
                              this->cam_,
                              lvl, lvl,
                              kf_image, f_image, kf_didxy,
                              output_jtra, output_jrot, output_jexp, output_r);
        jposefd_renderer.Render(mesh, pose_transform, this->cam_, lvl, lvl, kf_image, f_image, kf_didxy, reference_jtra, reference_jrot, reference_r);

        // Performance validation
        double duration = timer.Stop();
        // EXPECT_LT(duration, 1000.0) << "Rendering should complete within 1 second";

        double rmse_jtra = RMSEV(output_jtra, reference_jtra, lvl);
        double rmse_jrot = RMSEV(output_jrot, reference_jrot, lvl);
        double rmse_r = RMSE(output_r, reference_r, lvl);

        std::cout << "JPoseReference RMSE " << lvl << " " << rmse_jtra << " " << rmse_jrot << std::endl;
        EXPECT_LT(rmse_jtra, this->thresholds_.ref_max_jtra_error) << "RMSE error: " << rmse_jtra;
        EXPECT_LT(rmse_jrot, this->thresholds_.ref_max_jrot_error) << "RMSE error: " << rmse_jrot;
        EXPECT_LT(rmse_r, this->thresholds_.ref_max_r_error) << "RMSE error: " << rmse_r;
    }

    cv::Mat result_jtra = DownloadTextureToMat(output_jtra, lvl);
    cv::Mat ref_jtra = DownloadTextureToMat(reference_jtra, lvl);

    cv::Mat result_jrot = DownloadTextureToMat(output_jrot, lvl);
    cv::Mat ref_jrot = DownloadTextureToMat(reference_jrot, lvl);

    cv::Mat result_r = DownloadTextureToMat(output_r, lvl);
    cv::Mat ref_r = DownloadTextureToMat(reference_r, lvl);

    // cv::Mat mask = (result != 0.0f);
    cv::Mat diff_jtra = result_jtra - ref_jtra;
    cv::Mat diff_jrot = result_jrot - ref_jrot;
    cv::Mat diff_r = result_r - ref_r;
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

    SaveDebugImageColor(diff_jtra, std::string(typeid(typename Traits::JPoseExpRendererT).name()) + "_jtra_diff.png");
    SaveDebugImageColor(diff_jrot, std::string(typeid(typename Traits::JPoseExpRendererT).name()) + "_jrot_diff.png");
    SaveDebugImage(diff_r, std::string(typeid(typename Traits::JPoseExpRendererT).name()) + "_r_diff.png");

    SaveDebugImageColor(result_jtra, std::string(typeid(typename Traits::JPoseExpRendererT).name()) + "_jtra_result.png");
    SaveDebugImageColor(ref_jtra, std::string(typeid(typename Traits::JPoseExpRendererT).name()) + "_jtra_reference.png");

    SaveDebugImageColor(result_jrot, std::string(typeid(typename Traits::JPoseExpRendererT).name()) + "_jrot_result.png");
    SaveDebugImageColor(ref_jrot, std::string(typeid(typename Traits::JPoseExpRendererT).name()) + "_jrot_reference.png");

    SaveDebugImage(result_r, std::string(typeid(typename Traits::JPoseExpRendererT).name()) + "_r_result.png");
    SaveDebugImage(ref_r, std::string(typeid(typename Traits::JPoseExpRendererT).name()) + "_r_reference.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";}
}

// Test image renderer against ground truth
TYPED_TEST_P(GroundTruthTests, JMapReferenceValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);
    typename Traits::MeshT mesh_sceen(this->screen_vertex_, this->screen_indices_, true, true, false);

    typename Traits::template TextureT<float> kf_depth(this->w_, this->h_, 0.0f);
    typename Traits::template TextureT<ImageType> kf_image(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> kf_didxy(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<ImageType> f_image(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> f_didxy(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));

    typename Traits::template TextureT<Vec3<float>> reference_jmap(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<Vec3<PidType>> reference_pids(this->w_, this->h_, Vec3<PidType>(-1, -1, -1));
    typename Traits::template TextureT<float> reference_r(this->w_, this->h_, 0);

    typename Traits::template TextureT<Vec3<float>> output_jmap(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<Vec3<float>> output_jexp(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<Vec3<PidType>> output_pids(this->w_, this->h_, Vec3<PidType>(-1, -1, -1));
    typename Traits::template TextureT<float> output_r(this->w_, this->h_, 0);

    UploadMatToTexture(kf_depth, 0, this->depth_src_cv_);
    UploadMatToTexture(kf_image, 0, this->image_src_cv_);

    UploadMatToTexture(f_image, 0, this->image_dst_cv_);

    typename Traits::ImageRendererT image_renderer;
    typename Traits::DIDxyRendererT didxy_renderer;
    typename Traits::JMapExpRendererT jmap_renderer;
    typename Traits::JMapFDRendererT jmapfd_renderer;

    // didxy_renderer.Render(mesh_sceen, 0, 0, f_image, f_didxy);
    // f_didxy.generate_mipmaps(0);

    // didxy_renderer.Render(mesh_sceen, 0, 0, kf_image, kf_didxy);
    // kf_didxy.generate_mipmaps(0);

    for (int lvl = 0; lvl < output_jmap.levels(); lvl++)
    {
        didxy_renderer.Render(mesh_sceen, lvl, lvl, f_image, f_didxy);
        didxy_renderer.Render(mesh_sceen, lvl, lvl, kf_image, kf_didxy);
    }

    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();
    Vec2<float> exposure(0.0, 0.0);

    int lvl = 1;
    // for (int lvl = 4; lvl >= 0; lvl--)
    {
        // if (lvl > 0)
        //     continue;

        reference_jmap.fill(lvl, reference_jmap.nodata());

        PerformanceTimer timer;
        timer.Start();

        // mesh.set_positions(positions);
        jmap_renderer.Render(mesh,
                             pose_transform,
                             exposure,
                             this->cam_,
                             lvl, lvl,
                             kf_image, f_image, kf_didxy,
                             output_jmap, output_jexp, output_pids, output_r);
        jmapfd_renderer.Render(mesh,
                               pose_transform,
                               this->cam_,
                               lvl, lvl,
                               kf_image, f_image, kf_didxy,
                               reference_jmap, reference_pids, reference_r);

        // Performance validation
        double duration = timer.Stop();
        // EXPECT_LT(duration, 1000.0) << "Rendering should complete within 1 second";

        double rmse_jmap = RMSEV(output_jmap, reference_jmap, lvl);

        std::cout << "JMapReference RMSE " << lvl << " " << rmse_jmap << std::endl;
        EXPECT_LT(rmse_jmap, this->thresholds_.ref_max_jmap_error) << "RMSE error: " << rmse_jmap;
    }

    cv::Mat result_jmap = DownloadTextureToMat(output_jmap, lvl);
    cv::Mat ref_jmap = DownloadTextureToMat(reference_jmap, lvl);

    // cv::Mat mask = (result != 0.0f);
    cv::Mat diff_jmap = result_jmap - ref_jmap;
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

    SaveDebugImageColor(diff_jmap, std::string(typeid(typename Traits::JMapExpRendererT).name()) + "_jmap_diff.png");

    SaveDebugImageColor(result_jmap, std::string(typeid(typename Traits::JMapExpRendererT).name()) + "_jmap_result.png");
    SaveDebugImageColor(ref_jmap, std::string(typeid(typename Traits::JMapExpRendererT).name()) + "_jmap_reference.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";}
}

REGISTER_TYPED_TEST_SUITE_P(
    GroundTruthTests,
    DepthGroundTruthValidation,
    DepthReferenceValidation,
    DepthReferenceGTValidation,
    ImageGroundTruthValidation,
    ImageReferenceValidation,
    ImageReferenceGTValidation,
    ResidualValidation,
    JPoseReferenceValidation,
    JMapReferenceValidation);

using TestBackends = ::testing::Types<CPUBackendTraits
                                      // #ifdef COMPILE_GL
                                      //                                       ,
                                      //                                       GLBackendTraits
                                      // #endif
                                      >;

INSTANTIATE_TYPED_TEST_SUITE_P(AllBackends, GroundTruthTests, TestBackends);
