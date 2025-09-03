#include <gtest/gtest.h>
#include <benchmark/benchmark.h>
#include <opencv2/opencv.hpp>

#include "../common/test_framework.h"

// GL context setup for benchmarks
class BenchmarkEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        InitEGL();
    }
    void TearDown() override {}
};

static ::testing::Environment* const benchmark_env = 
    ::testing::AddGlobalTestEnvironment(new BenchmarkEnvironment());

class RendererBenchmark : public RendererTestBase {
protected:
    void SetUp() override {
        RendererTestBase::SetUp();
    }
};

// Benchmark CPU Depth Renderer
TEST_F(RendererBenchmark, BenchmarkDepthRendererCPU) {
    const int iterations = 10;
    const int in_lvl = 0, out_lvl = 0;
    
    MeshCPU mesh(vertices_, texcoords_, weights_, indices_);
    TextureCPU<float> input(w_, h_, 0.0f);
    TextureCPU<float> output(w_, h_, 0.0f);
    UploadMatToTextureCPU(input, 0, image_src_cv_);
    
    DepthRendererCPU renderer;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    std::vector<double> times;
    times.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        timer_.Start();
        renderer.Render(mesh, pose_transform, cam_, input, output, in_lvl, out_lvl);
        double time_ms = timer_.Stop();
        times.push_back(time_ms);
    }
    
    // Calculate statistics
    double total_time = 0.0;
    for (double t : times) total_time += t;
    double avg_time = total_time / iterations;
    
    std::sort(times.begin(), times.end());
    double median_time = times[iterations / 2];
    double min_time = times[0];
    double max_time = times[iterations - 1];
    
    std::cout << "CPU Depth Renderer Performance:\n";
    std::cout << "  Average: " << avg_time << " ms\n";
    std::cout << "  Median:  " << median_time << " ms\n";
    std::cout << "  Min:     " << min_time << " ms\n";
    std::cout << "  Max:     " << max_time << " ms\n";
    
    // Performance assertions
    EXPECT_LT(avg_time, 100.0) << "CPU depth rendering too slow";
}

// Benchmark GL Depth Renderer
TEST_F(RendererBenchmark, BenchmarkDepthRendererGL) {
    const int iterations = 10;
    const int in_lvl = 0, out_lvl = 0;
    
    MeshGL mesh(vertices_, texcoords_, weights_, indices_);
    TextureGL<float> input(w_, h_, 0.0f);
    TextureGL<float> output(w_, h_, 0.0f);
    UploadMatToTextureGL(input, 0, image_src_cv_);
    
    DepthRendererGL renderer;
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    std::vector<double> times;
    times.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        timer_.Start();
        renderer.Render(mesh, pose_transform, cam_, input, output, in_lvl, out_lvl);
        glFinish(); // Ensure GPU work is complete
        double time_ms = timer_.Stop();
        times.push_back(time_ms);
    }
    
    // Calculate statistics
    double total_time = 0.0;
    for (double t : times) total_time += t;
    double avg_time = total_time / iterations;
    
    std::sort(times.begin(), times.end());
    double median_time = times[iterations / 2];
    double min_time = times[0];
    double max_time = times[iterations - 1];
    
    std::cout << "GL Depth Renderer Performance:\n";
    std::cout << "  Average: " << avg_time << " ms\n";
    std::cout << "  Median:  " << median_time << " ms\n";
    std::cout << "  Min:     " << min_time << " ms\n";
    std::cout << "  Max:     " << max_time << " ms\n";
    
    // Performance assertions
    EXPECT_LT(avg_time, 50.0) << "GL depth rendering too slow";
}

// Comparative benchmark CPU vs GL
TEST_F(RendererBenchmark, ComparativePerformanceCPUvsGL) {
    const int iterations = 5;
    const int in_lvl = 0, out_lvl = 0;
    
    // CPU setup
    MeshCPU mesh_cpu(vertices_, texcoords_, weights_, indices_);
    TextureCPU<float> input_cpu(w_, h_, 0.0f);
    TextureCPU<float> output_cpu(w_, h_, 0.0f);
    UploadMatToTextureCPU(input_cpu, 0, image_src_cv_);
    DepthRendererCPU renderer_cpu;
    
    // GL setup
    MeshGL mesh_gl(vertices_, texcoords_, weights_);
    TextureGL<float> input_gl(w_, h_, 0.0f);
    TextureGL<float> output_gl(w_, h_, 0.0f);
    UploadMatToTextureGL(input_gl, 0, image_src_cv_);
    DepthRendererGL renderer_gl;
    
    SE3 pose_transform = pose_dst_ * pose_src_.inverse();
    
    // Benchmark CPU
    double cpu_total_time = 0.0;
    for (int i = 0; i < iterations; ++i) {
        timer_.Start();
        renderer_cpu.Render(mesh_cpu, pose_transform, cam_, input_cpu, output_cpu, in_lvl, out_lvl);
        cpu_total_time += timer_.Stop();
    }
    double cpu_avg_time = cpu_total_time / iterations;
    
    // Benchmark GL
    double gl_total_time = 0.0;
    for (int i = 0; i < iterations; ++i) {
        timer_.Start();
        renderer_gl.Render(mesh_gl, pose_transform, cam_, input_gl, output_gl, in_lvl, out_lvl);
        glFinish();
        gl_total_time += timer_.Stop();
    }
    double gl_avg_time = gl_total_time / iterations;
    
    double speedup = cpu_avg_time / gl_avg_time;
    
    std::cout << "Performance Comparison:\n";
    std::cout << "  CPU Average: " << cpu_avg_time << " ms\n";
    std::cout << "  GL Average:  " << gl_avg_time << " ms\n";
    std::cout << "  Speedup:     " << speedup << "x\n";
    
    // Validate results are similar
    cv::Mat cpu_result = DownloadTextureFromCPU(output_cpu, out_lvl, w_, h_, CV_32FC1);
    cv::Mat gl_result = DownloadTextureFromGL(output_gl, out_lvl, w_, h_, CV_32FC1);
    
    ValidationThresholds thresholds;
    TestValidator::ValidateCrossBackend(cpu_result, gl_result, thresholds);
    TestValidator::ValidatePerformance(cpu_avg_time, gl_avg_time, thresholds);
    
    EXPECT_GT(speedup, 1.0) << "GL should be faster than CPU";
}

// Memory usage benchmark
TEST_F(RendererBenchmark, MemoryUsageBenchmark) {
    const std::vector<int> resolutions = {256, 512, 1024, 2048};
    
    for (int res : resolutions) {
        std::cout << "Testing resolution: " << res << "x" << res << "\n";
        
        // CPU memory usage
        timer_.Start();
        {
            MeshCPU mesh_cpu(vertices_, texcoords_, weights_, indices_);
            TextureCPU<float> input_cpu(res, res, 0.0f);
            TextureCPU<float> output_cpu(res, res, 0.0f);
            
            DepthRendererCPU renderer_cpu;
            renderer_cpu.Render(mesh_cpu, SE3(), cam_, input_cpu, output_cpu, 0, 0);
        }
        double cpu_time = timer_.Stop();
        
        // GL memory usage
        timer_.Start();
        {
            MeshGL mesh_gl(vertices_, texcoords_, weights_, indices_);
            TextureGL<float> input_gl(res, res, 0.0f);
            TextureGL<float> output_gl(res, res, 0.0f);
            
            DepthRendererGL renderer_gl;
            renderer_gl.Render(mesh_gl, SE3(), cam_, input_gl, output_gl, 0, 0);
            glFinish();
        }
        double gl_time = timer_.Stop();
        
        std::cout << "  CPU: " << cpu_time << " ms, GL: " << gl_time << " ms\n";
        
        // Memory should scale reasonably with resolution
        EXPECT_LT(cpu_time, res * 0.1) << "CPU memory scaling issue";
        EXPECT_LT(gl_time, res * 0.05) << "GL memory scaling issue";
    }
}
