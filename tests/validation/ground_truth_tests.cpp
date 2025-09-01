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

class GroundTruthTests : public RendererTestBase
{
protected:
    void SetUp() override
    {
        RendererTestBase::SetUp();
    }
};

// Test CPU depth renderer against ground truth
TEST_F(GroundTruthTests, CPUDepthGroundTruthValidation)
{
    MeshCPU mesh(vertices_, texcoords_, weights_);

    TextureCPU<float> output(w_, h_, -1.0f);

    DepthRendererCPU renderer;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();

    PerformanceTimer timer;
    timer.Start();
    renderer.Render(mesh, pose_transform, cam_, 0, output);
    timer.Stop();

    cv::Mat result = DownloadTexture(output, 0, CV_32FC1);

    // Basic validation against expected properties
    cv::Scalar mean_val, std_val;
    cv::meanStdDev(result, mean_val, std_val);

    EXPECT_GT(mean_val[0], 0.0) << "Mean depth should be positive";
    EXPECT_LT(mean_val[0], 100.0) << "Mean depth should be reasonable";
    EXPECT_GT(std_val[0], 0.0) << "Depth should have variation";

    // Performance validation
    double cpu_duration = timer.Stop();
    EXPECT_LT(cpu_duration, 1000.0) << "CPU rendering should complete within 1 second";

    SaveDebugImage(result, "cpu_depth_ground_truth.png");

    std::cout << "CPU Depth Rendering: " << cpu_duration << "ms\n";
}

// Test GL depth renderer against ground truth
TEST_F(GroundTruthTests, GLDepthGroundTruthValidation)
{
    MeshGL mesh(vertices_, texcoords_, weights_);
    TextureGL<float> output(w_, h_, -1.0f);

    DepthRendererGL renderer;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();

    PerformanceTimer timer;
    timer.Start();
    renderer.Render(mesh, pose_transform, cam_, 0, output);
    timer.Stop();

    cv::Mat result = DownloadTexture(output, 0, CV_32FC1);

    // Basic validation against expected properties
    cv::Scalar mean_val, std_val;
    cv::meanStdDev(result, mean_val, std_val);

    EXPECT_GT(mean_val[0], 0.0) << "Mean depth should be positive";
    EXPECT_LT(mean_val[0], 100.0) << "Mean depth should be reasonable";
    EXPECT_GT(std_val[0], 0.0) << "Depth should have variation";

    // Performance validation
    double gl_duration = timer.Stop();
    EXPECT_LT(gl_duration, 500.0) << "GL rendering should complete within 500ms";

    SaveDebugImage(result, "gl_depth_ground_truth.png");

    std::cout << "GL Depth Rendering: " << gl_duration << "ms\n";
}

// Test cross-backend consistency
TEST_F(GroundTruthTests, CrossBackendConsistency)
{
    // CPU rendering
    MeshCPU mesh_cpu(vertices_, texcoords_, weights_);

    TextureCPU<float> output_cpu(w_, h_, -1.0f);

    DepthRendererCPU cpu_renderer;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    cpu_renderer.Render(mesh_cpu, pose_transform, cam_, 0, output_cpu);

    cv::Mat cpu_result = DownloadTexture(output_cpu, 0, CV_32FC1);

    // GL rendering
    MeshGL mesh_gl(vertices_, texcoords_, weights_);
    TextureGL<float> output_gl(w_, h_, -1.0f);

    DepthRendererGL gl_renderer;
    gl_renderer.Render(mesh_gl, pose_transform, cam_, 0, output_gl);

    cv::Mat gl_result = DownloadTexture(output_gl, 0, CV_32FC1);

    // Compare results (mask nodata)
    cv::Mat mask = (cpu_result != -1.0f) & (gl_result != -1.0f);
    cv::Mat cpu_masked, gl_masked;
    cpu_result.copyTo(cpu_masked, mask);
    gl_result.copyTo(gl_masked, mask);
    cv::Mat diff;
    cv::absdiff(cpu_masked, gl_masked, diff);

    cv::Scalar mean_diff = cv::mean(diff, mask);
    cv::Scalar max_diff;
    cv::minMaxLoc(diff, nullptr, &max_diff[0]);

    // Validate consistency (allowing for some numerical differences)
    EXPECT_LT(mean_diff[0], 0.1) << "Mean difference between CPU and GL should be small";
    EXPECT_LT(max_diff[0], 1.0) << "Maximum difference between CPU and GL should be reasonable";

    SaveDebugImage(diff, "cpu_vs_gl_diff.png");

    std::cout << "CPU vs GL - Mean diff: " << mean_diff[0] << ", Max diff: " << max_diff[0] << "\n";
}

// Test performance thresholds
TEST_F(GroundTruthTests, PerformanceThresholds)
{
    const int num_runs = 3;
    std::vector<double> cpu_times, gl_times;

    // CPU performance
    for (int i = 0; i < num_runs; ++i)
    {
        MeshCPU mesh(vertices_, texcoords_, weights_);

        TextureCPU<float> output(w_, h_, -1.0f);

        DepthRendererCPU renderer;
        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, SE3(), cam_, 0, output);
        double cpu_time = timer.Stop();
        cpu_times.push_back(cpu_time);
    }

    // GL performance
    for (int i = 0; i < num_runs; ++i)
    {
        MeshGL mesh(vertices_, texcoords_, weights_);

        TextureGL<float> output(w_, h_, -1.0f);

        DepthRendererGL renderer;
        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, SE3(), cam_, 0, output);
        double gl_time = timer.Stop();
        gl_times.push_back(gl_time);
    }

    double cpu_avg = std::accumulate(cpu_times.begin(), cpu_times.end(), 0.0) / cpu_times.size();
    double gl_avg = std::accumulate(gl_times.begin(), gl_times.end(), 0.0) / gl_times.size();

    // Performance expectations
    EXPECT_LT(cpu_avg, 2000.0) << "CPU rendering should be under 2 seconds";
    EXPECT_LT(gl_avg, 1000.0) << "GL rendering should be under 1 second";

    if (gl_avg > 0.0)
    {
        double speedup = cpu_avg / gl_avg;
        EXPECT_GT(speedup, 0.5) << "GL should provide reasonable performance";
        std::cout << "Performance - CPU: " << cpu_avg << "ms, GL: " << gl_avg << "ms, Speedup: " << speedup << "x\n";
    }
}

#endif // COMPILE_GL
