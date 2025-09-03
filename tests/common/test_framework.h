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
#include "core/common.h"
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
        // dataset_ = std::make_unique<LoadDatasetIclNuim>(std::string(TEST_DATA_DIR));
        // dataset_ = std::make_unique<LoadDesktopDataset>(std::string(TEST_DATA_DIR));
        dataset_ = std::make_unique<LoadDatasetTumRgbd>(std::string(TEST_DATA_DIR));

        image_files_ = dataset_->GetImageFiles();
        depth_files_ = dataset_->GetDepthFiles();
        poses_ = dataset_->GetPoses();
        cam_ = dataset_->GetCamera();
        w_ = dataset_->GetWidth();
        h_ = dataset_->GetHeight();
        depth_factor_ = dataset_->GetDepthFactor();
    }

    void TearDown() override
    {
        // Cleanup
        dataset_.reset();
    }

    cv::Mat ReadMat(const std::string &filename)
    {
        cv::Mat image = cv::imread(filename, cv::IMREAD_GRAYSCALE);
        image.convertTo(image, CV_32FC1);
        return image;
    }

    // Helper functions for texture operations
    template <typename Texture>
    void UploadMatToTexture(Texture &tex, int lvl, const cv::Mat &mat)
    {
        assert(tex.width(lvl) == mat.cols && tex.height(lvl) == mat.rows);
        auto mapped = tex.MapWrite(lvl);
        std::memcpy(mapped.data(), mat.ptr(), mat.total() * tex.type_size());
        tex.generate_mipmaps(lvl);
    }

    template <typename Texture>
    cv::Mat DownloadTexture(const Texture &tex, int lvl, int cv_type)
    {
        cv::Mat result(tex.height(lvl), tex.width(lvl), cv_type);
        auto mapped = tex.MapRead(lvl);
        std::memcpy(result.ptr(), mapped.data(), tex.height(lvl) * tex.width(lvl) * tex.type_size());
        return result;
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

    // Error computation
    template <typename Texture>
    double RMSE(const Texture &tex1, const Texture &tex2, int lvl)
    {
        EXPECT_EQ(tex1.size(lvl), tex2.size(lvl));

        double total_error = 0.0;
        int valid_pixels = 0;

        auto tx1_map = tex1.MapRead(lvl);
        auto tx2_map = tex2.MapRead(lvl);

        for (int i = 0; i < tex1.size(lvl); ++i)
        {
            auto val1 = tx1_map[i];
            auto val2 = tx2_map[i];

            if (val1 == tex1.nodata() || val2 == tex2.nodata())
            {
                continue;
            }

            auto diff = val1 - val2;
            total_error += diff * diff;

            valid_pixels++;
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
    // std::unique_ptr<LoadDatasetIclNuim> dataset_;
    // std::unique_ptr<LoadDesktopDataset> dataset_;
    std::unique_ptr<LoadDatasetTumRgbd> dataset_;

    std::vector<std::string> image_files_, depth_files_;
    std::vector<SE3> poses_;
    Camera cam_;
    int w_, h_;
    float depth_factor_;

    // Test frames
    // cv::Mat image_src_cv_, depth_src_cv_, image_dst_cv_, depth_dst_cv_;
    // SE3 pose_src_, pose_dst_;

    // Mesh data
    // std::vector<float> vertices_, texcoords_, weights_;
    // std::vector<int> indices_;

    // Performance timer
    PerformanceTimer timer_;
};

class TwoViewTests : public RendererTestBase
{
protected:
    void SetUp() override
    {
        RendererTestBase::SetUp();

        float scale = 1.0f / depth_factor_;

        image_src_cv_ = ReadMat(image_files_[0]);
        depth_src_cv_ = ReadMat(depth_files_[0]) * scale;
        pose_src_ = poses_[0];

        image_dst_cv_ = ReadMat(image_files_[50]);
        depth_dst_cv_ = ReadMat(depth_files_[50]) * scale;
        pose_dst_ = poses_[50];

        TextureCPU<float> depth_src_cpu(w_, h_, 0.0f);
        UploadMatToTexture(depth_src_cpu, 0, depth_src_cv_);

        CreateMesh(depth_src_cpu, cam_, 32, vertices_, texcoords_, weights_, indices_);

        CreateScreenQuad(screen_vertices_, screen_texcoords_, screen_weights_, screen_indices_);
    }

    cv::Mat image_src_cv_, depth_src_cv_, image_dst_cv_, depth_dst_cv_;

    SE3 pose_src_, pose_dst_;

    std::vector<float> vertices_, texcoords_, weights_;
    std::vector<unsigned int> indices_;

    std::vector<float> screen_vertices_, screen_texcoords_, screen_weights_;
    std::vector<unsigned int> screen_indices_;
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
    double gt_max_depth_error = 0.4;
    double gt_max_image_error = 10.0;

    double cr_max_depth_error = 9e-5;
    double cr_max_image_error = 3.0;
    double cr_max_residual_error = 3.5;
    double cr_max_l2_error = 250.0;
    double cr_max_didxy_error = 0.5;
    double cr_max_jtra_error = 0.005;
    double cr_max_jrot_error = 0.002;
    double cr_max_r_error = 4.0;
    double cr_max_jmap_error = 0.3;
    double cr_max_pids_error = 0.4;
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

    // double max_l2_error = 1.0;
    // double max_cross_backend_error = 0.5;
    // double max_execution_time_ms = 1000.0;
    // double max_performance_ratio = 2.0; // CPU vs GL performance ratio
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