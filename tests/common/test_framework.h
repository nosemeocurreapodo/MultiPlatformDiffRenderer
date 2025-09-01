#pragma once

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <fstream>
#include <sstream>
#include "../test_config.h"

#include "core/format_converters.h"
#include "core/types.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/meshcpu.h"
#include "backends/cpu/renderercpu.h"
#ifdef COMPILE_GL
#include "backends/gl/devicegl_glad.h"
#include "backends/gl/texturegl.h"
#include "backends/gl/meshgl.h"
#include "backends/gl/renderergl.h"
#endif
#include "loaddataset.h"

// Performance measurement utilities
class PerformanceTimer
{
public:
    void Start()
    {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    double Stop()
    {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
        return duration.count() / 1000.0; // Return milliseconds
    }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
};

// Test result structure for validation
template <typename T>
struct TestResult
{
    cv::Mat output;
    double execution_time_ms;
    std::string backend_name;
    bool success;
    std::string error_message;

    TestResult() : execution_time_ms(0.0), success(false) {}
};

// Base test fixture for all renderer tests
class RendererTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Load test dataset
        dataset_ = std::make_unique<LoadDatasetIclNuim>(std::string(TEST_DATA_DIR));

        image_files_ = dataset_->GetImageFiles();
        depth_files_ = dataset_->GetDepthFiles();
        poses_ = dataset_->GetPoses();
        cam_ = dataset_->GetCamera();
        w_ = dataset_->GetWidth();
        h_ = dataset_->GetHeight();
        depth_factor_ = dataset_->GetDepthFactor();

        // Setup test frames
        SetupTestFrames();

        // Create mesh data
        CreateTestMesh();
    }

    void SetupTestFrames()
    {
        const int src = 0;
        const int dst = 50;

        // Load and preprocess images
        image_src_cv_ = cv::imread(image_files_[src], cv::IMREAD_GRAYSCALE);
        depth_src_cv_ = cv::imread(depth_files_[src], cv::IMREAD_GRAYSCALE);
        image_dst_cv_ = cv::imread(image_files_[dst], cv::IMREAD_GRAYSCALE);
        depth_dst_cv_ = cv::imread(depth_files_[dst], cv::IMREAD_GRAYSCALE);

        ASSERT_FALSE(image_src_cv_.empty()) << "Failed to load source image";
        ASSERT_FALSE(depth_src_cv_.empty()) << "Failed to load source depth";
        ASSERT_FALSE(image_dst_cv_.empty()) << "Failed to load destination image";
        ASSERT_FALSE(depth_dst_cv_.empty()) << "Failed to load destination depth";

        // Apply morphological operations to reduce noise
        const int morph_size = 5;
        cv::Mat element = cv::getStructuringElement(
            cv::MORPH_ELLIPSE,
            cv::Size(2 * morph_size + 1, 2 * morph_size + 1),
            cv::Point(morph_size, morph_size));
        cv::morphologyEx(depth_src_cv_, depth_src_cv_, cv::MORPH_CLOSE, element);
        cv::morphologyEx(depth_dst_cv_, depth_dst_cv_, cv::MORPH_CLOSE, element);

        // Convert to float and scale
        image_src_cv_.convertTo(image_src_cv_, CV_32FC1);
        depth_src_cv_.convertTo(depth_src_cv_, CV_32FC1);
        image_dst_cv_.convertTo(image_dst_cv_, CV_32FC1);
        depth_dst_cv_.convertTo(depth_dst_cv_, CV_32FC1);

        depth_src_cv_ /= depth_factor_;
        depth_src_cv_ *= 100.0f;
        depth_dst_cv_ /= depth_factor_;
        depth_dst_cv_ *= 100.0f;

        pose_src_ = poses_[src].inverse();
        pose_dst_ = poses_[dst].inverse();
    }

    void CreateTestMesh()
    {
        const int grid_size = 32;
        std::vector<Vec2> grid_uv = UniformTexCoords(grid_size, grid_size);

        vertices_.clear();
        texcoords_.clear();
        weights_.clear();

        vertices_.reserve(grid_uv.size() * 3);
        texcoords_.reserve(grid_uv.size() * 2);
        weights_.reserve(grid_uv.size());

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

            vertices_.push_back(vertex(0));
            vertices_.push_back(vertex(1));
            vertices_.push_back(vertex(2));
            texcoords_.push_back(uv(0));
            texcoords_.push_back(uv(1));
            weights_.push_back(1.0f);
        }
    }

    // Helper functions for texture operations
    template <typename Texture>
    void UploadMatToTexture(Texture &tex, int lvl, const cv::Mat &mat)
    {
        assert(tex.width(lvl) == mat.cols && tex.height(lvl) == mat.rows);
        auto mapped = tex.MapWrite(lvl);
        std::memcpy(mapped.data(), mat.ptr(), mat.total() * tex.type_size());
        tex.generate_mipmaps(0);
    }

    template <typename Texture>
    cv::Mat DownloadTexture(const Texture &tex, int lvl, int cv_type)
    {
        cv::Mat result(tex.height(lvl), tex.width(lvl), cv_type);
        auto mapped = tex.MapRead(lvl);
        std::memcpy(result.ptr(), mapped.data(), tex.height(lvl) * tex.width(lvl) * tex.type_size());
        return result;
    }

    // Screen quad for image-space rendering
    void CreateScreenQuad(std::vector<float> &pos, std::vector<float> &uv, std::vector<float> &weights)
    {
        pos = {-1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
               -1.f, 1.f, 1.f, 1.f, -1.f, 1.f, 1.f, 1.f, 1.f};
        uv = {0.f, 1.f, 0.f, 0.f, 1.f, 0.f,
              0.f, 1.f, 1.f, 0.f, 1.f, 1.f};
        weights.assign(6, 1.0f);
    }

    // Error computation
    template <typename T>
    double ComputeL2Error(const cv::Mat &mat1, const cv::Mat &mat2, T nodata_value)
    {
        EXPECT_EQ(mat1.size(), mat2.size());
        EXPECT_EQ(mat1.type(), mat2.type());

        double total_error = 0.0;
        int valid_pixels = 0;

        for (int y = 0; y < mat1.rows; ++y)
        {
            for (int x = 0; x < mat1.cols; ++x)
            {
                const T val1 = mat1.at<T>(y, x);
                const T val2 = mat2.at<T>(y, x);

                if (val1 != nodata_value && val2 != nodata_value)
                {
                    if constexpr (std::is_arithmetic_v<T>)
                    {
                        double diff = static_cast<double>(val1 - val2);
                        total_error += diff * diff;
                    }
                    else
                    {
                        // Handle vector types like cv::Vec3f
                        auto diff = val1 - val2;
                        for (int i = 0; i < diff.channels; ++i)
                        {
                            double d = static_cast<double>(diff[i]);
                            total_error += d * d;
                        }
                    }
                    valid_pixels++;
                }
            }
        }

        return valid_pixels > 0 ? std::sqrt(total_error / valid_pixels) : 0.0;
    }

    // Save debug images
    void SaveDebugImage(const cv::Mat &image, const std::string &filename)
    {
        cv::Mat normalized;
        cv::normalize(image, normalized, 0, 255, cv::NORM_MINMAX);
        normalized.convertTo(normalized, CV_8UC1);
        cv::imwrite(filename, normalized);
    }

    void SaveDebugImageColor(const cv::Mat &image, const std::string &filename)
    {
        cv::Mat normalized;
        cv::normalize(image, normalized, 0, 255, cv::NORM_MINMAX);
        normalized.convertTo(normalized, CV_8UC3);
        cv::imwrite(filename, normalized);
    }

    // Mipmap level dimension calculation
    int GetLevelDim(int base_dim, int level)
    {
        return std::max(1, base_dim >> level);
    }

protected:
    // Dataset and test data
    std::unique_ptr<LoadDatasetIclNuim> dataset_;
    std::vector<std::string> image_files_, depth_files_;
    std::vector<SE3> poses_;
    Camera cam_;
    int w_, h_;
    float depth_factor_;

    // Test frames
    cv::Mat image_src_cv_, depth_src_cv_, image_dst_cv_, depth_dst_cv_;
    SE3 pose_src_, pose_dst_;

    // Mesh data
    std::vector<float> vertices_, texcoords_, weights_;

    // Performance timer
    PerformanceTimer timer_;
};

// Test result reporting utilities
class TestReporter
{
public:
    struct TestResult
    {
        std::string test_name;
        std::string backend;
        double execution_time_ms;
        double memory_usage_mb;
        double error_metric;
        bool passed;
        std::string failure_reason;
    };

    static void RecordResult(const TestResult &result)
    {
        results_.push_back(result);
    }

    static void GenerateReport(const std::string &output_file = "test_report.json")
    {
        std::ofstream file(output_file);
        file << "{\n";
        file << "  \"test_results\": [\n";

        for (size_t i = 0; i < results_.size(); ++i)
        {
            const auto &r = results_[i];
            file << "    {\n";
            file << "      \"test_name\": \"" << r.test_name << "\",\n";
            file << "      \"backend\": \"" << r.backend << "\",\n";
            file << "      \"execution_time_ms\": " << r.execution_time_ms << ",\n";
            file << "      \"memory_usage_mb\": " << r.memory_usage_mb << ",\n";
            file << "      \"error_metric\": " << r.error_metric << ",\n";
            file << "      \"passed\": " << (r.passed ? "true" : "false") << ",\n";
            file << "      \"failure_reason\": \"" << r.failure_reason << "\"\n";
            file << "    }" << (i < results_.size() - 1 ? "," : "") << "\n";
        }

        file << "  ],\n";
        file << "  \"summary\": {\n";
        file << "    \"total_tests\": " << results_.size() << ",\n";
        file << "    \"passed_tests\": " << CountPassed() << ",\n";
        file << "    \"failed_tests\": " << CountFailed() << ",\n";
        file << "    \"pass_rate\": " << (results_.empty() ? 0.0 : (double)CountPassed() / results_.size() * 100.0) << "\n";
        file << "  }\n";
        file << "}\n";
    }

private:
    static std::vector<TestResult> results_;

    static size_t CountPassed()
    {
        return std::count_if(results_.begin(), results_.end(),
                             [](const TestResult &r)
                             { return r.passed; });
    }

    static size_t CountFailed()
    {
        return results_.size() - CountPassed();
    }
};

// Memory usage monitoring
class MemoryMonitor
{
public:
    static size_t GetCurrentMemoryUsage()
    {
        // Simple implementation - in production, use more sophisticated memory tracking
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line))
        {
            if (line.substr(0, 6) == "VmRSS:")
            {
                std::istringstream iss(line);
                std::string key, value, unit;
                iss >> key >> value >> unit;
                return std::stoul(value) / 1024; // Convert KB to MB
            }
        }
        return 0;
    }

    static void CheckMemoryLimits()
    {
        size_t current_mb = GetCurrentMemoryUsage();
        EXPECT_LT(current_mb, TestConfig::MAX_MEMORY_PER_TEST_MB)
            << "Memory usage exceeded limit: " << current_mb << "MB";
    }
};

// Validation thresholds
struct ValidationThresholds
{
    double max_depth_error = 3e-6;
    double max_image_error = 3.0;
    double max_residual_error = 3.0;
    double max_l2_error = 400.0;
    double max_didxy_error = 0.5;
    double max_jtra_error = 0.009;
    double max_jrot_error = 0.002;
    double max_r_error = 3.0;
    double max_jmap_error = 0.5;
    double max_pids_error = 0.4;
    double max_cpu_depth_time_ms = 53.0;
    double max_gl_depth_time_ms = 5.0;
    double max_cpu_image_time_ms = 350.0;
    double max_gl_image_time_ms = 5.0;
    double max_cpu_didxy_time_ms = 700.0;
    double max_gl_didxy_time_ms = 5.0;
    double max_cpu_jtra_time_ms = 1800.0;
    double max_gl_jtra_time_ms = 11.0;
    double max_cpu_jrot_time_ms = 1800.0;
    double max_gl_jrot_time_ms = 11.0;

    //double max_l2_error = 1.0;
    //double max_cross_backend_error = 0.5;
    //double max_execution_time_ms = 1000.0;
    //double max_performance_ratio = 2.0; // CPU vs GL performance ratio
};

/*
// Test result validation
class TestValidator
{
public:
    static void ValidateAgainstGroundTruth(const cv::Mat &result, const cv::Mat &ground_truth,
                                           const ValidationThresholds &thresholds)
    {
        double error = ComputeImageError<float>(result, ground_truth, 0.0f);
        EXPECT_LT(error, thresholds.max_l2_error)
            << "Ground truth validation failed with L2 error: " << error;
    }

    static void ValidateCrossBackend(const cv::Mat &cpu_result, const cv::Mat &gl_result,
                                     const ValidationThresholds &thresholds)
    {
        double error = ComputeImageError<float>(cpu_result, gl_result, 0.0f);
        EXPECT_LT(error, thresholds.max_cross_backend_error)
            << "Cross-backend validation failed with L2 error: " << error;
    }

    static void ValidatePerformance(double cpu_time, double gl_time,
                                    const ValidationThresholds &thresholds)
    {
        EXPECT_LT(cpu_time, thresholds.max_cpu_time_ms)
            << "CPU execution time exceeded threshold: " << cpu_time << "ms";
        EXPECT_LT(gl_time, thresholds.max_gl_time_ms)
            << "GL execution time exceeded threshold: " << gl_time << "ms";

        if (gl_time > 0)
        {
            double ratio = cpu_time / gl_time;
            EXPECT_LT(ratio, thresholds.max_performance_ratio)
                << "Performance ratio CPU/GL exceeded threshold: " << ratio;
        }
    }
};
*/