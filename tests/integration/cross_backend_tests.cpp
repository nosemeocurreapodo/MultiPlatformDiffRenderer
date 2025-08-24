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

class CrossBackendTests : public RendererTestBase {
protected:
    ValidationThresholds thresholds_;
    
    void SetUp() override {
        RendererTestBase::SetUp();
        
        // Set cross-backend validation thresholds
        thresholds_.max_l2_error = 1.5;
        thresholds_.max_cross_backend_error = 0.5;
        thresholds_.max_execution_time_ms = 500.0;
        thresholds_.max_performance_ratio = 5.0;
    }
};

// Compare CPU vs GL depth rendering
TEST_F(CrossBackendTests, DepthRenderingComparison) {
    const int in_lvl = 0, out_lvl = 0;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    // CPU implementation
    MeshCPU mesh_cpu(vertices_, texcoords_, weights_);
    TextureCPU<float> input_cpu(w_, h_, 0.0f);
    TextureCPU<float> output_cpu(w_, h_, 0.0f);
    UploadMatToTextureCPU(input_cpu, 0, image_src_cv_);
    
    DepthRendererCPU renderer_cpu;
    timer_.Start();
    renderer_cpu.Render(mesh_cpu, pose_transform, cam_, input_cpu, output_cpu, in_lvl, out_lvl);
    double cpu_time = timer_.Stop();
    
    cv::Mat cpu_result = DownloadTextureFromCPU(output_cpu, out_lvl, w_, h_, CV_32FC1);
    
    // GL implementation
    MeshGL mesh_gl(vertices_, texcoords_, weights_);
    TextureGL<float> input_gl(w_, h_, 0.0f);
    TextureGL<float> output_gl(w_, h_, 0.0f);
    UploadMatToTextureGL(input_gl, 0, image_src_cv_);
    
    DepthRendererGL renderer_gl;
    timer_.Start();
    renderer_gl.Render(mesh_gl, pose_transform, cam_, input_gl, output_gl, in_lvl, out_lvl);
    double gl_time = timer_.Stop();
    
    cv::Mat gl_result = DownloadTextureFromGL(output_gl, out_lvl, w_, h_, CV_32FC1);
    
    // Cross-backend validation
    TestValidator::ValidateCrossBackend(cpu_result, gl_result, thresholds_);
    TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    
    // Detailed error analysis
    double l2_error = ComputeL2Error<float>(cpu_result, gl_result, 0.0f);
    
    // Save comparison images
    SaveDebugImage(cpu_result, "cross_depth_cpu.png");
    SaveDebugImage(gl_result, "cross_depth_gl.png");
    
    // Create difference image
    cv::Mat diff_image;
    cv::absdiff(cpu_result, gl_result, diff_image);
    SaveDebugImage(diff_image, "cross_depth_diff.png");
    
    std::cout << "Depth Rendering Cross-Backend Comparison:\n";
    std::cout << "  L2 Error: " << l2_error << "\n";
    std::cout << "  CPU Time: " << cpu_time << " ms\n";
    std::cout << "  GL Time:  " << gl_time << " ms\n";
    std::cout << "  Speedup:  " << (cpu_time / gl_time) << "x\n";
    
    EXPECT_LT(l2_error, thresholds_.max_cross_backend_error);
}

// Compare CPU vs GL image rendering
TEST_F(CrossBackendTests, ImageRenderingComparison) {
    const int in_lvl = 0, out_lvl = 0;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    // CPU implementation
    MeshCPU mesh_cpu(vertices_, texcoords_, weights_);
    TextureCPU<float> input_cpu(w_, h_, 0.0f);
    TextureCPU<float> output_cpu(w_, h_, 0.0f);
    UploadMatToTextureCPU(input_cpu, 0, image_src_cv_);
    
    ImageRendererCPU renderer_cpu;
    timer_.Start();
    renderer_cpu.Render(mesh_cpu, pose_transform, cam_, input_cpu, output_cpu, in_lvl, out_lvl);
    double cpu_time = timer_.Stop();
    
    cv::Mat cpu_result = DownloadTextureFromCPU(output_cpu, out_lvl, w_, h_, CV_32FC1);
    
    // GL implementation
    MeshGL mesh_gl(vertices_, texcoords_, weights_);
    TextureGL<float> input_gl(w_, h_, 0.0f);
    TextureGL<float> output_gl(w_, h_, 0.0f);
    UploadMatToTextureGL(input_gl, 0, image_src_cv_);
    
    ImageRendererGL renderer_gl;
    timer_.Start();
    renderer_gl.Render(mesh_gl, pose_transform, cam_, input_gl, output_gl, in_lvl, out_lvl);
    double gl_time = timer_.Stop();
    
    cv::Mat gl_result = DownloadTextureFromGL(output_gl, out_lvl, w_, h_, CV_32FC1);
    
    // Cross-backend validation
    TestValidator::ValidateCrossBackend(cpu_result, gl_result, thresholds_);
    TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    
    double l2_error = ComputeL2Error<float>(cpu_result, gl_result, 0.0f);
    
    SaveDebugImage(cpu_result, "cross_image_cpu.png");
    SaveDebugImage(gl_result, "cross_image_gl.png");
    
    cv::Mat diff_image;
    cv::absdiff(cpu_result, gl_result, diff_image);
    SaveDebugImage(diff_image, "cross_image_diff.png");
    
    std::cout << "Image Rendering Cross-Backend Comparison:\n";
    std::cout << "  L2 Error: " << l2_error << "\n";
    std::cout << "  CPU Time: " << cpu_time << " ms\n";
    std::cout << "  GL Time:  " << gl_time << " ms\n";
    std::cout << "  Speedup:  " << (cpu_time / gl_time) << "x\n";
}

// Compare CPU vs GL gradient computation
TEST_F(CrossBackendTests, GradientComputationComparison) {
    const int in_lvl = 0, out_lvl = 0;
    
    std::vector<float> quad_pos, quad_uv, quad_weights;
    CreateScreenQuad(quad_pos, quad_uv, quad_weights);
    
    // CPU implementation
    MeshCPU mesh_cpu(quad_pos, quad_uv, quad_weights);
    TextureCPU<float> input_cpu(w_, h_, 0.0f);
    TextureCPU<Vec3> output_cpu(w_, h_, Vec3(0.0f, 0.0f, 0.0f));
    UploadMatToTextureCPU(input_cpu, 0, image_src_cv_);
    
    DIDxyRendererCPU renderer_cpu;
    timer_.Start();
    renderer_cpu.Render(mesh_cpu, SE3(), cam_, input_cpu, output_cpu, in_lvl, out_lvl);
    double cpu_time = timer_.Stop();
    
    cv::Mat cpu_result = DownloadTextureFromCPU(output_cpu, out_lvl, w_, h_, CV_32FC3);
    
    // GL implementation
    MeshGL mesh_gl(quad_pos, quad_uv, quad_weights);
    TextureGL<float> input_gl(w_, h_, 0.0f);
    TextureGL<Vec3> output_gl(w_, h_, Vec3(0.0f, 0.0f, 0.0f));
    UploadMatToTextureGL(input_gl, 0, image_src_cv_);
    
    DIDxyRendererGL renderer_gl;
    timer_.Start();
    renderer_gl.Render(mesh_gl, SE3(), cam_, input_gl, output_gl, in_lvl, out_lvl);
    double gl_time = timer_.Stop();
    
    cv::Mat gl_result = DownloadTextureFromGL(output_gl, out_lvl, w_, h_, CV_32FC3);
    
    // Cross-backend validation for Vec3 data
    double l2_error = ComputeL2Error<cv::Vec3f>(cpu_result, gl_result, cv::Vec3f(0.0f, 0.0f, 0.0f));
    EXPECT_LT(l2_error, thresholds_.max_cross_backend_error);
    
    TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    
    SaveDebugImageColor(cpu_result, "cross_gradient_cpu.png");
    SaveDebugImageColor(gl_result, "cross_gradient_gl.png");
    
    cv::Mat diff_image;
    cv::absdiff(cpu_result, gl_result, diff_image);
    SaveDebugImageColor(diff_image, "cross_gradient_diff.png");
    
    std::cout << "Gradient Computation Cross-Backend Comparison:\n";
    std::cout << "  L2 Error: " << l2_error << "\n";
    std::cout << "  CPU Time: " << cpu_time << " ms\n";
    std::cout << "  GL Time:  " << gl_time << " ms\n";
}

// Test full Jacobian pipeline comparison
TEST_F(CrossBackendTests, JacobianPipelineComparison) {
    const int in_lvl = 0, out_lvl = 0;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    std::vector<float> quad_pos, quad_uv, quad_weights;
    CreateScreenQuad(quad_pos, quad_uv, quad_weights);
    
    // CPU pipeline
    MeshCPU mesh_img_cpu(quad_pos, quad_uv, quad_weights);
    MeshCPU mesh_cpu(vertices_, texcoords_, weights_);
    
    TextureCPU<float> input_cpu(w_, h_, 0.0f);
    TextureCPU<Vec3> didxy_cpu(w_, h_, Vec3(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3> jtra_cpu(w_, h_, Vec3(0.0f, 0.0f, 0.0f));
    TextureCPU<Vec3> jrot_cpu(w_, h_, Vec3(0.0f, 0.0f, 0.0f));
    UploadMatToTextureCPU(input_cpu, 0, image_dst_cv_);
    
    DIDxyRendererCPU didxy_renderer_cpu;
    JtraRendererCPU jtra_renderer_cpu;
    JrotRendererCPU jrot_renderer_cpu;
    
    timer_.Start();
    didxy_renderer_cpu.Render(mesh_img_cpu, SE3(), cam_, input_cpu, didxy_cpu, in_lvl, out_lvl);
    jtra_renderer_cpu.Render(mesh_cpu, pose_transform, cam_, didxy_cpu, jtra_cpu, out_lvl, out_lvl);
    jrot_renderer_cpu.Render(mesh_cpu, pose_transform, cam_, jtra_cpu, jrot_cpu, out_lvl, out_lvl);
    double cpu_time = timer_.Stop();
    
    cv::Mat cpu_jtra = DownloadTextureFromCPU(jtra_cpu, out_lvl, w_, h_, CV_32FC3);
    cv::Mat cpu_jrot = DownloadTextureFromCPU(jrot_cpu, out_lvl, w_, h_, CV_32FC3);
    
    // GL pipeline
    MeshGL mesh_img_gl(quad_pos, quad_uv, quad_weights);
    MeshGL mesh_gl(vertices_, texcoords_, weights_);
    
    TextureGL<float> input_gl(w_, h_, 0.0f);
    TextureGL<Vec3> didxy_gl(w_, h_, Vec3(0.0f, 0.0f, 0.0f));
    TextureGL<Vec3> jtra_gl(w_, h_, Vec3(0.0f, 0.0f, 0.0f));
    TextureGL<Vec3> jrot_gl(w_, h_, Vec3(0.0f, 0.0f, 0.0f));
    UploadMatToTextureGL(input_gl, 0, image_dst_cv_);
    
    DIDxyRendererGL didxy_renderer_gl;
    JtraRendererGL jtra_renderer_gl;
    JrotRendererGL jrot_renderer_gl;
    
    timer_.Start();
    didxy_renderer_gl.Render(mesh_img_gl, SE3(), cam_, input_gl, didxy_gl, in_lvl, out_lvl);
    jtra_renderer_gl.Render(mesh_gl, pose_transform, cam_, didxy_gl, jtra_gl, out_lvl, out_lvl);
    jrot_renderer_gl.Render(mesh_gl, pose_transform, cam_, jtra_gl, jrot_gl, out_lvl, out_lvl);
    double gl_time = timer_.Stop();
    
    cv::Mat gl_jtra = DownloadTextureFromGL(jtra_gl, out_lvl, w_, h_, CV_32FC3);
    cv::Mat gl_jrot = DownloadTextureFromGL(jrot_gl, out_lvl, w_, h_, CV_32FC3);
    
    // Validate both Jtra and Jrot
    double jtra_error = ComputeL2Error<cv::Vec3f>(cpu_jtra, gl_jtra, cv::Vec3f(0.0f, 0.0f, 0.0f));
    double jrot_error = ComputeL2Error<cv::Vec3f>(cpu_jrot, gl_jrot, cv::Vec3f(0.0f, 0.0f, 0.0f));
    
    EXPECT_LT(jtra_error, thresholds_.max_cross_backend_error) << "Jtra cross-backend error too high";
    EXPECT_LT(jrot_error, thresholds_.max_cross_backend_error) << "Jrot cross-backend error too high";
    
    TestValidator::ValidatePerformance(cpu_time, gl_time, thresholds_);
    
    SaveDebugImageColor(cpu_jtra, "cross_jtra_cpu.png");
    SaveDebugImageColor(gl_jtra, "cross_jtra_gl.png");
    SaveDebugImageColor(cpu_jrot, "cross_jrot_cpu.png");
    SaveDebugImageColor(gl_jrot, "cross_jrot_gl.png");
    
    std::cout << "Jacobian Pipeline Cross-Backend Comparison:\n";
    std::cout << "  Jtra L2 Error: " << jtra_error << "\n";
    std::cout << "  Jrot L2 Error: " << jrot_error << "\n";
    std::cout << "  CPU Time: " << cpu_time << " ms\n";
    std::cout << "  GL Time:  " << gl_time << " ms\n";
    std::cout << "  Speedup:  " << (cpu_time / gl_time) << "x\n";
}

// Test different mesh complexities
TEST_F(CrossBackendTests, MeshComplexityComparison) {
    const std::vector<int> grid_sizes = {8, 16, 32, 64};
    
    for (int grid_size : grid_sizes) {
        std::cout << "Testing mesh complexity: " << grid_size << "x" << grid_size << "\n";
        
        // Create mesh with different complexity
        std::vector<Vec2> grid_uv = UniformTexCoords(grid_size, grid_size);
        std::vector<float> test_vertices, test_texcoords, test_weights;
        
        test_vertices.reserve(grid_uv.size() * 3);
        test_texcoords.reserve(grid_uv.size() * 2);
        test_weights.reserve(grid_uv.size());
        
        for (const Vec2& uv : grid_uv) {
            const float ix = uv(0) * (w_ - 1);
            const float iy = uv(1) * (h_ - 1);
            const int x = static_cast<int>(ix);
            const int y = static_cast<int>(iy);
            const float depth = depth_src_cv_.at<float>(y, x);
            
            if (depth <= 0.0f) continue;
            
            const Vec3 ray = cam_.PixToRay(uv);
            const Vec3 vertex = ray * depth;
            
            test_vertices.push_back(vertex(0));
            test_vertices.push_back(vertex(1));
            test_vertices.push_back(vertex(2));
            test_texcoords.push_back(uv(0));
            test_texcoords.push_back(uv(1));
            test_weights.push_back(1.0f);
        }
        
        if (test_vertices.empty()) continue;
        
        // Test CPU
        MeshCPU mesh_cpu(test_vertices, test_texcoords, test_weights);
        TextureCPU<float> input_cpu(w_, h_, 0.0f);
        TextureCPU<float> output_cpu(w_, h_, 0.0f);
        UploadMatToTextureCPU(input_cpu, 0, image_src_cv_);
        
        DepthRendererCPU renderer_cpu;
        timer_.Start();
        renderer_cpu.Render(mesh_cpu, SE3(), cam_, input_cpu, output_cpu, 0, 0);
        double cpu_time = timer_.Stop();
        
        // Test GL
        MeshGL mesh_gl(test_vertices, test_texcoords, test_weights);
        TextureGL<float> input_gl(w_, h_, 0.0f);
        TextureGL<float> output_gl(w_, h_, 0.0f);
        UploadMatToTextureGL(input_gl, 0, image_src_cv_);
        
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

// Test numerical precision comparison
TEST_F(CrossBackendTests, NumericalPrecisionComparison) {
    const int in_lvl = 0, out_lvl = 0;
    const int iterations = 3;
    
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    std::vector<cv::Mat> cpu_results, gl_results;
    cpu_results.reserve(iterations);
    gl_results.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        // CPU
        MeshCPU mesh_cpu(vertices_, texcoords_, weights_);
        TextureCPU<float> input_cpu(w_, h_, 0.0f);
        TextureCPU<float> output_cpu(w_, h_, 0.0f);
        UploadMatToTextureCPU(input_cpu, 0, image_src_cv_);
        
        DepthRendererCPU renderer_cpu;
        renderer_cpu.Render(mesh_cpu, pose_transform, cam_, input_cpu, output_cpu, in_lvl, out_lvl);
        cpu_results.push_back(DownloadTextureFromCPU(output_cpu, out_lvl, w_, h_, CV_32FC1));
        
        // GL
        MeshGL mesh_gl(vertices_, texcoords_, weights_);
        TextureGL<float> input_gl(w_, h_, 0.0f);
        TextureGL<float> output_gl(w_, h_, 0.0f);
        UploadMatToTextureGL(input_gl, 0, image_src_cv_);
        
        DepthRendererGL renderer_gl;
        renderer_gl.Render(mesh_gl, pose_transform, cam_, input_gl, output_gl, in_lvl, out_lvl);
        gl_results.push_back(DownloadTextureFromGL(output_gl, out_lvl, w_, h_, CV_32FC1));
    }
    
    // Check CPU consistency
    for (int i = 1; i < iterations; ++i) {
        double cpu_error = ComputeL2Error<float>(cpu_results[0], cpu_results[i], 0.0f);
        EXPECT_LT(cpu_error, 1e-5) << "CPU results not deterministic, iteration " << i;
    }
    
    // Check GL consistency
    for (int i = 1; i < iterations; ++i) {
        double gl_error = ComputeL2Error<float>(gl_results[0], gl_results[i], 0.0f);
        EXPECT_LT(gl_error, 1e-4) << "GL results not sufficiently consistent, iteration " << i;
    }
    
    // Check cross-backend consistency
    double cross_error = ComputeL2Error<float>(cpu_results[0], gl_results[0], 0.0f);
    EXPECT_LT(cross_error, thresholds_.max_cross_backend_error) 
        << "Cross-backend precision error: " << cross_error;
    
    std::cout << "Numerical Precision Comparison:\n";
    std::cout << "  CPU Consistency: < 1e-5\n";
    std::cout << "  GL Consistency:  < 1e-4\n";
    std::cout << "  Cross-Backend:   " << cross_error << "\n";
}

#endif // COMPILE_GL
