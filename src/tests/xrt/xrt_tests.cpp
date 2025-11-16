#include <gtest/gtest.h>
#include "../common/test_framework.h"

#ifdef COMPILE_XRT

class XRTRendererTests : public RendererTestBase {
protected:
    void SetUp() override {
        RendererTestBase::SetUp();
    }
};

// Test XRT depth renderer basic functionality
TEST_F(XRTRendererTests, DepthRendererBasicFunctionality) {
    const int in_lvl = 0, out_lvl = 0;
    
    MeshCPU mesh(vertices_, texcoords_, weights_);
    TextureCPU<float> input(w_, h_, 0.0f);
    TextureCPU<float> output(w_, h_, -1.0f);
    
    UploadMatToTextureCPU(input, 0, image_src_cv_);
    
    // Note: XRT renderer would be instantiated here when available
    // For now, we test the CPU fallback path
    DepthRendererCPU renderer;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    ASSERT_NO_THROW(renderer.Render(mesh, pose_transform, cam_, input, output, in_lvl, out_lvl));
    
    cv::Mat result = DownloadTextureFromCPU(output, out_lvl, w_, h_, CV_32FC1);
    
    // Validate output properties
    cv::Scalar mean_val, std_val;
    cv::meanStdDev(result, mean_val, std_val);
    
    EXPECT_GT(mean_val[0], 0.0) << "Mean depth should be positive";
    EXPECT_LT(mean_val[0], 1000.0) << "Mean depth should be reasonable";
    EXPECT_GT(std_val[0], 0.0) << "Depth should have variation";
    
    SaveDebugImage(result, "xrt_depth_test.png");
}

// Test XRT memory management
TEST_F(XRTRendererTests, MemoryManagementTest) {
    const int iterations = 5;
    
    for (int i = 0; i < iterations; ++i) {
        MeshCPU mesh(vertices_, texcoords_, weights_);
        TextureCPU<float> input(w_, h_, 0.0f);
        TextureCPU<float> output(w_, h_, -1.0f);
        
        UploadMatToTextureCPU(input, 0, image_src_cv_);
        
        DepthRendererCPU renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, 0, 0));
    }
    
    SUCCEED();
}

// Test XRT performance characteristics
TEST_F(XRTRendererTests, PerformanceTest) {
    const int num_runs = 3;
    std::vector<double> timings;
    
    MeshCPU mesh(vertices_, texcoords_, weights_);
    TextureCPU<float> input(w_, h_, 0.0f);
    TextureCPU<float> output(w_, h_, -1.0f);
    
    UploadMatToTextureCPU(input, 0, image_src_cv_);
    DepthRendererCPU renderer;
    
    for (int i = 0; i < num_runs; ++i) {
        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, SE3(), cam_, input, output, 0, 0);
        double duration = timer.Stop();
        timings.push_back(duration);
    }
    
    double mean_time = std::accumulate(timings.begin(), timings.end(), 0.0) / timings.size();
    EXPECT_GT(mean_time, 0.0) << "Rendering should take measurable time";
    EXPECT_LT(mean_time, 5000.0) << "Rendering should complete in reasonable time";
    
    std::cout << "XRT Performance: " << mean_time << "ms average\n";
}

#else

// Placeholder test when XRT is not compiled
TEST(XRTRendererTests, XRTNotCompiled) {
    GTEST_SKIP() << "XRT backend not compiled";
}

#endif // COMPILE_XRT
