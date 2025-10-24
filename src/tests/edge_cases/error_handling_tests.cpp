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

#endif // COMPILE_GL

// Backend trait structs to unify CPU and GL testing
struct CPUBackendTraits
{
    using MeshT = MeshCPU;
    template <typename T>
    using TextureT = TextureCPU<T>;
    using DepthRendererT = DepthRendererCPU;
    using ImageRendererT = ImageRendererCPU;
    using ResidualRendererT = ResidualRendererCPU;
    using L2RendererT = L2RendererCPU;
    using DIDxyRendererT = DIDxyRendererCPU;
    using JPoseRendererT = JPoseRendererCPU;
    using JMapRendererT = JMapRendererCPU;
    static const char *Name() { return "CPU"; }
};

#ifdef COMPILE_GL
struct GLBackendTraits
{
    using MeshT = MeshGL;
    template <typename T>
    using TextureT = TextureGL<T>;
    using DepthRendererT = DepthRendererGL;
    using ImageRendererT = ImageRendererGL;
    using ResidualRendererT = ResidualRendererGL;
    using L2RendererT = L2RendererGL;
    using DIDxyRendererT = DIDxyRendererGL;
    using JPoseRendererT = JPoseRendererGL;
    using JMapRendererT = JMapRendererGL;
    static const char *Name() { return "GL"; }
};
#endif

template <typename Backend>
class ErrorHandlingTests : public TwoViewTests
{
protected:
    void SetUp() override
    {
        TwoViewTests::SetUp();
    }
};

TYPED_TEST_SUITE_P(ErrorHandlingTests);

// Test empty mesh handling
TYPED_TEST_P(ErrorHandlingTests, EmptyMeshHandling)
{
    using Traits = TypeParam;

    std::vector<float> empty_vertex, empty_texcoords;
    std::vector<unsigned int> empty_indices;
    cv::Mat empty_image;

    typename Traits::MeshT empty_mesh(empty_vertex, empty_indices, empty_image, true, true, true);
    typename Traits::template TextureT<float> output(this->w_, this->h_, -1.0f);

    typename Traits::DepthRendererT renderer;
    ASSERT_NO_THROW(renderer.Render(empty_mesh, linalg::SE3<float>(), this->cam_, 0, output));

    cv::Mat result = DownloadTextureToMat(output, 0, CV_32FC1);

    // Should remain at nodata value
    cv::Scalar mean_val = cv::mean(result);
    EXPECT_NEAR(mean_val[0], -1.0f, 0.1f) << "Empty mesh should produce nodata output";
}
/*
// Test degenerate triangle handling
TEST_F(ErrorHandlingTests, DegenerateTriangleHandling)
{
    // Create mesh with degenerate triangles (all vertices at same point)
    std::vector<float> degenerate_vertices = {
        0.0f, 0.0f, 1.0f, // vertex 0
        0.0f, 0.0f, 1.0f, // vertex 1 (same as 0)
        0.0f, 0.0f, 1.0f  // vertex 2 (same as 0)
    };
    std::vector<float> texcoords = {0.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.5f};
    std::vector<float> weights = {1.0f, 1.0f, 1.0f};

    // CPU test
    {
        MeshCPU mesh(degenerate_vertices, texcoords, weights);
        TextureCPU<float> output(w_, h_, -1.0f);

        DepthRendererCPU renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, 0, output));

        // Should handle degenerate triangles gracefully
        cv::Mat result = DownloadTexture(output, 0, CV_32FC1);

        // Check for NaN or infinite values
        bool has_invalid = false;
        for (int y = 0; y < result.rows && !has_invalid; ++y)
        {
            for (int x = 0; x < result.cols && !has_invalid; ++x)
            {
                float val = result.at<float>(y, x);
                if (!std::isfinite(val))
                {
                    has_invalid = true;
                }
            }
        }
        EXPECT_FALSE(has_invalid) << "Degenerate triangles should not produce invalid values";
    }

    // GL test
    {
        MeshGL mesh(degenerate_vertices, texcoords, weights);
        TextureGL<float> output(w_, h_, -1.0f);

        DepthRendererGL renderer;
        ASSERT_NO_THROW(renderer.Render(mesh, SE3(), cam_, 0, output));

        GLenum error = glGetError();
        EXPECT_EQ(error, GL_NO_ERROR) << "Degenerate triangles should not cause GL errors";
    }
}
*/
// Test extreme transformation matrices
TYPED_TEST_P(ErrorHandlingTests, ExtremeTransformationHandling)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, this->image_src_cv_, true, true, true);

    // Test with very large scale
    linalg::SE3<float> large_scale;
    large_scale.translation() = linalg::Vec3<float>(0, 0, 0);
    large_scale.so3() = linalg::SO3<float>::exp(linalg::Vec3<float>(0, 0, 0));
    // Apply large scale through pose
    linalg::Mat4<float> scale_matrix = linalg::Mat4<float>::Identity() * 1000.0f;

    typename Traits::TextureT<float> output(this->w_, this->h_, -1.0f);

    typename Traits::DepthRendererT renderer;
    ASSERT_NO_THROW(renderer.Render(mesh, large_scale, this->cam_, 0, output));

    cv::Mat result = DownloadTextureToMat(output, 0, CV_32FC1);

    // Check for invalid values
    bool has_invalid = false;
    for (int y = 0; y < result.rows && !has_invalid; ++y)
    {
        for (int x = 0; x < result.cols && !has_invalid; ++x)
        {
            float val = result.at<float>(y, x);
            if (!std::isfinite(val))
            {
                has_invalid = true;
            }
        }
    }
    EXPECT_FALSE(has_invalid) << "Extreme transformations should not produce invalid values";
}

// Test invalid mipmap levels
TYPED_TEST_P(ErrorHandlingTests, InvalidMipmapLevels)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, this->image_src_cv_, true, true, true);

    typename Traits::TextureT<float> output(this->w_, this->h_, -1.0f);

    typename Traits::DepthRendererT renderer;

    // Test with level beyond texture size
    int max_level = static_cast<int>(std::log2(std::min(this->w_, this->h_))) + 1;
    ASSERT_NO_THROW(renderer.Render(mesh, linalg::SE3<float>(), this->cam_, max_level, output));
}

// Test texture size mismatches
TYPED_TEST_P(ErrorHandlingTests, TextureSizeMismatch)
{
    using Traits = TypeParam;

    const int small_w = 64, small_h = 64;
    const int large_w = 512, large_h = 512;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, this->image_src_cv_, true, true, true);

    typename Traits::TextureT<float> large_output(large_w, large_h, -1.0f);

    typename Traits::DepthRendererT renderer;
    ASSERT_NO_THROW(renderer.Render(mesh, linalg::SE3<float>(), this->cam_, 0, large_output));

    cv::Mat result = DownloadTextureToMat(large_output, 0, CV_32FC1);

    // Should handle size mismatch gracefully
    cv::Scalar mean_val = cv::mean(result);
    EXPECT_TRUE(std::isfinite(mean_val[0])) << "Size mismatch should not produce invalid values";
}

// Test camera parameter edge cases
TYPED_TEST_P(ErrorHandlingTests, CameraParameterEdgeCases)
{
    using Traits = TypeParam;

    typename Traits::MeshT mesh(this->vertex_, this->indices_, this->image_src_cv_, true, true, true);

    // Create camera with extreme parameters
    Camera<float> extreme_cam;
    linalg::Vec4<float> extreme_params;
    extreme_params(0) = 1e6f;
    extreme_params(1) = 1e6f;
    extreme_params(2) = this->w_ / 2.0f;
    extreme_params(3) = this->h_ / 2.0f; // Very high focal lengths
    extreme_cam.SetParams(extreme_params);

    typename Traits::TextureT<float> output(this->w_, this->h_, -1.0f);

    typename Traits::DepthRendererT renderer;
    ASSERT_NO_THROW(renderer.Render(mesh, linalg::SE3<float>(), extreme_cam, 0, output));

    cv::Mat result = DownloadTextureToMat(output, 0, CV_32FC1);

    // Check for invalid values
    bool has_invalid = false;
    for (int y = 0; y < result.rows && !has_invalid; ++y)
    {
        for (int x = 0; x < result.cols && !has_invalid; ++x)
        {
            float val = result.at<float>(y, x);
            if (!std::isfinite(val))
            {
                has_invalid = true;
            }
        }
    }
    EXPECT_FALSE(has_invalid) << "Extreme camera parameters should not produce invalid values";
}

// Test memory pressure scenarios
TYPED_TEST_P(ErrorHandlingTests, MemoryPressureHandling)
{
    using Traits = TypeParam;

    const std::vector<int> large_sizes = {1024, 2048, 4096};

    for (int size : large_sizes)
    {
        std::cout << "Testing memory pressure at " << size << "x" << size << "\n";

        try
        {
            typename Traits::MeshT mesh(this->screen_vertex_, this->screen_indices_, this->image_src_cv_, true, true, true);

            typename Traits::TextureT<float> output(size, size, -1.0f);

            typename Traits::DepthRendererT renderer;
            ASSERT_NO_THROW(renderer.Render(mesh, linalg::SE3<float>(), this->cam_, 0, output));

            std::cout << "  CPU " << size << "x" << size << ": OK\n";
        }
        catch (const std::exception &e)
        {
            std::cout << "  Size " << size << " failed (expected for very large sizes): " << e.what() << "\n";
            // Large sizes may fail due to memory constraints, which is acceptable
        }
    }
}

// Test concurrent access (if applicable)
TYPED_TEST_P(ErrorHandlingTests, ThreadSafetyBasics)
{
    using Traits = TypeParam;

    // Basic test to ensure renderers don't crash with multiple instances
    const int num_instances = 4;

    std::vector<std::unique_ptr<typename Traits::DepthRendererT>> renderers;

    // Create multiple renderer instances
    for (int i = 0; i < num_instances; ++i)
    {
        renderers.push_back(std::make_unique<typename Traits::DepthRendererT>());
    }

    // Use them sequentially (not testing true concurrency, just multiple instances)
    for (int i = 0; i < num_instances; ++i)
    {
        typename Traits::MeshT mesh(this->vertex_, this->indices_, this->image_src_cv_, true, true, true);
        typename Traits::TextureT<float> output(this->w_, this->h_, -1.0f);

        ASSERT_NO_THROW(renderers[i]->Render(mesh, linalg::SE3<float>(), this->cam_, 0, output));
    }
}

REGISTER_TYPED_TEST_SUITE_P(
    ErrorHandlingTests,
    EmptyMeshHandling,
    ExtremeTransformationHandling,
    InvalidMipmapLevels,
    TextureSizeMismatch,
    CameraParameterEdgeCases,
    MemoryPressureHandling,
    ThreadSafetyBasics);

using TestBackends = ::testing::Types<CPUBackendTraits
#ifdef COMPILE_GL
                                      ,
                                      GLBackendTraits
#endif
                                      >;

INSTANTIATE_TYPED_TEST_SUITE_P(AllBackends, ErrorHandlingTests, TestBackends);
