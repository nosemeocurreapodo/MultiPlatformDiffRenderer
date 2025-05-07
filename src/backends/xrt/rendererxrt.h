#pragma once

#include "devicexrt.h"

template <typename InTexType, typename OutTexType>
class BaseRendererXRT
{
public:
    BaseRendererXRT()
    {
    }

    void WriteMesh(const MeshCPU &mesh)
    {
        pos_bo_size_ = mesh.pos_buffer_.size();
        pos_bo_ = xrt::bo(device_xrt, mesh.pos_buffer_.size() * sizeof(float), kernel_.group_id(0));
        float *pos_bo_map = pos_bo_.map<float *>();
        std::memcmp(mesh.pos_buffer_.get(), pos_bo_map, mesh.pos_buffer_.size());
        pos_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

        tex_bo_size_ = mesh.tex_buffer_.size();
        tex_bo_ = xrt::bo(device_xrt, mesh.tex_buffer_.size() * sizeof(float), kernel.group_id(2));
        float *tex_bo_map = tex_bo_.map<float *>();
        std::memcmp(mesh.tex_buffer_.get(), tex_bo_map, mesh.pos_buffer_.size());
        tex_bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);

        wei_bo_size_ = mesh.wei_buffer_.size();
        wei_bo_ = xrt::bo(device_xrt, mesh.wei_buffer_.size() * sizeof(float), kernel.group_id(4));
        float *wei_bo_map = wei_bo_.map<float *>();
        std::memcmp(mesh.wei_buffer_.get(), wei_bo_map, mesh.wei_buffer_.size());
        wei_bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);

        ebo_size_ = mesh.ebo_buffer_.size();
        ebo_ = xrt::bo(device_xrt, mesh.ebo_buffer_.size() * sizeof(unsigned int), kernel.group_id(6));
        float *ebo_map = ebo_.map<unsigned int *>();
        std::memcmp(mesh.ebo_buffer_.get(), ebo_map, mesh.ebo_buffer_.size());
        ebo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }

    void WriteInTexture(const TextureCPU<InTexType> &texture)
    {
        in_w_ = texture.width();
        in_h_ = texture.height();
        in_c_ = texture.channels();
        in_tex_bo_ = xrt::bo(device_xrt, texture.size() * sizeof(InTexType), kernel.group_id(10));
        InTexType *in_tex_bo_map = in_tex_bo.map<InTexType *>();
        std::memcmp(texture.get(), in_tex_bo_map, texture.size());
        in_tex_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }

    void PrepareOutTexture(TextureCPU<OutTexType> &texture)
    {
        out_tex_w_ = texture.width();
        out_tex_h_ = texture.height();
        out_tex_c_ = texture.channels();
        out_tex_bo_ = xrt::bo(device_xrt, texture.size() * sizeof(OutTexType), kernel.group_id(14));
    }

    void ReadOutTexture(TextureCPU<OutTexType> &texture)
    {
        out_tex_bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        OutTexType *buffer_map = out_tex_bo_.map<OutTexType *>();
        std::memcmp(buffer_map, texture.data(), texture.size())
    }

    void Render(const SE3 pose, const CameraType cam, int lvl)
    {
        kernel_(pos_bo_, pos_bo_size_,
                tex_bo_, tex_bo_size_,
                wei_bo_, wei_bo_size_,
                ebo_, ebo_size_,
                &pose, &cam,
                in_tex_bo_, in_tex_w, in_tex_h, in_tex_c,
                out_tex_bo_, out_tex_w, out_tex_h, out_tex_c);
    }

protected:
    xrt::bo pos_bo_;
    xrt::bo tex_bo_;
    xrt::bo wei_bo_;
    xrt::bo ebo_;

    unsigned int pos_bo_size_;
    unsigned int tex_bo_size_;
    unsigned int wei_bo_size_;
    unsigned int ebo_size_;

    xrt::bo in_tex_bo_;
    InTexType in_tex_nodata_;
    unsigned int in_tex_w_, in_tex_h_, in_tex_c_;

    xrt::bo out_tex_bo_;
    OutTexType out_tex_nodata_;
    unsigned int out_tx_w_, out_tex_h_, out_tex_c_;

    xrt::kernel kernel_;
};

class DepthRendererXRT : public BaseRendererXRT<float /*InTexType*/, float /*OutTexType*/>
{
public:
    DepthRendererXRT() : BaseRendererXRT()
    {
        kernel_ = xrt::kernel(device_xrt, uuid_xrt, "DepthRenderFPGA");
    }

private:
};