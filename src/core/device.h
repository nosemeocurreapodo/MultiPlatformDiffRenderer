// multi_platform_renderer.hpp
//
// Skeleton for a multi‑device renderer (CPU, OpenGL, FPGA/SYCL).
//
// Split into logical “files” with markers so you can easily peel them out into
// real headers / sources later.
//
// ├─ include/
// │   └─ renderer/
// │       ├─ handles.hpp
// │       ├─ device.hpp
// │       ├─ pipeline.hpp
// │       ├─ cpu_backend.hpp
// │       ├─ gl_backend.hpp
// │       └─ sycl_backend.hpp
// ├─ src/...
// ├─ CMakeLists.txt
// └─ main.cpp
//
// All code is in namespace `rndr`.  Requires C++20.
//
// To build the FPGA path you need an Intel oneAPI or AMD Vitis tool‑chain and
// pass ‑DENABLE_BACKEND_FPGA=ON.
//
//=============================================================================
//
// handles.hpp
#pragma once
#include <cstdint>
namespace rndr
{
    struct BufferHandle  { std::uint32_t id{}; };
    struct ShaderHandle  { std::uint32_t id{}; };
    struct Pipeline      { std::uint32_t id{}; };
}
//=============================================================================
//
// pipeline.hpp
#pragma once
#include <vector>
#include <string_view>
namespace rndr
{
    enum class ShaderStage { Vertex, Fragment, Compute };
    using ShaderIR = std::vector<std::uint32_t>;           // SPIR‑V blob

    struct PipelineDesc
    {
        ShaderHandle vs{};
        ShaderHandle fs{};
        // + blend, depth‑stencil, raster state ...
    };

    struct DrawCall
    {
        Pipeline     pipeline;
        BufferHandle vbo;
        std::uint32_t vertexCount = 0;
    };

    struct Dispatch
    {
        Pipeline pipeline;
        std::uint32_t groupsX{1}, groupsY{1}, groupsZ{1};
    };
}
//=============================================================================
//
// device.hpp
#pragma once
#include <memory>
#include <span>
namespace rndr
{
    enum class BufferUsage { Static, Dynamic };

    struct IRenderDevice
    {
        virtual BufferHandle createBuffer(std::size_t bytes, BufferUsage) = 0;
        virtual void         upload(BufferHandle, std::span<const std::byte>)   = 0;

        virtual ShaderHandle createShader(ShaderIR ir, ShaderStage stage)       = 0;
        virtual Pipeline     createPipeline(const PipelineDesc&)                = 0;

        virtual void         draw(const DrawCall&)                              = 0;
        virtual void         dispatch(const Dispatch&)                          = 0;

        virtual void         present()                                          = 0;
        virtual ~IRenderDevice()                                                = default;
    };

    // Factory
    enum class Target { CPU, OpenGL, FPGA };
    std::unique_ptr<IRenderDevice> makeDevice(Target);
}
//=============================================================================
//
// cpu_backend.hpp / .cpp
#pragma once
#include <vector>
#include <unordered_map>
#include <thread>
#include <execution>   // C++20 parallel algorithms
namespace rndr
{
    class CPUBackend final : public IRenderDevice
    {
        struct Buffer { std::vector<std::byte> data; BufferUsage usage; };
        std::unordered_map<std::uint32_t, Buffer> buffers_;
        std::uint32_t nextId_ = 1;

        // Extremely naïve software rasterizer for demonstration only
        void rasterTriangle(/* ... */);

    public:
        BufferHandle createBuffer(std::size_t bytes, BufferUsage) override;
        void         upload(BufferHandle, std::span<const std::byte>) override;

        ShaderHandle createShader(ShaderIR, ShaderStage) override;
        Pipeline     createPipeline(const PipelineDesc&)  override;

        void draw(const DrawCall&)     override;
        void dispatch(const Dispatch&) override {}

        void present() override {}
    };
}
//=============================================================================
//
// gl_backend.hpp / .cpp
#pragma once
#ifdef ENABLE_BACKEND_GL
#include <GL/glew.h>
#include <unordered_map>
namespace rndr
{
    class GLBackend final : public IRenderDevice
    {
        struct Buffer { GLuint glId{0}; BufferUsage usage; };
        std::unordered_map<std::uint32_t, Buffer> buffers_;
        std::uint32_t nextId_ = 1;

        GLuint compileShader(ShaderStage, const ShaderIR&);

    public:
        BufferHandle createBuffer(std::size_t bytes, BufferUsage) override;
        void         upload(BufferHandle, std::span<const std::byte>) override;

        ShaderHandle createShader(ShaderIR, ShaderStage) override;
        Pipeline     createPipeline(const PipelineDesc&)  override;

        void draw(const DrawCall&)     override;
        void dispatch(const Dispatch&) override {}

        void present() override;
    };
}
#endif // ENABLE_BACKEND_GL
//=============================================================================
//
// sycl_backend.hpp / .cpp
#pragma once
#ifdef ENABLE_BACKEND_FPGA
#include <sycl/sycl.hpp>
namespace rndr
{
    class SYCLBackend final : public IRenderDevice
    {
        sycl::queue q_;
        std::vector<sycl::buffer<std::byte, 1>> syclBuffers_;
        std::uint32_t nextId_ = 1;

    public:
        SYCLBackend()
        : q_{ sycl::ext::intel::fpga_emulator_selector{} }  // pick FPGA at run‑time
        {}

        BufferHandle createBuffer(std::size_t bytes, BufferUsage) override;
        void         upload(BufferHandle, std::span<const std::byte>) override;

        ShaderHandle createShader(ShaderIR, ShaderStage) override;
        Pipeline     createPipeline(const PipelineDesc&)  override;

        void draw(const DrawCall&)     override {}
        void dispatch(const Dispatch&) override;

        void present() override {}
    };
}
#endif // ENABLE_BACKEND_FPGA
//=============================================================================
//
// factory.cpp
#include <stdexcept>
namespace rndr
{
    std::unique_ptr<IRenderDevice> makeDevice(Target t)
    {
        switch (t)
        {
            case Target::CPU:    return std::make_unique<CPUBackend>();
#ifdef ENABLE_BACKEND_GL
            case Target::OpenGL: return std::make_unique<GLBackend>();
#endif
#ifdef ENABLE_BACKEND_FPGA
            case Target::FPGA:   return std::make_unique<SYCLBackend>();
#endif
            default: throw std::runtime_error("Unsupported target");
        }
    }
}
//=============================================================================
//
// main.cpp
#include <iostream>
int main()
{
    using namespace rndr;

    auto device = makeDevice(Target::CPU);        // choose at run‑time

    auto vbo = device->createBuffer(3 * sizeof(float) * 3, BufferUsage::Static);
    std::array<float, 9> triangle = { 0.0f, 0.5f, 0.0f,
                                     -0.5f,-0.5f, 0.0f,
                                      0.5f,-0.5f, 0.0f };
    device->upload(vbo, std::as_bytes(std::span(triangle)));

    // assume we already have a pipeline id=1 that draws a solid color
    DrawCall dc;
    dc.pipeline.id = 1;
    dc.vbo         = vbo;
    dc.vertexCount = 3;

    device->draw(dc);
    device->present();

    std::cout << "Frame rendered on CPU backend\n";
}
//=============================================================================
//
// CMakeLists.txt (excerpt)
cmake_minimum_required(VERSION 3.21)
project(MultiDeviceRenderer CXX)

option(ENABLE_BACKEND_GL   "Build OpenGL backend" OFF)
option(ENABLE_BACKEND_FPGA "Build FPGA backend"   OFF)

set(CMAKE_CXX_STANDARD 20)

add_library(renderer
    multi_platform_renderer.hpp     # amalgamated for brevity
)

target_include_directories(renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(ENABLE_BACKEND_GL)
    find_package(OpenGL REQUIRED)
    target_link_libraries(renderer PRIVATE OpenGL::GL GLEW)
    target_compile_definitions(renderer PUBLIC ENABLE_BACKEND_GL)
endif()

if(ENABLE_BACKEND_FPGA)
    # oneAPI SYCL
    find_package(IntelSYCL REQUIRED)
    target_link_libraries(renderer PRIVATE IntelSYCL::SYCL)
    target_compile_definitions(renderer PUBLIC ENABLE_BACKEND_FPGA)
endif()

add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE renderer)