#include <gtest/gtest.h>
#include "../common/test_framework.h"

#ifdef COMPILE_GL

// GL context setup
class GLContextEnv : public ::testing::Environment {
public:
    void SetUp() override { InitEGL(); }
    void TearDown() override {}
};
static ::testing::Environment* const gl_env = 
    ::testing::AddGlobalTestEnvironment(new GLContextEnv());

class GLRendererTests : public RendererTestBase {
protected:
    void SetUp() override {
        RendererTestBase::SetUp();
    }
};

// Test GL depth renderer basic functionality
TEST_F(GLRendererTests, DepthRendererBasicFunctionality) {
    MeshGL mesh(vertices_, texcoords_, weights_);
    TextureGL<float> input(w_, h_, 0.0f);
    TextureGL<float> output(w_, h_, -1.0f);
    
    UploadMatToTextureGL(input, 0, image_src_cv_);
    
    DepthRendererGL renderer;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    ASSERT_NO_THROW(renderer.Render(mesh, pose_transform, cam_, input, output, 0, 0));
    
    cv::Mat result = DownloadTextureFromGL(output, 0, w_, h_, CV_32FC1);
    
    // Validate output properties
    cv::Scalar mean_val, std_val;
    cv::meanStdDev(result, mean_val, std_val);
    
    EXPECT_GT(mean_val[0], 0.0) << "Mean depth should be positive";
    EXPECT_LT(mean_val[0], 1000.0) << "Mean depth should be reasonable";
    EXPECT_GT(std_val[0], 0.0) << "Depth should have variation";
    
    SaveDebugImage(result, "gl_depth_basic.png");
}

// Test GL error handling
TEST_F(GLRendererTests, ErrorHandling) {
    // Test with empty mesh
    std::vector<float> empty_vertices, empty_texcoords, empty_weights;
    MeshGL empty_mesh(empty_vertices, empty_texcoords, empty_weights);
    TextureGL<float> input(w_, h_, 0.0f);
    TextureGL<float> output(w_, h_, -1.0f);
    
    DepthRendererGL renderer;
    ASSERT_NO_THROW(renderer.Render(empty_mesh, SE3(), cam_, input, output, 0, 0));
    
    // Check GL error state
    GLenum error = glGetError();
    EXPECT_EQ(error, GL_NO_ERROR) << "Empty mesh should not cause GL errors";
}

// Test GL framebuffer operations
TEST_F(GLRendererTests, FramebufferOperations) {
    MeshGL mesh(vertices_, texcoords_, weights_);
    
    // Test different texture sizes
    const std::vector<std::pair<int, int>> sizes = {{64, 64}, {128, 128}, {256, 256}};
    
    for (const auto& size : sizes) {
        int w = size.first, h = size.second;
        
        TextureGL<float> input(w, h, 0.0f);
        TextureGL<float> output(w, h, -1.0f);
        
        // Create test pattern
        cv::Mat test_image(h, w, CV_32FC1);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                test_image.at<float>(y, x) = (x + y) % 256;
            }
        }
        
        UploadMatToTextureGL(input, 0, test_image);
        
        DepthRendererGL renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, 0, 0));
        
        GLenum error = glGetError();
        EXPECT_EQ(error, GL_NO_ERROR) << "Size " << w << "x" << h << " should not cause GL errors";
    }
}

// Test GL resource management
TEST_F(GLRendererTests, ResourceManagement) {
    const int num_iterations = 10;
    
    for (int i = 0; i < num_iterations; ++i) {
        MeshGL mesh(vertices_, texcoords_, weights_);
        TextureGL<float> input(w_, h_, 0.0f);
        TextureGL<float> output(w_, h_, -1.0f);
        
        UploadMatToTextureGL(input, 0, image_src_cv_);
        
        DepthRendererGL renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, 0, 0));
        
        // Resources should be cleaned up automatically when going out of scope
    }
    
    GLenum error = glGetError();
    EXPECT_EQ(error, GL_NO_ERROR) << "Resource management should not cause GL errors";
}

// Test GL performance consistency
TEST_F(GLRendererTests, PerformanceConsistency) {
    MeshGL mesh(vertices_, texcoords_, weights_);
    TextureGL<float> input(w_, h_, 0.0f);
    TextureGL<float> output(w_, h_, -1.0f);
    
    UploadMatToTextureGL(input, 0, image_src_cv_);
    
    DepthRendererGL renderer;
    
    std::vector<double> timings;
    const int num_runs = 5;
    
    for (int i = 0; i < num_runs; ++i) {
        PerformanceTimer timer;
        timer.Start();
        renderer.Render(mesh, SE3(), cam_, input, output, 0, 0);
        double duration = timer.Stop();
        timings.push_back(duration);
    }
    
    // Check timing consistency (coefficient of variation should be reasonable)
    double mean_time = std::accumulate(timings.begin(), timings.end(), 0.0) / timings.size();
    double variance = 0.0;
    for (double t : timings) {
        variance += (t - mean_time) * (t - mean_time);
    }
    variance /= timings.size();
    double std_dev = std::sqrt(variance);
    double cv = std_dev / mean_time;
    
    EXPECT_LT(cv, 0.5) << "Performance should be reasonably consistent (CV < 50%)";
    EXPECT_GT(mean_time, 0.0) << "Rendering should take measurable time";
    
    std::cout << "GL Depth Rendering: " << mean_time << "ms ± " << std_dev << "ms (CV: " << cv << ")\n";
}

#endif // COMPILE_GL
