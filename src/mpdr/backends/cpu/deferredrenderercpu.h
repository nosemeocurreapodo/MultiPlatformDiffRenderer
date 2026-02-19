#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
#include "mpdr/common/render_constants.h"
#include "mpdr/common/error_handling.h"
#include "mpdr/common/types.h"
#include "mpdr/common/camera.h"
#include "mpdr/common/boundingbox.h"
#include "mpdr/backends/cpu/devicecpu.h"
#include "mpdr/backends/cpu/texturecpu.h"
#include "mpdr/backends/cpu/buffercpu.h"
#include "mpdr/backends/cpu/meshcpu.h"
#include "mpdr/backends/base/deferredrendererbase.h"

template <class Base>
class DeferredRendererBaseCPU : public DeferredBase<TextureViewReadCPU, Base>
{
public:
    using DBase = DeferredBase<TextureViewReadCPU, Base>;
    using DFragment = typename DBase::DFragment;
    using BFragment = typename Base::Fragment;

    template <typename VertexBufferView, typename EboBufferView>
    void RenderNaive(const BoundingBox<int> &viewport,
                     // const Mesh &mesh,
                     const VertexBufferView &vertex_buffer,
                     const EboBufferView &ebo_buffer,
                     const typename Base::Uniforms &uniforms,
                     const typename Base::InTextures &intextures,
                     typename Base::OutTextures &outtextures)
    {
        const int W = viewport.width_;
        const int H = viewport.height_;
        const std::size_t n = static_cast<std::size_t>(W) * static_cast<std::size_t>(H);

        // Resize once, reuse capacity across calls
        bfragment_buffer_.resize(n);
        dfragment_buffer_.resize(n);
        depth_buffer_.resize(n);

        // Initialize buffers
        DFragment dnodata_frag;
        dnodata_frag.fpos = Vec3<float>(0.0f, 0.0f, 0.0f);
        dnodata_frag.kfpos = Vec3<float>(0.0f, 0.0f, 0.0f);
        dnodata_frag.bcid = Vec3<float>(0.0f, 0.0f, 0.0f);

        const BFragment bnodata_frag = Base::fragment_nodata(outtextures);
        std::fill(bfragment_buffer_.begin(), bfragment_buffer_.end(), bnodata_frag);
        std::fill(dfragment_buffer_.begin(), dfragment_buffer_.end(), dnodata_frag);
        std::fill(depth_buffer_.begin(), depth_buffer_.end(), -1.0f);

        BFragment *bfragment_buffer = bfragment_buffer_.data();
        DFragment *dfragment_buffer = dfragment_buffer_.data();
        float *depth_buffer = depth_buffer_.data();

        this->RenderTileDeferred(viewport,
                                 viewport,
                                 vertex_buffer,
                                 ebo_buffer,
                                 uniforms,
                                 intextures,
                                 outtextures,
                                 dfragment_buffer,
                                 bfragment_buffer,
                                 depth_buffer);
    }

private:
    std::vector<BFragment> bfragment_buffer_;
    std::vector<DFragment> dfragment_buffer_;
    std::vector<float> depth_buffer_;
};

class ImageDRendererCPU
    : public DeferredRendererBaseCPU<ImageRendererBase<TextureViewReadCPU,
                                                       TextureViewWriteCPU>>
{
public:
    using Base = ImageRendererBase<TextureViewReadCPU,
                                   TextureViewWriteCPU>;

    ImageDRendererCPU() = default;
    ~ImageDRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &diffuse_texture,
                TextureCPU<ImageType> &out_texture)
    {
        Base::Uniforms uniforms;
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uniforms.camera = cam;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;
        uniforms.exposure = exposure;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{diffuse_texture.MapRead(out_lvl)};
        Base::OutTextures outtextures{out_texture.MapWrite(out_lvl)};

        DeferredRendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh.vertex_buffer_.MapRead(),
            mesh.ebo_buffer_.MapRead(),
            uniforms,
            intextures,
            outtextures);
    }

private:
};
