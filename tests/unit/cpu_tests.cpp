#include <gtest/gtest.h>
#include "../common/test_framework.h"

class CPURendererTests : public RendererTestBase {
protected:
    void SetUp() override {
        RendererTestBase::SetUp();
    }
};

// Test CPU depth renderer in isolation
TEST_F(CPURendererTests, DepthRendererBasicFunctionality) {
    const int in_lvl = 0, out_lvl = 0;
    
    MeshCPU mesh(vertices_, texcoords_, weights_);
    TextureCPU<float> input(w_, h_, 0.0f);
    TextureCPU<float> output(w_, h_, -1.0f); // Use -1 as nodata
    
    UploadMatToTextureCPU(input, 0, image_src_cv_);
    
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
    
    // Check for valid depth values
    int valid_pixels = 0;
    for (int y = 0; y < result.rows; ++y) {
        for (int x = 0; x < result.cols; ++x) {
            float depth = result.at<float>(y, x);
            if (depth > 0.0f) {
                valid_pixels++;
                EXPECT_LT(depth, 1000.0f) << "Depth value too large at (" << x << "," << y << ")";
            }
        }
    }
    
    EXPECT_GT(valid_pixels, result.total() * 0.1) << "Too few valid pixels rendered";
    
    SaveDebugImage(result, "cpu_depth_test.png");
}

// Test CPU image renderer
TEST_F(CPURendererTests, ImageRendererBasicFunctionality) {
    const int in_lvl = 0, out_lvl = 0;
    
    MeshCPU mesh(vertices_, texcoords_, weights_);
    TextureCPU<float> input(w_, h_, 0.0f);
    TextureCPU<float> output(w_, h_, -1.0f);
    
    UploadMatToTextureCPU(input, 0, image_src_cv_);
    
    ImageRendererCPU renderer;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    ASSERT_NO_THROW(renderer.Render(mesh, pose_transform, cam_, input, output, in_lvl, out_lvl));
    
    cv::Mat result = DownloadTextureFromCPU(output, out_lvl, w_, h_, CV_32FC1);
    
    // Validate output properties
    cv::Scalar mean_val, std_val;
    cv::meanStdDev(result, mean_val, std_val);
    
    EXPECT_GE(mean_val[0], 0.0) << "Mean intensity should be non-negative";
    EXPECT_LE(mean_val[0], 255.0) << "Mean intensity should be reasonable";
    
    SaveDebugImage(result, "cpu_image_test.png");
}

// Test CPU DIDxy renderer
TEST_F(CPURendererTests, DIDxyRendererBasicFunctionality) {
    const int in_lvl = 0, out_lvl = 0;
    
    std::vector<float> quad_pos, quad_uv, quad_weights;
    CreateScreenQuad(quad_pos, quad_uv, quad_weights);
    
    MeshCPU mesh(quad_pos, quad_uv, quad_weights);
    TextureCPU<float> input(w_, h_, 0.0f);
    TextureCPU<Vec3> output(w_, h_, Vec3(0.0f, 0.0f, 0.0f));
    
    UploadMatToTextureCPU(input, 0, image_src_cv_);
    
    DIDxyRendererCPU renderer;
    
    ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, in_lvl, out_lvl));
    
    cv::Mat result = DownloadTextureFromCPU(output, out_lvl, w_, h_, CV_32FC3);
    
    // Check gradient properties
    cv::Mat channels[3];
    cv::split(result, channels);
    
    cv::Scalar mean_dx = cv::mean(channels[0]);
    cv::Scalar mean_dy = cv::mean(channels[1]);
    
    // Gradients should have some variation
    cv::Scalar std_dx, std_dy;
    cv::meanStdDev(channels[0], cv::Scalar(), std_dx);
    cv::meanStdDev(channels[1], cv::Scalar(), std_dy);
    
    EXPECT_GT(std_dx[0], 0.01) << "X gradient should have variation";
    EXPECT_GT(std_dy[0], 0.01) << "Y gradient should have variation";
    
    SaveDebugImageColor(result, "cpu_didxy_test.png");
}

// Test CPU Jtra renderer
TEST_F(CPURendererTests, JtraRendererBasicFunctionality) {
    const int in_lvl = 0, out_lvl = 0;
    
    // First compute gradients
    std::vector<float> quad_pos, quad_uv, quad_weights;
    CreateScreenQuad(quad_pos, quad_uv, quad_weights);
    
    MeshCPU mesh_img(quad_pos, quad_uv, quad_weights);
    MeshCPU mesh(vertices_, texcoords_, weights_);
    
    TextureCPU<float> input(w_, h_, 0.0f);
    TextureCPU<Vec3> didxy(w_, h_, Vec3(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3> jtra(w_, h_, Vec3(0.0f, 0.0f, 0.0f));
    
    UploadMatToTextureCPU(input, 0, image_dst_cv_);
    
    DIDxyRendererCPU didxy_renderer;
    JtraRendererCPU jtra_renderer;
    
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    ASSERT_NO_THROW(didxy_renderer.Render(mesh_img, SE3(), cam_, input, didxy, in_lvl, out_lvl));
    ASSERT_NO_THROW(jtra_renderer.Render(mesh, pose_transform, cam_, didxy, jtra, in_lvl, out_lvl));
    
    cv::Mat result = DownloadTextureFromCPU(jtra, out_lvl, w_, h_, CV_32FC3);
    
    // Check Jacobian properties
    cv::Scalar mean_jtra = cv::mean(result);
    
    // Jacobian values should be reasonable
    for (int i = 0; i < 3; ++i) {
        EXPECT_LT(std::abs(mean_jtra[i]), 1000.0) << "Jacobian component " << i << " too large";
    }
    
    SaveDebugImageColor(result, "cpu_jtra_test.png");
}

// Test error handling and edge cases
TEST_F(CPURendererTests, ErrorHandlingAndEdgeCases) {
    const int in_lvl = 0, out_lvl = 0;
    
    // Test with empty mesh
    std::vector<float> empty_vertices, empty_texcoords, empty_weights;
    MeshCPU empty_mesh(empty_vertices, empty_texcoords, empty_weights);
    
    TextureCPU<float> input(w_, h_, 0.0f);
    TextureCPU<float> output(w_, h_, -1.0f);
    
    DepthRendererCPU renderer;
    
    // Should handle empty mesh gracefully
    ASSERT_NO_THROW(renderer.Render(empty_mesh, SE3(), cam_, input, output, in_lvl, out_lvl));
    
    // Test with identity transformation
    MeshCPU mesh(vertices_, texcoords_, weights_);
    ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, in_lvl, out_lvl));
    
    // Test with different mipmap levels
    if (w_ >= 4 && h_ >= 4) {
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, 0, 1));
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, 1, 0));
    }
}

// Test memory management and resource cleanup
TEST_F(CPURendererTests, MemoryManagementTest) {
    const int iterations = 10;
    
    for (int i = 0; i < iterations; ++i) {
        // Create and destroy resources multiple times
        MeshCPU mesh(vertices_, texcoords_, weights_);
        TextureCPU<float> input(w_, h_, 0.0f);
        TextureCPU<float> output(w_, h_, -1.0f);
        
        UploadMatToTextureCPU(input, 0, image_src_cv_);
        
        DepthRendererCPU renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, 0, 0));
        
        // Resources should be automatically cleaned up
    }
    
    // Test should complete without memory leaks or crashes
    SUCCEED();
}

// Test numerical precision and consistency
TEST_F(CPURendererTests, NumericalPrecisionTest) {
    const int in_lvl = 0, out_lvl = 0;
    const int iterations = 5;
    
    MeshCPU mesh(vertices_, texcoords_, weights_);
    TextureCPU<float> input(w_, h_, 0.0f);
    UploadMatToTextureCPU(input, 0, image_src_cv_);
    
    DepthRendererCPU renderer;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    std::vector<cv::Mat> results;
    results.reserve(iterations);
    
    // Run same computation multiple times
    for (int i = 0; i < iterations; ++i) {
        TextureCPU<float> output(w_, h_, -1.0f);
        renderer.Render(mesh, pose_transform, cam_, input, output, in_lvl, out_lvl);
        results.push_back(DownloadTextureFromCPU(output, out_lvl, w_, h_, CV_32FC1));
    }
    
    // Results should be identical (deterministic)
    for (int i = 1; i < iterations; ++i) {
        double error = ComputeL2Error<float>(results[0], results[i], -1.0f);
        EXPECT_LT(error, 1e-6) << "Results should be deterministic, iteration " << i;
    }
}
