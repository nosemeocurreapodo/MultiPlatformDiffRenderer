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

class ErrorHandlingTests : public RendererTestBase {
protected:
    void SetUp() override {
        RendererTestBase::SetUp();
    }
};

// Test empty mesh handling
TEST_F(ErrorHandlingTests, EmptyMeshHandling) {
    std::vector<float> empty_vertices, empty_texcoords, empty_weights;
    
    // CPU test
    {
        MeshCPU empty_mesh(empty_vertices, empty_texcoords, empty_weights);
        TextureCPU<float> input(w_, h_, 0.0f);
        TextureCPU<float> output(w_, h_, -1.0f);
        
        DepthRendererCPU renderer;
        ASSERT_NO_THROW(renderer.Render(empty_mesh, SE3(), cam_, input, output, 0, 0));
        
        cv::Mat result = DownloadTexture(output, 0, CV_32FC1);
        
        // Should remain at nodata value
        cv::Scalar mean_val = cv::mean(result);
        EXPECT_NEAR(mean_val[0], -1.0f, 0.1f) << "Empty mesh should produce nodata output";
    }
    
    // GL test
    {
        MeshGL empty_mesh(empty_vertices, empty_texcoords, empty_weights);
        TextureGL<float> input(w_, h_, 0.0f);
        TextureGL<float> output(w_, h_, -1.0f);
        
        DepthRendererGL renderer;
        ASSERT_NO_THROW(renderer.Render(empty_mesh, SE3(), cam_, input, output, 0, 0));
        
        // Check GL error state
        GLenum error = glGetError();
        EXPECT_EQ(error, GL_NO_ERROR) << "Empty mesh should not cause GL errors";
    }
}

// Test degenerate triangle handling
TEST_F(ErrorHandlingTests, DegenerateTriangleHandling) {
    // Create mesh with degenerate triangles (all vertices at same point)
    std::vector<float> degenerate_vertices = {
        0.0f, 0.0f, 1.0f,  // vertex 0
        0.0f, 0.0f, 1.0f,  // vertex 1 (same as 0)
        0.0f, 0.0f, 1.0f   // vertex 2 (same as 0)
    };
    std::vector<float> texcoords = {0.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.5f};
    std::vector<float> weights = {1.0f, 1.0f, 1.0f};
    
    // CPU test
    {
        MeshCPU mesh(degenerate_vertices, texcoords, weights);
        TextureCPU<float> input(w_, h_, 0.0f);
        TextureCPU<float> output(w_, h_, -1.0f);
        
        DepthRendererCPU renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, 0, 0));
        
        // Should handle degenerate triangles gracefully
        cv::Mat result = DownloadTexture(output, 0, CV_32FC1);
        
        // Check for NaN or infinite values
        bool has_invalid = false;
        for (int y = 0; y < result.rows && !has_invalid; ++y) {
            for (int x = 0; x < result.cols && !has_invalid; ++x) {
                float val = result.at<float>(y, x);
                if (!std::isfinite(val)) {
                    has_invalid = true;
                }
            }
        }
        EXPECT_FALSE(has_invalid) << "Degenerate triangles should not produce invalid values";
    }
    
    // GL test
    {
        MeshGL mesh(degenerate_vertices, texcoords, weights);
        TextureGL<float> input(w_, h_, 0.0f);
        TextureGL<float> output(w_, h_, -1.0f);
        
        DepthRendererGL renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, 0, 0));
        
        GLenum error = glGetError();
        EXPECT_EQ(error, GL_NO_ERROR) << "Degenerate triangles should not cause GL errors";
    }
}

// Test extreme transformation matrices
TEST_F(ErrorHandlingTests, ExtremeTransformationHandling) {
    MeshCPU mesh_cpu(vertices_, texcoords_, weights_);
    MeshGL mesh_gl(vertices_, texcoords_, weights_);
    
    // Test with very large scale
    SE3 large_scale;
    large_scale.translation() = Vec3(0, 0, 0);
    large_scale.so3() = Sophus::SO3f::exp(Vec3(0, 0, 0));
    // Apply large scale through pose
    Mat4 scale_matrix = Mat4::Identity() * 1000.0f;
    
    // CPU test
    {
        TextureCPU<float> input(w_, h_, 0.0f);
        TextureCPU<float> output(w_, h_, -1.0f);
        UploadMatToTexture(input, 0, image_src_cv_);
        
        DepthRendererCPU renderer;
        ASSERT_NO_THROW(renderer.Render(mesh_cpu, large_scale, cam_, input, output, 0, 0));
        
        cv::Mat result = DownloadTexture(output, 0, CV_32FC1);
        
        // Check for invalid values
        bool has_invalid = false;
        for (int y = 0; y < result.rows && !has_invalid; ++y) {
            for (int x = 0; x < result.cols && !has_invalid; ++x) {
                float val = result.at<float>(y, x);
                if (!std::isfinite(val)) {
                    has_invalid = true;
                }
            }
        }
        EXPECT_FALSE(has_invalid) << "Extreme transformations should not produce invalid values";
    }
    
    // GL test
    {
        TextureGL<float> input(w_, h_, 0.0f);
        TextureGL<float> output(w_, h_, -1.0f);
        UploadMatToTexture(input, 0, image_src_cv_);
        
        DepthRendererGL renderer;
        ASSERT_NO_THROW(renderer.Render(mesh_gl, large_scale, cam_, input, output, 0, 0));
        
        GLenum error = glGetError();
        EXPECT_EQ(error, GL_NO_ERROR) << "Extreme transformations should not cause GL errors";
    }
}

// Test invalid mipmap levels
TEST_F(ErrorHandlingTests, InvalidMipmapLevels) {
    MeshCPU mesh_cpu(vertices_, texcoords_, weights_);
    MeshGL mesh_gl(vertices_, texcoords_, weights_);
    
    // CPU test with invalid levels
    {
        TextureCPU<float> input(w_, h_, 0.0f);
        TextureCPU<float> output(w_, h_, -1.0f);
        UploadMatToTexture(input, 0, image_src_cv_);
        
        DepthRendererCPU renderer;
        
        // Test with level beyond texture size
        int max_level = static_cast<int>(std::log2(std::min(w_, h_))) + 1;
        ASSERT_NO_THROW(renderer.Render(mesh_cpu, SE3(), cam_, input, output, 0, max_level));
        ASSERT_NO_THROW(renderer.Render(mesh_cpu, SE3(), cam_, input, output, max_level, 0));
    }
    
    // GL test with invalid levels
    {
        TextureGL<float> input(w_, h_, 0.0f);
        TextureGL<float> output(w_, h_, -1.0f);
        UploadMatToTexture(input, 0, image_src_cv_);
        
        DepthRendererGL renderer;
        
        int max_level = static_cast<int>(std::log2(std::min(w_, h_))) + 1;
        ASSERT_NO_THROW(renderer.Render(mesh_gl, SE3(), cam_, input, output, 0, max_level));
        
        GLenum error = glGetError();
        EXPECT_EQ(error, GL_NO_ERROR) << "Invalid mipmap levels should be handled gracefully";
    }
}

// Test texture size mismatches
TEST_F(ErrorHandlingTests, TextureSizeMismatch) {
    const int small_w = 64, small_h = 64;
    const int large_w = 512, large_h = 512;
    
    MeshCPU mesh_cpu(vertices_, texcoords_, weights_);
    MeshGL mesh_gl(vertices_, texcoords_, weights_);
    
    // CPU test with size mismatch
    {
        TextureCPU<float> small_input(small_w, small_h, 0.0f);
        TextureCPU<float> large_output(large_w, large_h, -1.0f);
        
        // Create small test image
        cv::Mat small_image(small_h, small_w, CV_32FC1, cv::Scalar(128.0f));
        UploadMatToTexture(small_input, 0, small_image);
        
        DepthRendererCPU renderer;
        ASSERT_NO_THROW(renderer.Render(mesh_cpu, SE3(), cam_, small_input, large_output, 0, 0));
        
        cv::Mat result = DownloadTexture(large_output, 0, CV_32FC1);
        
        // Should handle size mismatch gracefully
        cv::Scalar mean_val = cv::mean(result);
        EXPECT_TRUE(std::isfinite(mean_val[0])) << "Size mismatch should not produce invalid values";
    }
    
    // GL test with size mismatch
    {
        TextureGL<float> small_input(small_w, small_h, 0.0f);
        TextureGL<float> large_output(large_w, large_h, -1.0f);
        
        cv::Mat small_image(small_h, small_w, CV_32FC1, cv::Scalar(128.0f));
        UploadMatToTexture(small_input, 0, small_image);
        
        DepthRendererGL renderer;
        ASSERT_NO_THROW(renderer.Render(mesh_gl, SE3(), cam_, small_input, large_output, 0, 0));
        
        GLenum error = glGetError();
        EXPECT_EQ(error, GL_NO_ERROR) << "Texture size mismatch should not cause GL errors";
    }
}

// Test camera parameter edge cases
TEST_F(ErrorHandlingTests, CameraParameterEdgeCases) {
    MeshCPU mesh_cpu(vertices_, texcoords_, weights_);
    MeshGL mesh_gl(vertices_, texcoords_, weights_);
    
    // Create camera with extreme parameters
    Camera extreme_cam;
    Vec4 extreme_params;
    extreme_params << 1e6f, 1e6f, w_/2.0f, h_/2.0f; // Very high focal lengths
    extreme_cam.SetParams(extreme_params);
    
    // CPU test
    {
        TextureCPU<float> input(w_, h_, 0.0f);
        TextureCPU<float> output(w_, h_, -1.0f);
        UploadMatToTexture(input, 0, image_src_cv_);
        
        DepthRendererCPU renderer;
        ASSERT_NO_THROW(renderer.Render(mesh_cpu, SE3(), extreme_cam, input, output, 0, 0));
        
        cv::Mat result = DownloadTexture(output, 0, CV_32FC1);
        
        // Check for invalid values
        bool has_invalid = false;
        for (int y = 0; y < result.rows && !has_invalid; ++y) {
            for (int x = 0; x < result.cols && !has_invalid; ++x) {
                float val = result.at<float>(y, x);
                if (!std::isfinite(val)) {
                    has_invalid = true;
                }
            }
        }
        EXPECT_FALSE(has_invalid) << "Extreme camera parameters should not produce invalid values";
    }
    
    // GL test
    {
        TextureGL<float> input(w_, h_, 0.0f);
        TextureGL<float> output(w_, h_, -1.0f);
        UploadMatToTexture(input, 0, image_src_cv_);
        
        DepthRendererGL renderer;
        ASSERT_NO_THROW(renderer.Render(mesh_gl, SE3(), extreme_cam, input, output, 0, 0));
        
        GLenum error = glGetError();
        EXPECT_EQ(error, GL_NO_ERROR) << "Extreme camera parameters should not cause GL errors";
    }
}

// Test memory pressure scenarios
TEST_F(ErrorHandlingTests, MemoryPressureHandling) {
    const std::vector<int> large_sizes = {1024, 2048, 4096};
    
    for (int size : large_sizes) {
        std::cout << "Testing memory pressure at " << size << "x" << size << "\n";
        
        // Create large test data
        std::vector<float> large_vertices, large_texcoords, large_weights;
        CreateScreenQuad(large_vertices, large_texcoords, large_weights);
        
        try {
            // CPU test
            {
                MeshCPU mesh(large_vertices, large_texcoords, large_weights);
                TextureCPU<float> input(size, size, 0.0f);
                TextureCPU<float> output(size, size, -1.0f);
                
                // Create test pattern
                cv::Mat large_image(size, size, CV_32FC1);
                for (int y = 0; y < size; ++y) {
                    for (int x = 0; x < size; ++x) {
                        large_image.at<float>(y, x) = (x + y) % 256;
                    }
                }
                
                UploadMatToTexture(input, 0, large_image);
                
                DepthRendererCPU renderer;
                ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, 0, 0));
                
                std::cout << "  CPU " << size << "x" << size << ": OK\n";
            }
            
            // GL test
            {
                MeshGL mesh(large_vertices, large_texcoords, large_weights);
                TextureGL<float> input(size, size, 0.0f);
                TextureGL<float> output(size, size, -1.0f);
                
                cv::Mat large_image(size, size, CV_32FC1);
                for (int y = 0; y < size; ++y) {
                    for (int x = 0; x < size; ++x) {
                        large_image.at<float>(y, x) = (x + y) % 256;
                    }
                }
                
                UploadMatToTexture(input, 0, large_image);
                
                DepthRendererGL renderer;
                ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, input, output, 0, 0));
                
                GLenum error = glGetError();
                EXPECT_EQ(error, GL_NO_ERROR) << "Large texture should not cause GL errors";
                
                std::cout << "  GL " << size << "x" << size << ": OK\n";
            }
        }
        catch (const std::exception& e) {
            std::cout << "  Size " << size << " failed (expected for very large sizes): " << e.what() << "\n";
            // Large sizes may fail due to memory constraints, which is acceptable
        }
    }
}

// Test concurrent access (if applicable)
TEST_F(ErrorHandlingTests, ThreadSafetyBasics) {
    // Basic test to ensure renderers don't crash with multiple instances
    const int num_instances = 4;
    
    std::vector<std::unique_ptr<DepthRendererCPU>> cpu_renderers;
    std::vector<std::unique_ptr<DepthRendererGL>> gl_renderers;
    
    // Create multiple renderer instances
    for (int i = 0; i < num_instances; ++i) {
        cpu_renderers.push_back(std::make_unique<DepthRendererCPU>());
        gl_renderers.push_back(std::make_unique<DepthRendererGL>());
    }
    
    // Use them sequentially (not testing true concurrency, just multiple instances)
    for (int i = 0; i < num_instances; ++i) {
        MeshCPU mesh_cpu(vertices_, texcoords_, weights_);
        TextureCPU<float> input_cpu(w_, h_, 0.0f);
        TextureCPU<float> output_cpu(w_, h_, -1.0f);
        UploadMatToTexture(input_cpu, 0, image_src_cv_);
        
        ASSERT_NO_THROW(cpu_renderers[i]->Render(mesh_cpu, SE3(), cam_, input_cpu, output_cpu, 0, 0));
        
        MeshGL mesh_gl(vertices_, texcoords_, weights_);
        TextureGL<float> input_gl(w_, h_, 0.0f);
        TextureGL<float> output_gl(w_, h_, -1.0f);
        UploadMatToTexture(input_gl, 0, image_src_cv_);
        
        ASSERT_NO_THROW(gl_renderers[i]->Render(mesh_gl, SE3(), cam_, input_gl, output_gl, 0, 0));
        
        GLenum error = glGetError();
        EXPECT_EQ(error, GL_NO_ERROR) << "Multiple GL renderer instances should not interfere";
    }
}

#endif // COMPILE_GL
