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

class CrossBackendTests : public TwoViewTests
{
protected:
    ValidationThresholds thresholds_;

    void SetUp() override
    {
        TwoViewTests::SetUp();

        // Set cross-backend validation thresholds
        // thresholds_.max_l2_error = 1.5;
        // thresholds_.max_cross_backend_error = 0.5;
        // thresholds_.max_execution_time_ms = 500.0;
        // thresholds_.max_performance_ratio = 5.0;
    }
};

// Compare CPU vs GL image rendering
TEST_F(CrossBackendTests, MipMapComparison)
{
    TextureCPU<float> input_cpu(w_, h_, -1.0f);
    TextureGL<float> input_gl(w_, h_, -1.0f);

    UploadMatToTexture(input_cpu, 0, image_src_cv_);
    UploadMatToTexture(input_gl, 0, image_src_cv_);

    double acc_cpu_time = 0.0, acc_gl_time = 0.0, acc_l2_error = 0.0;

    for (int in_lvl = 0; in_lvl < input_cpu.levels(); ++in_lvl)
    {
        if (in_lvl > 3)
            continue;

        timer_.Start();
        cv::Mat cpu_result = DownloadTextureToMat(input_cpu, in_lvl, CV_32FC1);
        acc_cpu_time += timer_.Stop();

        timer_.Start();
        cv::Mat gl_result = DownloadTextureToMat(input_gl, in_lvl, CV_32FC1);
        acc_gl_time += timer_.Stop();

        int valid_cpu = CountValid(input_cpu, in_lvl);
        int valid_gl = CountValid(input_gl, in_lvl);
        int valid_diff = std::abs(valid_cpu - valid_gl);

        EXPECT_LT(valid_diff, thresholds_.cr_max_valid_diff) << "Cross-backend validation failed with valid diff: " << valid_diff << " in lvl " << in_lvl;

        double l2_error = ComputeL2Error(cpu_result, gl_result, -1.0f);
        EXPECT_LT(l2_error, thresholds_.cr_max_mipmap_error) << "Cross-backend validation failed with L2 error: " << l2_error << " in lvl " << in_lvl;
        acc_l2_error = std::max(acc_l2_error, l2_error);
    }

    cv::Mat cpu_result = DownloadTextureToMat(input_cpu, 1, CV_32FC1);
    cv::Mat gl_result = DownloadTextureToMat(input_gl, 1, CV_32FC1);

    SaveDebugImage(cpu_result, "mipmap_image_cpu.png");
    SaveDebugImage(gl_result, "mipmap_image_gl.png");

    // cv::Mat diff_image;
    // cv::absdiff(cpu_result, gl_result, diff_image);
    // SaveDebugImage(diff_image, "cross_image_diff.png");

    std::cout << "MipMap Cross-Backend Comparison:\n";
    std::cout << "  L2 Error: " << acc_l2_error << "\n";
    std::cout << "  CPU Time: " << acc_cpu_time << " ms\n";
    std::cout << "  GL Time:  " << acc_gl_time << " ms\n";
    std::cout << "  Speedup:  " << (acc_cpu_time / acc_gl_time) << "x\n";

    // Cross-backend validation
    // TestValidator::ValidateCrossBackend(cpu_result, gl_result, thresholds_);
    // TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    // EXPECT_LT(cpu_time, thresholds_.max_cpu_image_time_ms) << "CPU execution time exceeded threshold: " << cpu_time << "ms";
    // EXPECT_LT(gl_time, thresholds_.max_gl_image_time_ms) << "GL execution time exceeded threshold: " << gl_time << "ms";
}

// Compare CPU vs GL depth rendering
TEST_F(CrossBackendTests, GouraudRenderingComparison)
{
    linalg::SE3<float> pose_transform = pose_dst_ * pose_src_.inverse();

    MeshCPU mesh_cpu(vertex_, indices_, image_src_cv_);
    MeshGL mesh_gl(vertex_, indices_, image_src_cv_);

    TextureCPU<linalg::Vec3<float>> output_cpu(w_, h_, linalg::Vec3<float>(-1.0f, -1.0f, -1.0f));
    TextureGL<linalg::Vec3<float>> output_gl(w_, h_, linalg::Vec3<float>(-1.0f, -1.0f, -1.0f));

    GouraudRendererCPU renderer_cpu;
    GouraudRendererGL renderer_gl;

    double acc_cpu_time = 0.0, acc_gl_time = 0.0, acc_l2_error = 0.0;

    linalg::Vec3<float> light_pos(0.0, -0.5, 0.1);
    linalg::Vec3<float> light_color(1.0, 0.0, 0.0);
    linalg::Vec3<float> ambient_reflectance(0.0, 0.0, 0.0);
    linalg::Vec3<float> diffuse_reflectance(0.0, 0.0, 0.0);
    linalg::Vec3<float> specular_reflectance(1.0, 1.0, 1.0);
    float shininess = 1.0;
    linalg::Vec3<float> ambient_light(0.0, 0.0, 0.0);

    for (int lvl = 0; lvl < output_cpu.levels(); ++lvl)
    {
        if (lvl > 3)
            continue;

        timer_.Start();
        renderer_cpu.Render(mesh_cpu, pose_transform, cam_, light_pos, light_color, ambient_reflectance, diffuse_reflectance, specular_reflectance, shininess, ambient_light, lvl, output_cpu);
        cv::Mat cpu_result = DownloadTextureToMat(output_cpu, lvl, CV_32FC3);
        acc_cpu_time += timer_.Stop();

        timer_.Start();
        renderer_gl.Render(mesh_gl, pose_transform, cam_, light_pos, light_color, ambient_reflectance, diffuse_reflectance, specular_reflectance, shininess, ambient_light, lvl, output_gl);
        cv::Mat gl_result = DownloadTextureToMat(output_gl, lvl, CV_32FC3);
        acc_gl_time += timer_.Stop();

        int valid_cpu = CountValid(output_cpu, lvl);
        int valid_gl = CountValid(output_gl, lvl);
        int valid_diff = std::abs(valid_cpu - valid_gl);

        EXPECT_LT(valid_diff, thresholds_.cr_max_valid_diff) << "Cross-backend validation failed with valid diff: " << valid_diff << " lvl " << lvl;

        // Detailed error analysis
        double l2_error = ComputeL2Error(cpu_result, gl_result, cv::Vec3f(-1.0f, -1.0f, -1.0f));
        EXPECT_LT(l2_error, thresholds_.cr_max_gouraud_error) << "Cross-backend validation failed with L2 error: " << l2_error << " lvl " << lvl;
        acc_l2_error = std::max(acc_l2_error, l2_error);
    }

    cv::Mat cpu_result = DownloadTextureToMat(output_cpu, 0, CV_32FC3);
    cv::Mat gl_result = DownloadTextureToMat(output_gl, 0, CV_32FC3);

    // Save comparison images
    SaveDebugImage(cpu_result, "cross_gouraud_cpu.png");
    SaveDebugImage(gl_result, "cross_gouraud_gl.png");

    // Create difference image
    // cv::Mat diff_image;
    // cv::absdiff(cpu_result, gl_result, diff_image);
    // SaveDebugImage(diff_image, "cross_depth_diff.png");

    std::cout << "Gouraud Rendering Cross-Backend Comparison:\n";
    std::cout << "  L2 Error: " << acc_l2_error << "\n";
    std::cout << "  CPU Time: " << acc_cpu_time << " ms\n";
    std::cout << "  GL Time:  " << acc_gl_time << " ms\n";
    std::cout << "  Speedup:  " << (acc_cpu_time / acc_gl_time) << "x\n";

    // Cross-backend validation
    // TestValidator::ValidateCrossBackend(cpu_result, gl_result, thresholds_);
    // EXPECT_LT(l2_error, thresholds_.cr_max_depth_error) << "Cross-backend validation failed with L2 error: " << l2_error;
    // EXPECT_LT(cpu_time, thresholds_.max_cpu_depth_time_ms) << "CPU execution time exceeded threshold: " << cpu_time << "ms";
    // EXPECT_LT(gl_time, thresholds_.max_gl_depth_time_ms) << "GL execution time exceeded threshold: " << gl_time << "ms";
    //  TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
}

// Compare CPU vs GL depth rendering
TEST_F(CrossBackendTests, DepthRenderingComparison)
{
    linalg::SE3<float> pose_transform = pose_dst_ * pose_src_.inverse();

    MeshCPU mesh_cpu(vertex_, indices_, image_src_cv_);
    MeshGL mesh_gl(vertex_, indices_, image_src_cv_);

    TextureCPU<float> output_cpu(w_, h_, -1.0f);
    TextureGL<float> output_gl(w_, h_, -1.0f);

    DepthRendererCPU renderer_cpu;
    DepthRendererGL renderer_gl;

    double acc_cpu_time = 0.0, acc_gl_time = 0.0, acc_l2_error = 0.0;

    for (int lvl = 0; lvl < output_cpu.levels(); ++lvl)
    {
        if (lvl > 3)
            continue;

        timer_.Start();
        renderer_cpu.Render(mesh_cpu, pose_transform, cam_, lvl, output_cpu);
        cv::Mat cpu_result = DownloadTextureToMat(output_cpu, lvl, CV_32FC1);
        acc_cpu_time += timer_.Stop();

        timer_.Start();
        renderer_gl.Render(mesh_gl, pose_transform, cam_, lvl, output_gl);
        cv::Mat gl_result = DownloadTextureToMat(output_gl, lvl, CV_32FC1);
        acc_gl_time += timer_.Stop();

        int valid_cpu = CountValid(output_cpu, lvl);
        int valid_gl = CountValid(output_gl, lvl);
        int valid_diff = std::abs(valid_cpu - valid_gl);

        EXPECT_LT(valid_diff, thresholds_.cr_max_valid_diff) << "Cross-backend validation failed with valid diff: " << valid_diff << " lvl " << lvl;

        // Detailed error analysis
        double l2_error = ComputeL2Error(cpu_result, gl_result, -1.0f);
        EXPECT_LT(l2_error, thresholds_.cr_max_depth_error) << "Cross-backend validation failed with L2 error: " << l2_error << " lvl " << lvl;
        acc_l2_error = std::max(acc_l2_error, l2_error);
    }

    cv::Mat cpu_result = DownloadTextureToMat(output_cpu, 1, CV_32FC1);
    cv::Mat gl_result = DownloadTextureToMat(output_gl, 1, CV_32FC1);

    SaveDebugImage(cpu_result, "cross_depth_cpu.png");
    SaveDebugImage(gl_result, "cross_depth_gl.png");

    // Create difference image
    // cv::Mat diff_image;
    // cv::absdiff(cpu_result, gl_result, diff_image);
    // SaveDebugImage(diff_image, "cross_depth_diff.png");

    std::cout << "Depth Rendering Cross-Backend Comparison:\n";
    std::cout << "  L2 Error: " << acc_l2_error << "\n";
    std::cout << "  CPU Time: " << acc_cpu_time << " ms\n";
    std::cout << "  GL Time:  " << acc_gl_time << " ms\n";
    std::cout << "  Speedup:  " << (acc_cpu_time / acc_gl_time) << "x\n";

    // Cross-backend validation
    // TestValidator::ValidateCrossBackend(cpu_result, gl_result, thresholds_);
    // EXPECT_LT(l2_error, thresholds_.cr_max_depth_error) << "Cross-backend validation failed with L2 error: " << l2_error;
    // EXPECT_LT(cpu_time, thresholds_.max_cpu_depth_time_ms) << "CPU execution time exceeded threshold: " << cpu_time << "ms";
    // EXPECT_LT(gl_time, thresholds_.max_gl_depth_time_ms) << "GL execution time exceeded threshold: " << gl_time << "ms";
    //  TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
}

// Compare CPU vs GL image rendering
TEST_F(CrossBackendTests, ImageRenderingComparison)
{
    linalg::SE3<float> pose_transform = pose_dst_ * pose_src_.inverse();

    MeshCPU mesh_cpu(vertex_, indices_, image_src_cv_);
    MeshGL mesh_gl(vertex_, indices_, image_src_cv_);

    // TextureCPU<float> input_cpu(w_, h_, -1.0f);
    TextureCPU<float> output_cpu(w_, h_, -1.0f);

    // TextureGL<float> input_gl(w_, h_, -1.0f);
    TextureGL<float> output_gl(w_, h_, -1.0f);

    // UploadMatToTexture(input_cpu, 0, image_src_cv_);
    // UploadMatToTexture(input_gl, 0, image_src_cv_);

    double acc_cpu_time = 0.0, acc_gl_time = 0.0, acc_l2_error = 0.0;

    for (int out_lvl = 0; out_lvl < output_cpu.levels(); ++out_lvl)
    {
        // for (int in_lvl = 0; in_lvl < input_cpu.levels(); ++in_lvl)
        int in_lvl = out_lvl;
        {
            if (in_lvl > 3)
                continue;
            if (out_lvl > 3)
                continue;

            ImageRendererCPU renderer_cpu;
            timer_.Start();
            renderer_cpu.Render(mesh_cpu, pose_transform, cam_, in_lvl, out_lvl, output_cpu);
            cv::Mat cpu_result = DownloadTextureToMat(output_cpu, out_lvl, CV_32FC1);
            acc_cpu_time += timer_.Stop();

            ImageRendererGL renderer_gl;
            timer_.Start();
            renderer_gl.Render(mesh_gl, pose_transform, cam_, in_lvl, out_lvl, output_gl);
            cv::Mat gl_result = DownloadTextureToMat(output_gl, out_lvl, CV_32FC1);
            acc_gl_time += timer_.Stop();

            int valid_cpu = CountValid(output_cpu, out_lvl);
            int valid_gl = CountValid(output_gl, out_lvl);
            int valid_diff = std::abs(valid_cpu - valid_gl);

            EXPECT_LT(valid_diff, thresholds_.cr_max_valid_diff) << "Cross-backend validation failed with valid diff: " << valid_diff << " in lvl " << in_lvl << " out lvl " << out_lvl;

            double l2_error = ComputeL2Error(cpu_result, gl_result, -1.0f);
            EXPECT_LT(l2_error, thresholds_.cr_max_image_error) << "Cross-backend validation failed with L2 error: " << l2_error << " in lvl " << in_lvl << " out lvl " << out_lvl;
            acc_l2_error = std::max(acc_l2_error, l2_error);
        }
    }

    cv::Mat cpu_result = DownloadTextureToMat(output_cpu, 1, CV_32FC1);
    cv::Mat gl_result = DownloadTextureToMat(output_gl, 1, CV_32FC1);

    SaveDebugImage(cpu_result, "cross_image_cpu.png");
    SaveDebugImage(gl_result, "cross_image_gl.png");

    // cv::Mat diff_image;
    // cv::absdiff(cpu_result, gl_result, diff_image);
    // SaveDebugImage(diff_image, "cross_image_diff.png");

    std::cout << "Image Rendering Cross-Backend Comparison:\n";
    std::cout << "  L2 Error: " << acc_l2_error << "\n";
    std::cout << "  CPU Time: " << acc_cpu_time << " ms\n";
    std::cout << "  GL Time:  " << acc_gl_time << " ms\n";
    std::cout << "  Speedup:  " << (acc_cpu_time / acc_gl_time) << "x\n";

    // Cross-backend validation
    // TestValidator::ValidateCrossBackend(cpu_result, gl_result, thresholds_);
    // TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    // EXPECT_LT(cpu_time, thresholds_.max_cpu_image_time_ms) << "CPU execution time exceeded threshold: " << cpu_time << "ms";
    // EXPECT_LT(gl_time, thresholds_.max_gl_image_time_ms) << "GL execution time exceeded threshold: " << gl_time << "ms";
}

// Compare CPU vs GL image rendering
TEST_F(CrossBackendTests, ResidualRenderingComparison)
{
    linalg::SE3<float> pose_transform = pose_dst_ * pose_src_.inverse();

    MeshCPU mesh_cpu(vertex_, indices_, image_src_cv_);
    MeshGL mesh_gl(vertex_, indices_, image_src_cv_);

    TextureCPU<float> input1_cpu(w_, h_, -1.0f);
    TextureCPU<float> input2_cpu(w_, h_, -1.0f);
    TextureCPU<float> output_cpu(w_, h_, 0.0f);

    TextureGL<float> input1_gl(w_, h_, -1.0f);
    TextureGL<float> input2_gl(w_, h_, -1.0f);
    TextureGL<float> output_gl(w_, h_, 0.0f);

    UploadMatToTexture(input1_cpu, 0, image_src_cv_);
    UploadMatToTexture(input2_cpu, 0, image_dst_cv_);
    UploadMatToTexture(input1_gl, 0, image_src_cv_);
    UploadMatToTexture(input2_gl, 0, image_dst_cv_);

    ResidualRendererCPU renderer_cpu;
    ResidualRendererGL renderer_gl;

    double acc_cpu_time = 0.0, acc_gl_time = 0.0, acc_l2_error = 0.0;

    for (int out_lvl = 0; out_lvl < output_cpu.levels(); ++out_lvl)
    {
        // for (int in_lvl = 0; in_lvl < input1_cpu.levels(); ++in_lvl)
        int in_lvl = out_lvl;
        {
            if (in_lvl > 3)
                continue;
            if (out_lvl > 3)
                continue;

            timer_.Start();
            renderer_cpu.Render(mesh_cpu, pose_transform, cam_, in_lvl, out_lvl, input1_cpu, input2_cpu, output_cpu);
            cv::Mat cpu_result = DownloadTextureToMat(output_cpu, out_lvl, CV_32FC1);
            acc_cpu_time += timer_.Stop();

            timer_.Start();
            renderer_gl.Render(mesh_gl, pose_transform, cam_, in_lvl, out_lvl, input1_gl, input2_gl, output_gl);
            cv::Mat gl_result = DownloadTextureToMat(output_gl, out_lvl, CV_32FC1);
            acc_gl_time += timer_.Stop();

            int valid_cpu = CountValid(output_cpu, out_lvl);
            int valid_gl = CountValid(output_gl, out_lvl);
            int valid_diff = std::abs(valid_cpu - valid_gl);

            EXPECT_LT(valid_diff, thresholds_.cr_max_valid_diff) << "Cross-backend validation failed with valid diff: " << valid_diff << " cpu: " << valid_cpu << " gl: " << valid_gl << " in lvl " << in_lvl << " out lvl " << out_lvl;

            double l2_error = ComputeL2Error(cpu_result, gl_result, 0.0f);
            EXPECT_LT(l2_error, thresholds_.cr_max_residual_error) << "Cross-backend validation failed with L2 error: " << l2_error << " in lvl " << in_lvl << " out lvl " << out_lvl;
            acc_l2_error = std::max(acc_l2_error, l2_error);
        }
    }

    cv::Mat cpu_result = DownloadTextureToMat(output_cpu, 1, CV_32FC1);
    cv::Mat gl_result = DownloadTextureToMat(output_gl, 1, CV_32FC1);

    SaveDebugImage(cpu_result, "cross_residual_cpu.png");
    SaveDebugImage(gl_result, "cross_residual_gl.png");

    // cv::Mat diff_image;
    // cv::absdiff(cpu_result, gl_result, diff_image);
    // SaveDebugImage(diff_image, "cross_residual_diff.png");

    std::cout << "Residual Rendering Cross-Backend Comparison:\n";
    std::cout << "  L2 Error: " << acc_l2_error << "\n";
    std::cout << "  CPU Time: " << acc_cpu_time << " ms\n";
    std::cout << "  GL Time:  " << acc_gl_time << " ms\n";
    std::cout << "  Speedup:  " << (acc_cpu_time / acc_gl_time) << "x\n";

    // Cross-backend validation
    // TestValidator::ValidateCrossBackend(cpu_result, gl_result, thresholds_);
    // TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    // EXPECT_LT(cpu_time, thresholds_.max_cpu_image_time_ms) << "CPU execution time exceeded threshold: " << cpu_time << "ms";
    // EXPECT_LT(gl_time, thresholds_.max_gl_image_time_ms) << "GL execution time exceeded threshold: " << gl_time << "ms";
}

// Compare CPU vs GL image rendering
TEST_F(CrossBackendTests, L2RenderingComparison)
{
    linalg::SE3<float> pose_transform = pose_dst_ * pose_src_.inverse();

    MeshCPU mesh_cpu(vertex_, indices_, image_src_cv_);
    MeshGL mesh_gl(vertex_, indices_, image_src_cv_);

    TextureCPU<float> input1_cpu(w_, h_, -1.0f);
    TextureCPU<float> input2_cpu(w_, h_, -1.0f);
    TextureCPU<float> output_cpu(w_, h_, -1.0f);

    TextureGL<float> input1_gl(w_, h_, -1.0f);
    TextureGL<float> input2_gl(w_, h_, -1.0f);
    TextureGL<float> output_gl(w_, h_, -1.0f);

    UploadMatToTexture(input2_cpu, 0, image_dst_cv_);
    UploadMatToTexture(input2_gl, 0, image_dst_cv_);

    L2RendererCPU renderer_cpu;
    L2RendererGL renderer_gl;

    double acc_cpu_time = 0.0, acc_gl_time = 0.0, acc_l2_error = 0.0;

    for (int out_lvl = 0; out_lvl < output_cpu.levels(); ++out_lvl)
    {
        // for (int in_lvl = 0; in_lvl < output_cpu.levels(); ++in_lvl)
        int in_lvl = out_lvl;
        {
            if (in_lvl > 3)
                continue;
            if (out_lvl > 3)
                continue;

            timer_.Start();
            renderer_cpu.Render(mesh_cpu, pose_transform, cam_, in_lvl, out_lvl, input2_cpu, output_cpu);
            cv::Mat cpu_result = DownloadTextureToMat(output_cpu, out_lvl, CV_32FC1);
            acc_cpu_time += timer_.Stop();

            timer_.Start();
            renderer_gl.Render(mesh_gl, pose_transform, cam_, in_lvl, out_lvl, input2_gl, output_gl);
            cv::Mat gl_result = DownloadTextureToMat(output_gl, out_lvl, CV_32FC1);
            acc_gl_time += timer_.Stop();

            int valid_cpu = CountValid(output_cpu, out_lvl);
            int valid_gl = CountValid(output_gl, out_lvl);
            int valid_diff = std::abs(valid_cpu - valid_gl);

            EXPECT_LT(valid_diff, thresholds_.cr_max_valid_diff) << "Cross-backend validation failed with valid diff: " << valid_diff << " in lvl " << in_lvl << " out lvl " << out_lvl;

            double l2_error = ComputeL2Error(cpu_result, gl_result, -1.0f);
            EXPECT_LT(l2_error, thresholds_.cr_max_l2_error) << "Cross-backend validation failed with L2 error: " << l2_error << " in lvl " << in_lvl << " out lvl " << out_lvl;
            acc_l2_error = std::max(acc_l2_error, l2_error);
        }
    }

    cv::Mat cpu_result = DownloadTextureToMat(output_cpu, 1, CV_32FC1);
    cv::Mat gl_result = DownloadTextureToMat(output_gl, 1, CV_32FC1);

    SaveDebugImage(cpu_result, "cross_l2_cpu.png");
    SaveDebugImage(gl_result, "cross_l2_gl.png");

    // cv::Mat diff_image;
    // cv::absdiff(cpu_result, gl_result, diff_image);
    // SaveDebugImage(diff_image, "cross_l2_diff.png");

    std::cout << "L2 Rendering Cross-Backend Comparison:\n";
    std::cout << "  L2 Error: " << acc_l2_error << "\n";
    std::cout << "  CPU Time: " << acc_cpu_time << " ms\n";
    std::cout << "  GL Time:  " << acc_gl_time << " ms\n";
    std::cout << "  Speedup:  " << (acc_cpu_time / acc_gl_time) << "x\n";

    // Cross-backend validation
    // TestValidator::ValidateCrossBackend(cpu_result, gl_result, thresholds_);
    // TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    // EXPECT_LT(cpu_time, thresholds_.max_cpu_image_time_ms) << "CPU execution time exceeded threshold: " << cpu_time << "ms";
    // EXPECT_LT(gl_time, thresholds_.max_gl_image_time_ms) << "GL execution time exceeded threshold: " << gl_time << "ms";
}

// Compare CPU vs GL gradient computation
TEST_F(CrossBackendTests, GradientComputationComparison)
{
    MeshCPU mesh_cpu(screen_vertex_, screen_indices_, image_src_cv_, true, true, false);
    MeshGL mesh_gl(screen_vertex_, screen_indices_, image_src_cv_, true, true, false);

    TextureCPU<float> input_cpu(w_, h_, -1.0f);
    TextureCPU<linalg::Vec3<float>> output_cpu(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<float> input_gl(w_, h_, -1.0f);
    TextureGL<linalg::Vec3<float>> output_gl(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));

    UploadMatToTexture(input_cpu, 0, image_src_cv_);
    UploadMatToTexture(input_gl, 0, image_src_cv_);

    DIDxyRendererCPU renderer_cpu;
    DIDxyRendererGL renderer_gl;

    double acc_cpu_time = 0.0, acc_gl_time = 0.0, acc_l2_error = 0.0;

    for (int out_lvl = 0; out_lvl < output_cpu.levels(); ++out_lvl)
    {
        // for (int in_lvl = 0; in_lvl < input_cpu.levels(); ++in_lvl)
        int in_lvl = out_lvl;
        {
            if (in_lvl > 3)
                continue;
            if (out_lvl > 3)
                continue;

            timer_.Start();
            renderer_cpu.Render(mesh_cpu, in_lvl, out_lvl, input_cpu, output_cpu);
            cv::Mat cpu_result = DownloadTextureToMat(output_cpu, out_lvl, CV_32FC3);
            acc_cpu_time += timer_.Stop();

            timer_.Start();
            renderer_gl.Render(mesh_gl, in_lvl, out_lvl, input_gl, output_gl);
            cv::Mat gl_result = DownloadTextureToMat(output_gl, out_lvl, CV_32FC3);
            acc_gl_time += timer_.Stop();

            int valid_cpu = CountValid(output_cpu, out_lvl);
            int valid_gl = CountValid(output_gl, out_lvl);
            int valid_diff = std::abs(valid_cpu - valid_gl);

            EXPECT_LT(valid_diff, thresholds_.cr_max_valid_diff) << "Cross-backend validation failed with valid diff: " << valid_diff << " cpu: " << valid_cpu << " gl: " << valid_gl << " in lvl " << in_lvl << " out lvl " << out_lvl;

            // Cross-backend validation for Vec3 data
            double l2_error = ComputeL2Error(cpu_result, gl_result, cv::Vec3f(0.0f, 0.0f, 0.0f));
            EXPECT_LT(l2_error, thresholds_.cr_max_didxy_error) << "Cross-backend validation failed with L2 error: " << l2_error << " in lvl " << in_lvl << " out lvl " << out_lvl;
            acc_l2_error = std::max(acc_l2_error, l2_error);
        }
    }

    cv::Mat cpu_result = DownloadTextureToMat(output_cpu, 1, CV_32FC3);
    cv::Mat gl_result = DownloadTextureToMat(output_gl, 1, CV_32FC3);

    SaveDebugImageColor(cpu_result, "cross_gradient_cpu.png");
    SaveDebugImageColor(gl_result, "cross_gradient_gl.png");

    // cv::Mat diff_image;
    // cv::absdiff(cpu_result, gl_result, diff_image);
    // SaveDebugImageColor(diff_image, "cross_gradient_diff.png");

    std::cout << "Gradient Computation Cross-Backend Comparison:\n";
    std::cout << "  L2 Error: " << acc_l2_error << "\n";
    std::cout << "  CPU Time: " << acc_cpu_time << " ms\n";
    std::cout << "  GL Time:  " << acc_gl_time << " ms\n";
    std::cout << "  Speedup:  " << (acc_cpu_time / acc_gl_time) << "x\n";

    // TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    // EXPECT_LT(cpu_time, thresholds_.max_cpu_didxy_time_ms) << "CPU execution time exceeded threshold: " << cpu_time << "ms";
    // EXPECT_LT(gl_time, thresholds_.max_gl_didxy_time_ms) << "GL execution time exceeded threshold: " << gl_time << "ms";
}

// Test full Jacobian pipeline comparison
TEST_F(CrossBackendTests, JPosePipelineComparison)
{
    linalg::SE3<float> pose_transform = pose_dst_ * pose_src_.inverse();

    // CPU pipeline
    MeshCPU mesh_img_cpu(screen_vertex_, screen_indices_, image_src_cv_, true, true, false);
    MeshCPU mesh_cpu(vertex_, indices_, image_src_cv_);
    MeshGL mesh_img_gl(screen_vertex_, screen_indices_, image_src_cv_, true, true, false);
    MeshGL mesh_gl(vertex_, indices_, image_src_cv_);

    TextureCPU<float> f_cpu(w_, h_, -1.0f);
    TextureCPU<linalg::Vec3<float>> dfdxy_cpu(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> jtra_cpu(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> jrot_cpu(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<float> r_cpu(w_, h_, 0.0f);

    TextureGL<float> f_gl(w_, h_, -1.0f);
    TextureGL<linalg::Vec3<float>> dfdxy_gl(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<linalg::Vec3<float>> jtra_gl(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<linalg::Vec3<float>> jrot_gl(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<float> r_gl(w_, h_, 0.0f);

    UploadMatToTexture(f_cpu, 0, image_dst_cv_);
    UploadMatToTexture(f_gl, 0, image_dst_cv_);

    DIDxyRendererCPU didxy_renderer_cpu;
    JPoseRendererCPU jpose_renderer_cpu;
    DIDxyRendererGL didxy_renderer_gl;
    JPoseRendererGL jpose_renderer_gl;

    for (int lvl = 0; lvl < f_cpu.levels(); ++lvl)
    {
        didxy_renderer_cpu.Render(mesh_img_cpu, lvl, lvl, f_cpu, dfdxy_cpu);
        didxy_renderer_gl.Render(mesh_img_gl, lvl, lvl, f_gl, dfdxy_gl);
    }

    double acc_cpu_time = 0.0, acc_gl_time = 0.0, acc_jtra_error = 0.0, acc_jrot_error = 0.0, acc_r_error = 0.0;

    for (int out_lvl = 0; out_lvl < jtra_cpu.levels(); ++out_lvl)
    {
        // for (int in_lvl = 0; in_lvl < kf_gl.levels(); ++in_lvl)
        int in_lvl = out_lvl;
        {
            if (in_lvl > 3)
                continue;
            if (out_lvl > 3)
                continue;

            timer_.Start();
            jpose_renderer_cpu.Render(mesh_cpu, pose_transform, cam_, in_lvl, out_lvl, f_cpu, dfdxy_cpu, jtra_cpu, jrot_cpu, r_cpu);
            cv::Mat cpu_jtra = DownloadTextureToMat(jtra_cpu, out_lvl, CV_32FC3);
            cv::Mat cpu_jrot = DownloadTextureToMat(jrot_cpu, out_lvl, CV_32FC3);
            cv::Mat cpu_r = DownloadTextureToMat(r_cpu, out_lvl, CV_32FC1);
            acc_cpu_time += timer_.Stop();

            timer_.Start();
            jpose_renderer_gl.Render(mesh_gl, pose_transform, cam_, in_lvl, out_lvl, f_gl, dfdxy_gl, jtra_gl, jrot_gl, r_gl);
            cv::Mat gl_jtra = DownloadTextureToMat(jtra_gl, out_lvl, CV_32FC3);
            cv::Mat gl_jrot = DownloadTextureToMat(jrot_gl, out_lvl, CV_32FC3);
            cv::Mat gl_r = DownloadTextureToMat(r_gl, out_lvl, CV_32FC1);
            acc_gl_time += timer_.Stop();

            int valid_cpu = CountValid(r_cpu, out_lvl);
            int valid_gl = CountValid(r_gl, out_lvl);
            int valid_diff = std::abs(valid_cpu - valid_gl);

            // EXPECT_LT(valid_diff, thresholds_.cr_max_valid_diff) << "Cross-backend validation failed with valid diff: " << valid_diff << " cpu: " << valid_cpu << " gl: " << valid_gl << " in lvl " << in_lvl << " out lvl " << out_lvl;

            // Validate both Jtra and Jrot
            double jtra_error = ComputeL2Error(cpu_jtra, gl_jtra, cv::Vec3f(0.0f, 0.0f, 0.0f));
            double jrot_error = ComputeL2Error(cpu_jrot, gl_jrot, cv::Vec3f(0.0f, 0.0f, 0.0f));
            double r_error = ComputeL2Error(cpu_r, gl_r, 0.0);

            EXPECT_LT(jtra_error, thresholds_.cr_max_jtra_error) << "Jtra cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;
            EXPECT_LT(jrot_error, thresholds_.cr_max_jrot_error) << "Jrot cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;
            EXPECT_LT(r_error, thresholds_.cr_max_r_error) << "Jrot cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;
            acc_jtra_error = std::max(acc_jtra_error, jtra_error);
            acc_jrot_error = std::max(acc_jrot_error, jrot_error);
            acc_r_error = std::max(acc_r_error, r_error);
        }
    }

    cv::Mat cpu_jtra = DownloadTextureToMat(jtra_cpu, 1, CV_32FC3);
    cv::Mat cpu_jrot = DownloadTextureToMat(jrot_cpu, 1, CV_32FC3);
    cv::Mat cpu_r = DownloadTextureToMat(r_cpu, 1, CV_32FC1);
    cv::Mat gl_jtra = DownloadTextureToMat(jtra_gl, 1, CV_32FC3);
    cv::Mat gl_jrot = DownloadTextureToMat(jrot_gl, 1, CV_32FC3);
    cv::Mat gl_r = DownloadTextureToMat(r_gl, 1, CV_32FC1);

    SaveDebugImageColor(cpu_jtra, "cross_jtra_cpu.png");
    SaveDebugImageColor(gl_jtra, "cross_jtra_gl.png");
    SaveDebugImageColor(cpu_jrot, "cross_jrot_cpu.png");
    SaveDebugImageColor(gl_jrot, "cross_jrot_gl.png");
    SaveDebugImageColor(cpu_r, "cross_r_cpu.png");
    SaveDebugImageColor(gl_r, "cross_r_gl.png");

    std::cout << "Jacobian Pipeline Cross-Backend Comparison:\n";
    std::cout << "  Jtra L2 Error: " << acc_jtra_error << "\n";
    std::cout << "  Jrot L2 Error: " << acc_jrot_error << "\n";
    std::cout << "  r L2 Error: " << acc_r_error << "\n";
    std::cout << "  CPU Time: " << acc_cpu_time << " ms\n";
    std::cout << "  GL Time:  " << acc_gl_time << " ms\n";
    std::cout << "  Speedup:  " << (acc_cpu_time / acc_gl_time) << "x\n";

    // TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    // EXPECT_LT(cpu_time, thresholds_.max_cpu_jrot_time_ms) << "CPU execution time exceeded threshold: " << cpu_time << "ms";
    // EXPECT_LT(gl_time, thresholds_.max_gl_jrot_time_ms) << "GL execution time exceeded threshold: " << gl_time << "ms";
}

// Test full Jacobian pipeline comparison
TEST_F(CrossBackendTests, JMapPipelineComparison)
{
    linalg::SE3<float> pose_transform = pose_dst_ * pose_src_.inverse();

    // CPU pipeline
    MeshCPU mesh_img_cpu(screen_vertex_, screen_indices_, image_src_cv_, true, true, false);
    MeshCPU mesh_cpu(vertex_, indices_, image_src_cv_);
    MeshGL mesh_img_gl(screen_vertex_, screen_indices_, image_src_cv_, true, true, false);
    MeshGL mesh_gl(vertex_, indices_, image_src_cv_);

    TextureCPU<float> f_cpu(w_, h_, -1.0f);
    TextureCPU<linalg::Vec3<float>> dfdxy_cpu(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> jmap_cpu(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> pids_cpu(w_, h_, linalg::Vec3<float>(-1.0f, -1.0f, -1.0f));
    TextureCPU<float> r_cpu(w_, h_, 0.0f);

    TextureGL<float> f_gl(w_, h_, -1.0f);
    TextureGL<linalg::Vec3<float>> dfdxy_gl(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<linalg::Vec3<float>> jmap_gl(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<linalg::Vec3<float>> pids_gl(w_, h_, linalg::Vec3<float>(-1.0f, -1.0f, -1.0f));
    TextureGL<float> r_gl(w_, h_, 0.0f);

    UploadMatToTexture(f_cpu, 0, image_dst_cv_);
    UploadMatToTexture(f_gl, 0, image_dst_cv_);

    DIDxyRendererCPU didxy_renderer_cpu;
    JMapRendererCPU jpose_renderer_cpu;
    DIDxyRendererGL didxy_renderer_gl;
    JMapRendererGL jpose_renderer_gl;

    for (int lvl = 0; lvl < f_cpu.levels(); ++lvl)
    {
        didxy_renderer_cpu.Render(mesh_img_cpu, lvl, lvl, f_cpu, dfdxy_cpu);
        didxy_renderer_gl.Render(mesh_img_gl, lvl, lvl, f_gl, dfdxy_gl);
    }

    double acc_cpu_time = 0.0, acc_gl_time = 0.0, acc_jmap_error = 0.0, acc_pids_error = 0.0, acc_r_error = 0.0;
    for (int out_lvl = 0; out_lvl < jmap_cpu.levels(); ++out_lvl)
    {
        // for (int in_lvl = 0; in_lvl < kf_gl.levels(); ++in_lvl)
        int in_lvl = out_lvl;
        {
            if (in_lvl > 3)
                continue;
            if (out_lvl > 3)
                continue;

            timer_.Start();
            jpose_renderer_cpu.Render(mesh_cpu, pose_transform, cam_, in_lvl, out_lvl, f_cpu, dfdxy_cpu, jmap_cpu, pids_cpu, r_cpu);
            cv::Mat cpu_jmap = DownloadTextureToMat(jmap_cpu, out_lvl, CV_32FC3);
            cv::Mat cpu_pids = DownloadTextureToMat(pids_cpu, out_lvl, CV_32FC3);
            cv::Mat cpu_r = DownloadTextureToMat(r_cpu, out_lvl, CV_32FC1);
            acc_cpu_time += timer_.Stop();

            timer_.Start();
            jpose_renderer_gl.Render(mesh_gl, pose_transform, cam_, in_lvl, out_lvl, f_gl, dfdxy_gl, jmap_gl, pids_gl, r_gl);
            cv::Mat gl_jmap = DownloadTextureToMat(jmap_gl, out_lvl, CV_32FC3);
            cv::Mat gl_pids = DownloadTextureToMat(pids_gl, out_lvl, CV_32FC3);
            cv::Mat gl_r = DownloadTextureToMat(r_gl, out_lvl, CV_32FC1);
            acc_gl_time += timer_.Stop();

            int valid_cpu = CountValid(r_cpu, out_lvl);
            int valid_gl = CountValid(r_gl, out_lvl);
            int valid_diff = std::abs(valid_cpu - valid_gl);

            EXPECT_LT(valid_diff, thresholds_.cr_max_valid_diff) << "Cross-backend validation failed with valid diff: " << valid_diff << " cpu: " << valid_cpu << " gl: " << valid_gl << " in lvl " << in_lvl << " out lvl " << out_lvl;

            // Validate both Jtra and Jrot
            double jmap_error = ComputeL2Error(cpu_jmap, gl_jmap, cv::Vec3f(0.0f, 0.0f, 0.0f));
            double pids_error = ComputeL2Error(cpu_pids, gl_pids, cv::Vec3f(-1.0f, -1.0f, -1.0f));
            double r_error = ComputeL2Error(cpu_r, gl_r, 0.0);

            EXPECT_LT(jmap_error, thresholds_.cr_max_jmap_error) << "Jtra cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;
            EXPECT_LT(pids_error, thresholds_.cr_max_pids_error) << "Jrot cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;
            EXPECT_LT(r_error, thresholds_.cr_max_r_error) << "Jrot cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;

            acc_jmap_error = std::max(acc_jmap_error, jmap_error);
            acc_pids_error = std::max(acc_pids_error, pids_error);
            acc_r_error = std::max(acc_r_error, r_error);
        }
    }

    cv::Mat cpu_jmap = DownloadTextureToMat(jmap_cpu, 1, CV_32FC3);
    cv::Mat cpu_pids = DownloadTextureToMat(pids_cpu, 1, CV_32FC3);
    cv::Mat cpu_r = DownloadTextureToMat(r_cpu, 1, CV_32FC1);
    cv::Mat gl_jmap = DownloadTextureToMat(jmap_gl, 1, CV_32FC3);
    cv::Mat gl_pids = DownloadTextureToMat(pids_gl, 1, CV_32FC3);
    cv::Mat gl_r = DownloadTextureToMat(r_gl, 1, CV_32FC1);

    SaveDebugImageColor(cpu_jmap, "cross_jmap_cpu.png");
    SaveDebugImageColor(gl_jmap, "cross_jmap_gl.png");
    SaveDebugImageColor(cpu_pids, "cross_pids_cpu.png");
    SaveDebugImageColor(gl_pids, "cross_pids_gl.png");
    // SaveDebugImageColor(cpu_r, "cross_r_cpu.png");
    // SaveDebugImageColor(gl_r, "cross_r_gl.png");

    std::cout << "JMap Pipeline Cross-Backend Comparison:\n";
    std::cout << "  Jmap L2 Error: " << acc_jmap_error << "\n";
    std::cout << "  Pids L2 Error: " << acc_pids_error << "\n";
    std::cout << "  r L2 Error: " << acc_r_error << "\n";
    std::cout << "  CPU Time: " << acc_cpu_time << " ms\n";
    std::cout << "  GL Time:  " << acc_gl_time << " ms\n";
    std::cout << "  Speedup:  " << (acc_cpu_time / acc_gl_time) << "x\n";

    // TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    // EXPECT_LT(cpu_time, thresholds_.max_cpu_jrot_time_ms) << "CPU execution time exceeded threshold: " << cpu_time << "ms";
    // EXPECT_LT(gl_time, thresholds_.max_gl_jrot_time_ms) << "GL execution time exceeded threshold: " << gl_time << "ms";
}
/*
TEST_F(CrossBackendTests, DiffPipelineComparison)
{
    linalg::SE3<float> pose_transform = pose_dst_ * pose_src_.inverse();

    // CPU pipeline
    MeshCPU mesh_img_cpu(screen_vertices_, screen_texcoords_, screen_indices_);
    MeshCPU mesh_cpu(vertices_, texcoords_, indices_, image_src_cv_);
    MeshGL mesh_img_gl(screen_vertices_, screen_texcoords_, screen_indices_);
    MeshGL mesh_gl(vertices_, texcoords_, indices_, image_src_cv_);

    TextureCPU<float> image_cpu(w_, h_, -1.0f);
    TextureCPU<float> depth_cpu(w_, h_, -1.0f);
    TextureCPU<linalg::Vec3<float>> jtra_cpu(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> jrot_cpu(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> jmap_cpu(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureCPU<linalg::Vec3<float>> pids_cpu(w_, h_, linalg::Vec3<float>(-1.0f, -1.0f, -1.0f));

    TextureGL<float> image_gl(w_, h_, -1.0f);
    TextureGL<float> depth_gl(w_, h_, -1.0f);
    TextureGL<linalg::Vec3<float>> jtra_gl(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<linalg::Vec3<float>> jrot_gl(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<linalg::Vec3<float>> jmap_gl(w_, h_, linalg::Vec3<float>(0.0f, 0.0f, 0.0f));
    TextureGL<linalg::Vec3<float>> pids_gl(w_, h_, linalg::Vec3<float>(-1.0f, -1.0f, -1.0f));

    DiffRendererCPU jpose_renderer_cpu;
    DiffRendererGL jpose_renderer_gl;

    double acc_cpu_time = 0.0, acc_gl_time = 0.0, acc_image_error = 0.0, acc_depth_error = 0.0, acc_jtra_error = 0.0, acc_jrot_error = 0.0, acc_jmap_error = 0.0, acc_pids_error = 0.0;
    for (int out_lvl = 0; out_lvl < jmap_cpu.levels(); ++out_lvl)
    {
        // for (int in_lvl = 0; in_lvl < kf_gl.levels(); ++in_lvl)
        int in_lvl = out_lvl;
        {
            if (in_lvl > 3)
                continue;
            if (out_lvl > 3)
                continue;

            timer_.Start();
            jpose_renderer_cpu.Render(mesh_cpu, pose_transform, cam_, in_lvl, out_lvl, image_cpu, depth_cpu, jtra_cpu, jrot_cpu, jmap_cpu, pids_cpu);
            cv::Mat cpu_image = DownloadTextureToMat(image_cpu, out_lvl, CV_32FC1);
            cv::Mat cpu_depth = DownloadTextureToMat(depth_cpu, out_lvl, CV_32FC1);
            cv::Mat cpu_jtra = DownloadTextureToMat(jtra_cpu, out_lvl, CV_32FC3);
            cv::Mat cpu_jrot = DownloadTextureToMat(jrot_cpu, out_lvl, CV_32FC3);
            cv::Mat cpu_jmap = DownloadTextureToMat(jmap_cpu, out_lvl, CV_32FC3);
            cv::Mat cpu_pids = DownloadTextureToMat(pids_cpu, out_lvl, CV_32FC3);
            acc_cpu_time += timer_.Stop();

            timer_.Start();
            jpose_renderer_gl.Render(mesh_gl, pose_transform, cam_, in_lvl, out_lvl, image_gl, depth_gl, jtra_gl, jrot_gl, jmap_gl, pids_gl);
            cv::Mat gl_image = DownloadTextureToMat(image_gl, out_lvl, CV_32FC1);
            cv::Mat gl_depth = DownloadTextureToMat(depth_gl, out_lvl, CV_32FC1);
            cv::Mat gl_jtra = DownloadTextureToMat(jtra_gl, out_lvl, CV_32FC3);
            cv::Mat gl_jrot = DownloadTextureToMat(jrot_gl, out_lvl, CV_32FC3);
            cv::Mat gl_jmap = DownloadTextureToMat(jmap_gl, out_lvl, CV_32FC3);
            cv::Mat gl_pids = DownloadTextureToMat(pids_gl, out_lvl, CV_32FC3);
            acc_gl_time += timer_.Stop();

            // int valid_cpu = CountValid(r_cpu, out_lvl);
            // int valid_gl = CountValid(r_gl, out_lvl);
            // int valid_diff = std::abs(valid_cpu - valid_gl);

            // EXPECT_LT(valid_diff, thresholds_.cr_max_valid_diff) << "Cross-backend validation failed with valid diff: " << valid_diff << " cpu: " << valid_cpu << " gl: " << valid_gl << " in lvl " << in_lvl << " out lvl " << out_lvl;

            // Validate both Jtra and Jrot
            double image_error = ComputeL2Error(cpu_image, gl_image, -1.0);
            double depth_error = ComputeL2Error(cpu_depth, gl_depth, -1.0);
            double jtra_error = ComputeL2Error(cpu_jtra, gl_jtra, cv::Vec3f(0.0f, 0.0f, 0.0f));
            double jrot_error = ComputeL2Error(cpu_jrot, gl_jrot, cv::Vec3f(0.0f, 0.0f, 0.0f));
            double jmap_error = ComputeL2Error(cpu_jmap, gl_jmap, cv::Vec3f(0.0f, 0.0f, 0.0f));
            double pids_error = ComputeL2Error(cpu_pids, gl_pids, cv::Vec3f(-1.0f, -1.0f, -1.0f));

            EXPECT_LT(image_error, thresholds_.cr_max_image_error) << "Jtra cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;
            EXPECT_LT(depth_error, thresholds_.cr_max_depth_error) << "Jtra cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;
            EXPECT_LT(jtra_error, thresholds_.cr_max_jtra_error) << "Jtra cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;
            EXPECT_LT(jrot_error, thresholds_.cr_max_jrot_error) << "Jtra cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;
            EXPECT_LT(jmap_error, thresholds_.cr_max_jmap_error) << "Jtra cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;
            EXPECT_LT(pids_error, thresholds_.cr_max_pids_error) << "Jrot cross-backend error too high" << " in lvl " << in_lvl << " out lvl " << out_lvl;

            acc_image_error = std::max(acc_image_error, image_error);
            acc_depth_error = std::max(acc_depth_error, depth_error);
            acc_jtra_error = std::max(acc_jtra_error, jtra_error);
            acc_jrot_error = std::max(acc_jrot_error, jrot_error);
            acc_jmap_error = std::max(acc_jmap_error, jmap_error);
            acc_pids_error = std::max(acc_pids_error, pids_error);
        }
    }

    cv::Mat cpu_image = DownloadTextureToMat(image_cpu, 0, CV_32FC1);
    cv::Mat cpu_depth = DownloadTextureToMat(depth_cpu, 0, CV_32FC1);
    cv::Mat cpu_jtra = DownloadTextureToMat(jtra_cpu, 0, CV_32FC3);
    cv::Mat cpu_jrot = DownloadTextureToMat(jrot_cpu, 0, CV_32FC3);
    cv::Mat cpu_jmap = DownloadTextureToMat(jmap_cpu, 0, CV_32FC3);
    cv::Mat cpu_pids = DownloadTextureToMat(pids_cpu, 0, CV_32FC3);

    cv::Mat gl_image = DownloadTextureToMat(image_gl, 0, CV_32FC1);
    cv::Mat gl_depth = DownloadTextureToMat(depth_gl, 0, CV_32FC1);
    cv::Mat gl_jtra = DownloadTextureToMat(jtra_gl, 0, CV_32FC3);
    cv::Mat gl_jrot = DownloadTextureToMat(jrot_gl, 0, CV_32FC3);
    cv::Mat gl_jmap = DownloadTextureToMat(jmap_gl, 0, CV_32FC3);
    cv::Mat gl_pids = DownloadTextureToMat(pids_gl, 0, CV_32FC3);

    SaveDebugImageColor(cpu_image, "cross_diff_image_cpu.png");
    SaveDebugImageColor(gl_image, "cross_diff_image_gl.png");
    SaveDebugImageColor(cpu_depth, "cross_diff_depth_cpu.png");
    SaveDebugImageColor(gl_depth, "cross_diff_depth_gl.png");
    SaveDebugImageColor(cpu_jtra, "cross_diff_jtra_cpu.png");
    SaveDebugImageColor(gl_jtra, "cross_diff_jtra_gl.png");
    SaveDebugImageColor(cpu_jrot, "cross_diff_jrot_cpu.png");
    SaveDebugImageColor(gl_jrot, "cross_diff_jrot_gl.png");
    SaveDebugImageColor(cpu_jmap, "cross_diff_jmap_cpu.png");
    SaveDebugImageColor(gl_jmap, "cross_diff_jmap_gl.png");
    SaveDebugImageColor(cpu_pids, "cross_diff_pids_cpu.png");
    SaveDebugImageColor(gl_pids, "cross_diff_pids_gl.png");
    // SaveDebugImageColor(cpu_r, "cross_r_cpu.png");
    // SaveDebugImageColor(gl_r, "cross_r_gl.png");

    std::cout << "JMap Pipeline Cross-Backend Comparison:\n";
    std::cout << "  image L2 Error: " << acc_image_error << "\n";
    std::cout << "  depth L2 Error: " << acc_depth_error << "\n";
    std::cout << "  Jtra L2 Error: " << acc_jtra_error << "\n";
    std::cout << "  Jrot L2 Error: " << acc_jrot_error << "\n";
    std::cout << "  Jmap L2 Error: " << acc_jmap_error << "\n";
    std::cout << "  Pids L2 Error: " << acc_pids_error << "\n";

    std::cout << "  CPU Time: " << acc_cpu_time << " ms\n";
    std::cout << "  GL Time:  " << acc_gl_time << " ms\n";
    std::cout << "  Speedup:  " << (acc_cpu_time / acc_gl_time) << "x\n";

    // TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    // EXPECT_LT(cpu_time, thresholds_.max_cpu_jrot_time_ms) << "CPU execution time exceeded threshold: " << cpu_time << "ms";
    // EXPECT_LT(gl_time, thresholds_.max_gl_jrot_time_ms) << "GL execution time exceeded threshold: " << gl_time << "ms";
}
*/
/*
// Test different mesh complexities
TEST_F(CrossBackendTests, MeshComplexityComparison)
{
    const std::vector<int> grid_sizes = {8, 16, 32, 64};

    for (int grid_size : grid_sizes)
    {
        std::cout << "Testing mesh complexity: " << grid_size << "x" << grid_size << "\n";

        // Create mesh with different complexity
        std::vector<Vec2> grid_uv = UniformTexCoords(grid_size, grid_size);
        std::vector<float> test_vertices, test_texcoords, test_weights;

        test_vertices.reserve(grid_uv.size() * 3);
        test_texcoords.reserve(grid_uv.size() * 2);
        test_weights.reserve(grid_uv.size());

        for (const Vec2 &uv : grid_uv)
        {
            const float ix = uv(0) * (w_ - 1);
            const float iy = uv(1) * (h_ - 1);
            const int x = static_cast<int>(ix);
            const int y = static_cast<int>(iy);
            const float depth = depth_src_cv_.at<float>(y, x);

            if (depth <= 0.0f)
                continue;

            const Vec3 ray = cam_.PixToRay(uv);
            const Vec3 vertex = ray * depth;

            test_vertices.push_back(vertex(0));
            test_vertices.push_back(vertex(1));
            test_vertices.push_back(vertex(2));
            test_texcoords.push_back(uv(0));
            test_texcoords.push_back(uv(1));
            test_weights.push_back(1.0f);
        }

        if (test_vertices.empty())
            continue;

        // Test CPU
        MeshCPU mesh_cpu(test_vertices, test_texcoords, test_weights);
        TextureCPU<float> input_cpu(w_, h_, 0.0f);
        TextureCPU<float> output_cpu(w_, h_, 0.0f);
        UploadMatToTexture(input_cpu, 0, image_src_cv_);

        DepthRendererCPU renderer_cpu;
        timer_.Start();
        renderer_cpu.Render(mesh_cpu, SE3(), cam_, input_cpu, output_cpu, 0, 0);
        double cpu_time = timer_.Stop();

        // Test GL
        MeshGL mesh_gl(test_vertices, test_texcoords, test_weights);
        TextureGL<float> input_gl(w_, h_, 0.0f);
        TextureGL<float> output_gl(w_, h_, 0.0f);
        UploadMatToTexture(input_gl, 0, image_src_cv_);

        DepthRendererGL renderer_gl;
        timer_.Start();
        renderer_gl.Render(mesh_gl, SE3(), cam_, input_gl, output_gl, 0, 0);
        double gl_time = timer_.Stop();

        std::cout << "  Vertices: " << (test_vertices.size() / 3) << "\n";
        std::cout << "  CPU Time: " << cpu_time << " ms\n";
        std::cout << "  GL Time:  " << gl_time << " ms\n";
        std::cout << "  Speedup:  " << (cpu_time / gl_time) << "x\n";

        // Performance should scale reasonably
        EXPECT_LT(cpu_time, grid_size * 2.0) << "CPU scaling issue at grid size " << grid_size;
        EXPECT_LT(gl_time, grid_size * 0.5) << "GL scaling issue at grid size " << grid_size;
    }
}
*/

// Test numerical precision comparison
TEST_F(CrossBackendTests, NumericalPrecisionComparison)
{
    const int out_lvl = 0;
    const int iterations = 3;

    linalg::SE3<float> pose_transform = pose_dst_ * pose_src_.inverse();

    std::vector<cv::Mat> cpu_results, gl_results;
    cpu_results.reserve(iterations);
    gl_results.reserve(iterations);

    for (int i = 0; i < iterations; ++i)
    {
        // CPU
        MeshCPU mesh_cpu(vertex_, indices_, image_src_cv_);
        TextureCPU<float> output_cpu(w_, h_, 0.0f);

        DepthRendererCPU renderer_cpu;
        renderer_cpu.Render(mesh_cpu, pose_transform, cam_, out_lvl, output_cpu);
        cpu_results.push_back(DownloadTextureToMat(output_cpu, out_lvl, CV_32FC1));

        // GL
        MeshGL mesh_gl(vertex_, indices_, image_src_cv_);
        TextureGL<float> output_gl(w_, h_, 0.0f);

        DepthRendererGL renderer_gl;
        renderer_gl.Render(mesh_gl, pose_transform, cam_, out_lvl, output_gl);
        gl_results.push_back(DownloadTextureToMat(output_gl, out_lvl, CV_32FC1));
    }

    // Check CPU consistency
    for (int i = 1; i < iterations; ++i)
    {
        double cpu_error = ComputeL2Error(cpu_results[0], cpu_results[i], 0.0f);
        EXPECT_LT(cpu_error, 1e-5) << "CPU results not deterministic, iteration " << i;
    }

    // Check GL consistency
    for (int i = 1; i < iterations; ++i)
    {
        double gl_error = ComputeL2Error(gl_results[0], gl_results[i], 0.0f);
        EXPECT_LT(gl_error, 1e-4) << "GL results not sufficiently consistent, iteration " << i;
    }

    // Check cross-backend consistency
    double cross_error = ComputeL2Error(cpu_results[0], gl_results[0], 0.0f);
    EXPECT_LT(cross_error, thresholds_.cr_max_depth_error)
        << "Cross-backend precision error: " << cross_error;

    std::cout << "Numerical Precision Comparison:\n";
    std::cout << "  CPU Consistency: < 1e-5\n";
    std::cout << "  GL Consistency:  < 1e-4\n";
    std::cout << "  Cross-Backend:   " << cross_error << "\n";
}

#endif // COMPILE_GL
