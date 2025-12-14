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
    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();

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

    SaveDebugImage(diff, std::string(typeid(typename Traits::DepthRendererT).name()) + "_depth_ground_truth.png");

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
    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();

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

    SaveDebugImage(diff, std::string(typeid(typename Traits::DepthRendererT).name()) + "_depth_reference.png");

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

    for (int lvl = 0; lvl < output.levels(); lvl++)
    {
        if (lvl > 0)
            continue;

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

    typename Traits::template TextureT<float> input_depth(this->w_, this->h_, -1.0f);
    typename Traits::template TextureT<ImageType> input_image(this->w_, this->h_, 0);

    typename Traits::template TextureT<ImageType> output(this->w_, this->h_, 0);
    typename Traits::template TextureT<ImageType> reference(this->w_, this->h_, 0);

    UploadMatToTexture(input_depth, 0, this->depth_src_cv_);
    UploadMatToTexture(input_image, 0, this->image_src_cv_);

    typename Traits::ImageRendererT renderer;
    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();
    Vec2<float> exposure(0.0, 0.0);

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
TYPED_TEST_P(GroundTruthTests, JPoseReferenceValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);
    typename Traits::MeshT mesh_sceen(this->screen_vertex_, this->screen_indices_, true, true, false);

    typename Traits::template TextureT<float> kf_depth(this->w_, this->h_, -1.0f);
    typename Traits::template TextureT<ImageType> kf_image(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> kf_didxy(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<ImageType> f_image(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> f_didxy(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));

    typename Traits::template TextureT<ImageType> image_1(this->w_, this->h_, 0);
    typename Traits::template TextureT<ImageType> image_2(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> reference_jtra(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<Vec3<float>> reference_jrot(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));

    typename Traits::template TextureT<Vec3<float>> output_jtra(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<Vec3<float>> output_jrot(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<float> output_r(this->w_, this->h_, 0);

    UploadMatToTexture(kf_depth, 0, this->depth_src_cv_);
    UploadMatToTexture(kf_image, 0, this->image_src_cv_);

    UploadMatToTexture(f_image, 0, this->image_dst_cv_);

    typename Traits::ImageRendererT image_renderer;
    typename Traits::DIDxyRendererT didxy_renderer;
    typename Traits::JPoseRendererT jpose_renderer;

    // didxy_renderer.Render(mesh_sceen, 0, 0, f_image, f_didxy);
    // f_didxy.generate_mipmaps(0);

    // idxy_renderer.Render(mesh_sceen, 0, 0, kf_image, kf_didxy);
    // kf_didxy.generate_mipmaps(0);

    for (int lvl = 0; lvl < output_jtra.levels(); lvl++)
    {
        if (lvl > 5)
            continue;

        didxy_renderer.Render(mesh_sceen, lvl, lvl, f_image, f_didxy);
        didxy_renderer.Render(mesh_sceen, lvl, lvl, kf_image, kf_didxy);
    }

    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();
    Vec2<float> exposure(0.0, 0.0);

    for (int lvl = 0; lvl < output_jtra.levels(); lvl++)
    {
        if (lvl > 0)
            continue;

        reference_jtra.fill(lvl, reference_jtra.nodata());
        reference_jrot.fill(lvl, reference_jrot.nodata());

        PerformanceTimer timer;
        timer.Start();

        jpose_renderer.Render(mesh, pose_transform, this->cam_, lvl, lvl, kf_image, f_image, kf_didxy, output_jtra, output_jrot, output_r);

        for (int i = 0; i < 6; i++)
        {
            float delta;

            if (i < 3)
                delta = 1e-4;
            else
                delta = 1e-3;

            Vec6<float> inc(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
            inc(i) = delta;

            SE3<float> pose_transform_1 = SE3<float>::exp(-inc) * pose_transform;
            SE3<float> pose_transform_2 = SE3<float>::exp(inc) * pose_transform;

            image_renderer.Render(mesh, pose_transform_1, exposure, this->cam_, lvl, lvl, kf_image, image_1);
            image_renderer.Render(mesh, pose_transform_2, exposure, this->cam_, lvl, lvl, kf_image, image_2);

            for (int y = 0; y < f_image.height(lvl); y++)
            {
                for (int x = 0; x < f_image.width(lvl); x++)
                {
                    ImageType data_1 = image_1.texel_(y, x, lvl);
                    ImageType data_2 = image_2.texel_(y, x, lvl);

                    if (data_1 == image_1.nodata() || data_2 == image_2.nodata())
                        continue;

                    RealType der = (RealType(data_1) - RealType(data_2)) / (2 * delta);

                    if (i < 3)
                    {
                        Vec3<float> data = reference_jtra.texel_(y, x, lvl);
                        data(i) = der;
                        reference_jtra.set_texel_(data, y, x, lvl);
                    }
                    else
                    {
                        Vec3<float> data = reference_jrot.texel_(y, x, lvl);
                        data(i - 3) = der;
                        reference_jrot.set_texel_(data, y, x, lvl);
                    }
                }
            }
        }

        // Performance validation
        double duration = timer.Stop();
        // EXPECT_LT(duration, 1000.0) << "Rendering should complete within 1 second";

        double rmse_jtra = RMSEV(output_jtra, reference_jtra, lvl);
        double rmse_jrot = RMSEV(output_jrot, reference_jrot, lvl);

        std::cout << "JPoseReference RMSE " << lvl << " " << rmse_jtra << " " << rmse_jrot << std::endl;
        EXPECT_LT(rmse_jtra, this->thresholds_.ref_max_jtra_error) << "RMSE error: " << rmse_jtra;
        EXPECT_LT(rmse_jrot, this->thresholds_.ref_max_jrot_error) << "RMSE error: " << rmse_jrot;
    }

    cv::Mat result_jtra = DownloadTextureToMat(output_jtra, 0);
    cv::Mat ref_jtra = DownloadTextureToMat(reference_jtra, 0);

    cv::Mat result_jrot = DownloadTextureToMat(output_jrot, 0);
    cv::Mat ref_jrot = DownloadTextureToMat(reference_jrot, 0);

    // cv::Mat mask = (result != 0.0f);
    cv::Mat diff_jtra = result_jtra - ref_jtra;
    cv::Mat diff_jrot = result_jrot - ref_jrot;
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

    SaveDebugImage(diff_jtra, std::string(typeid(typename Traits::JPoseRendererT).name()) + "_jtra_diff.png");
    SaveDebugImage(diff_jrot, std::string(typeid(typename Traits::JPoseRendererT).name()) + "_jrot_diff.png");

    SaveDebugImage(result_jtra, std::string(typeid(typename Traits::JPoseRendererT).name()) + "_jtra_result.png");
    SaveDebugImage(ref_jtra, std::string(typeid(typename Traits::JPoseRendererT).name()) + "_jtra_reference.png");

    SaveDebugImage(result_jrot, std::string(typeid(typename Traits::JPoseRendererT).name()) + "_jrot_result.png");
    SaveDebugImage(ref_jrot, std::string(typeid(typename Traits::JPoseRendererT).name()) + "_jrot_reference.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";}
}

// Test image renderer against ground truth
TYPED_TEST_P(GroundTruthTests, JMapReferenceValidation)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, true, true, true);
    typename Traits::MeshT mesh_sceen(this->screen_vertex_, this->screen_indices_, true, true, false);

    typename Traits::template TextureT<float> kf_depth(this->w_, this->h_, -1.0f);
    typename Traits::template TextureT<ImageType> kf_image(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> kf_didxy(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<ImageType> f_image(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> f_didxy(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));

    typename Traits::template TextureT<ImageType> image_1(this->w_, this->h_, 0);
    typename Traits::template TextureT<ImageType> image_2(this->w_, this->h_, 0);
    typename Traits::template TextureT<Vec3<float>> reference_jmap(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));

    typename Traits::template TextureT<Vec3<float>> output_jmap(this->w_, this->h_, Vec3<float>(0.0, 0.0, 0.0));
    typename Traits::template TextureT<Vec3<PidType>> output_pids(this->w_, this->h_, Vec3<PidType>(-1, -1, -1));
    typename Traits::template TextureT<float> output_r(this->w_, this->h_, 0);

    UploadMatToTexture(kf_depth, 0, this->depth_src_cv_);
    UploadMatToTexture(kf_image, 0, this->image_src_cv_);

    UploadMatToTexture(f_image, 0, this->image_dst_cv_);

    typename Traits::ImageRendererT image_renderer;
    typename Traits::DIDxyRendererT didxy_renderer;
    typename Traits::JMapRendererT jmap_renderer;

    // didxy_renderer.Render(mesh_sceen, 0, 0, f_image, f_didxy);
    // f_didxy.generate_mipmaps(0);

    // idxy_renderer.Render(mesh_sceen, 0, 0, kf_image, kf_didxy);
    // kf_didxy.generate_mipmaps(0);

    for (int lvl = 0; lvl < output_jmap.levels(); lvl++)
    {
        if (lvl > 5)
            continue;

        didxy_renderer.Render(mesh_sceen, lvl, lvl, f_image, f_didxy);
        didxy_renderer.Render(mesh_sceen, lvl, lvl, kf_image, kf_didxy);
    }

    SE3<float> pose_transform = this->pose_dst_ * this->pose_src_.inverse();
    Vec2<float> exposure(0.0, 0.0);

    std::vector<float> positions = mesh.get_positions();
    std::vector<int> indices = mesh.get_indices();
    float delta = 1e-3;

    for (int lvl = 0; lvl < output_jmap.levels(); lvl++)
    {
        if (lvl > 0)
            continue;

        reference_jmap.fill(lvl, reference_jmap.nodata());

        PerformanceTimer timer;
        timer.Start();

        //mesh.set_positions(positions);
        jmap_renderer.Render(mesh, pose_transform, this->cam_, lvl, lvl, kf_image, f_image, kf_didxy, output_jmap, output_pids, output_r);
        
        for (int i = 0; i < mesh.vertex_count(); i++)
        {
            if (i > 100)
                break;

            std::vector<float> positions_p = positions;
            std::vector<float> positions_m = positions;

            Vec3<float> vertex;
            vertex(0) = positions[i * 3 + 0];
            vertex(1) = positions[i * 3 + 1];
            vertex(2) = positions[i * 3 + 2];

            Vec3<float> vertex_p;
            Vec3<float> vertex_m;

            float depth_p = vertex(2) + delta;
            float depth_m = vertex(2) - delta;

            vertex_p = (vertex / vertex(2)) * depth_p;
            vertex_m = (vertex / vertex(2)) * depth_m;

            positions_p[i * 3 + 0] = vertex_p(0);
            positions_p[i * 3 + 1] = vertex_p(1);
            positions_p[i * 3 + 2] = vertex_p(2);

            positions_m[i * 3 + 0] = vertex_m(0);
            positions_m[i * 3 + 1] = vertex_m(1);
            positions_m[i * 3 + 2] = vertex_m(2);

            mesh.set_positions(positions_p);

            image_renderer.Render(mesh, pose_transform, exposure, this->cam_, lvl, lvl, kf_image, image_1);

            mesh.set_positions(positions_m);

            image_renderer.Render(mesh, pose_transform, exposure, this->cam_, lvl, lvl, kf_image, image_2);

            for (int y = 0; y < f_image.height(lvl); y++)
            {
                for (int x = 0; x < f_image.width(lvl); x++)
                {
                    ImageType data_1 = image_1.texel_(y, x, lvl);
                    ImageType data_2 = image_2.texel_(y, x, lvl);

                    if (data_1 == image_1.nodata() || data_2 == image_2.nodata())
                        continue;

                    RealType der = (RealType(data_1) - RealType(data_2)) / (2 * delta);

                    if(der == RealType(0.0))
                        continue;

                    Vec3<float> data = reference_jmap.texel_(y, x, lvl);

                    Vec3<PidType> pids = output_pids.texel_(y, x, lvl);

                    for (int k = 0; k < 3; k++)
                    {
                        if (pids(k) == i)
                        {
                            data(k) = der;
                        }
                    }

                    reference_jmap.set_texel_(data, y, x, lvl);
                }
            }
        }
        
        // Performance validation
        double duration = timer.Stop();
        // EXPECT_LT(duration, 1000.0) << "Rendering should complete within 1 second";

        double rmse_jmap = RMSEV(output_jmap, reference_jmap, lvl);

        std::cout << "JMapReference RMSE " << lvl << " " << rmse_jmap << std::endl;
        EXPECT_LT(rmse_jmap, this->thresholds_.ref_max_jmap_error) << "RMSE error: " << rmse_jmap;
    }

    cv::Mat result_jmap = DownloadTextureToMat(output_jmap, 0);
    cv::Mat ref_jmap = DownloadTextureToMat(reference_jmap, 0);

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

    SaveDebugImage(diff_jmap, std::string(typeid(typename Traits::JMapRendererT).name()) + "_jmap_diff.png");

    SaveDebugImage(result_jmap, std::string(typeid(typename Traits::JMapRendererT).name()) + "_jmap_result.png");
    SaveDebugImage(ref_jmap, std::string(typeid(typename Traits::JMapRendererT).name()) + "_jmap_reference.png");

    // std::cout << "Depth Rendering: " << duration << "ms\n";}
}

REGISTER_TYPED_TEST_SUITE_P(
    GroundTruthTests,
    DepthGroundTruthValidation,
    DepthReferenceValidation,
    ImageGroundTruthValidation,
    ImageReferenceValidation,
    JPoseReferenceValidation,
    JMapReferenceValidation);

using TestBackends = ::testing::Types<CPUBackendTraits
                                      // #ifdef COMPILE_GL
                                      //                                       ,
                                      //                                       GLBackendTraits
                                      // #endif
                                      >;

INSTANTIATE_TYPED_TEST_SUITE_P(AllBackends, GroundTruthTests, TestBackends);
