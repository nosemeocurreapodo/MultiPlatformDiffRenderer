#pragma once
// #include <string>
// #include <vector>
// #include <utility>
// #include <iostream>
// #include <type_traits>
// #include <cassert>

#include "backends/xrt/devicexrt.h"
// #include "core/format_converters.h"
#include "core/types.h"
#include "core/camera.h"
#include "core/render_constants.h"
#include "core/error_handling.h"
#include "backends/xrt/meshxrt.h"
#include "backends/xrt/texturexrt.h"

class BaseRendererXRT
{
public:
    BaseRendererXRT()
    {
    }

protected:
};

class DepthRendererXRT
{
public:
    DepthRendererXRT()
    {
        kernel_ = xrt::kernel(device_xrt, uuid_xrt, "DepthRenderHLS");
    }

    void Render(MeshXRT &mesh,
                const SE3 &pose,
                const Camera &cam,
                int out_lvl,
                TextureXRT<float> &depth_texture)
    {
        // assert(kernel_.group_id(0) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(1) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(2) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(3) == mesh.ebo.bo_.get_memory_group());
        // assert(kernel_.group_id(4) == depth_texture.storage_.bo_.get_memory_group());

        mesh.pos_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        mesh.tex_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        mesh.wei_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        mesh.ebo_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        xrt::run run = kernel_(mesh.pos_buffer_.bo_, mesh.tex_buffer_.bo_, mesh.wei_buffer_.bo_, mesh.ebo_buffer_.bo_,
                               depth_texture.storage_.bo_,
                               mesh.pos_buffer_.size(), mesh.tex_buffer_.size(), mesh.wei_buffer_.size(), mesh.ebo_buffer_.size(),
                               depth_texture.width(out_lvl), depth_texture.height(out_lvl), depth_texture.nodata(), out_lvl,
                               pose.so3().unit_quaternion().x(), pose.so3().unit_quaternion().y(), pose.so3().unit_quaternion().z(), pose.so3().unit_quaternion().w(),
                               pose.translation()(0), pose.translation()(1), pose.translation()(2),
                               cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3));
        run.wait();
        depth_texture.storage_.bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    }

    // private:
    xrt::kernel kernel_;
};

class TestRendererXRT
{
public:
    TestRendererXRT()
    {
        kernel_ = xrt::kernel(device_xrt, uuid_xrt, "TestRenderHLS");
    }

    void Render(MeshXRT &mesh,
                const SE3 &pose,
                const Camera &cam,
                int out_lvl,
                TextureXRT<float> &depth_texture)
    {
        // mesh.pos_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        // mesh.tex_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        // mesh.wei_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        // mesh.ebo_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        xrt::run run = kernel_(depth_texture.storage_.bo_,
                               depth_texture.width(out_lvl), depth_texture.height(out_lvl), depth_texture.nodata(), out_lvl);
        run.wait();
        depth_texture.storage_.bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    }

    // private:
    xrt::kernel kernel_;
};
