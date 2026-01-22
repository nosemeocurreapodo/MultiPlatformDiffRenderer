#pragma once
// #include <string>
// #include <vector>
// #include <utility>
// #include <iostream>
// #include <type_traits>
// #include <cassert>

#include "backends/xrt/devicexrt.h"
// #include "core/format_converters.h"
// #include "core/types.h"
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
                const linalg::SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int out_lvl,
                TextureXRT<float> &depth_texture)
    {
        // assert(kernel_.group_id(0) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(1) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(2) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(3) == mesh.ebo.bo_.get_memory_group());
        // assert(kernel_.group_id(4) == depth_texture.storage_.bo_.get_memory_group());

        mesh.vertex_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        mesh.ebo_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        xrt::run run = kernel_(mesh.vertex_buffer_.bo_, mesh.ebo_buffer_.bo_,
                               depth_texture.storage_.bo_,
                               mesh.vertex_buffer_.size(), mesh.ebo_buffer_.size(),
                               depth_texture.width(0), depth_texture.height(0), depth_texture.nodata(), out_lvl,
                               pose.so3().unit_quaternion().x(), pose.so3().unit_quaternion().y(), pose.so3().unit_quaternion().z(), pose.so3().unit_quaternion().w(),
                               pose.translation()(0), pose.translation()(1), pose.translation()(2),
                               cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3));
        run.wait();
        depth_texture.storage_.bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    }

    // private:
    xrt::kernel kernel_;
};

class ImageRendererXRT
{
public:
    ImageRendererXRT()
    {
        kernel_ = xrt::kernel(device_xrt, uuid_xrt, "ImageRenderHLS");
    }

    void Render(MeshXRT &mesh,
                const linalg::SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                TextureXRT<ImageType> &diffuse_texture,
                TextureXRT<ImageType> &out_texture)
    {
        // assert(kernel_.group_id(0) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(1) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(2) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(3) == mesh.ebo.bo_.get_memory_group());
        // assert(kernel_.group_id(4) == depth_texture.storage_.bo_.get_memory_group());

        mesh.vertex_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        mesh.ebo_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        diffuse_texture.storage_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        xrt::run run = kernel_(mesh.vertex_buffer_.bo_,
                               mesh.ebo_buffer_.bo_,
                               diffuse_texture.storage_.bo_,
                               diffuse_texture.storage_.bo_,
                               diffuse_texture.storage_.bo_,
                               diffuse_texture.storage_.bo_,
                               diffuse_texture.level(in_lvl).offset,
                               out_texture.storage_.bo_,
                               out_texture.level(out_lvl).offset,
                               mesh.vertex_buffer_.size(), mesh.ebo_buffer_.size(),
                               diffuse_texture.width(0), diffuse_texture.height(0), diffuse_texture.nodata(), in_lvl,
                               out_texture.width(0), out_texture.height(0), out_texture.nodata(), out_lvl,
                               pose.so3().unit_quaternion().x(), pose.so3().unit_quaternion().y(), pose.so3().unit_quaternion().z(), pose.so3().unit_quaternion().w(),
                               pose.translation()(0), pose.translation()(1), pose.translation()(2),
                               cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3),
                               exposure(0), exposure(1));
        run.wait();
        out_texture.storage_.bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    }

    // private:
    xrt::kernel kernel_;
};

/*
class DiffRendererXRT
{
public:
    DiffRendererXRT()
    {
        kernel_ = xrt::kernel(device_xrt, uuid_xrt, "DiffRenderHLS");
    }

    void Render(MeshXRT &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                unsigned int in_lvl,
                unsigned int out_lvl,
                TextureXRT<float> &image_texture,
                TextureXRT<float> &depth_texture,
                TextureXRT<linalg::Vec3<float>> &jtra_texture,
                TextureXRT<linalg::Vec3<float>> &jrot_texture,
                TextureXRT<linalg::Vec3<float>> &jmap_texture,
                TextureXRT<linalg::Vec3<float>> &pids_texture)
    {
        // assert(kernel_.group_id(0) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(1) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(2) == mesh.pos.bo_.get_memory_group());
        // assert(kernel_.group_id(3) == mesh.ebo.bo_.get_memory_group());
        // assert(kernel_.group_id(4) == depth_texture.storage_.bo_.get_memory_group());

        mesh.vertex_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        mesh.ebo_buffer_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        mesh.diffuse_.storage_.bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        xrt::run run = kernel_(mesh.vertex_buffer_.bo_, mesh.ebo_buffer_.bo_,
                               mesh.diffuce_.storage_.bo_, image_texture.storage_.bo_, depth_texture.storage_.bo_, jtra_texture.storage_.bo_, jrot_texture.storage_.bo_, pids_texture.storage_.bo_,
                               mesh.pos_buffer_.size(), mesh.tex_buffer_.size(), mesh.wei_buffer_.size(), mesh.ebo_buffer_.size(),
                               f_texture.width(0), f_texture.height(0), f_texture.nodata(), in_lvl,
                               image_texture.width(0), image_texture.height(0),
                               image_texture.nodata(), depth_texture.nodata(), jtra_texture.nodata(), jrot_texture.nodata(), pids_texture.nodata(),
                               out_lvl,
                               pose.so3().unit_quaternion().x(), pose.so3().unit_quaternion().y(), pose.so3().unit_quaternion().z(), pose.so3().unit_quaternion().w(),
                               pose.translation()(0), pose.translation()(1), pose.translation()(2),
                               cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3));
        run.wait();
        image_texture.storage_.bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        depth_texture.storage_.bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        jtra_texture.storage_.bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        jrot_texture.storage_.bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        jmap_texture.storage_.bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        pids_texture.storage_.bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    }

    // private:
    xrt::kernel kernel_;
};
*/
