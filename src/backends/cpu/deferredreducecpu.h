#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
#include "core/render_constants.h"
#include "core/error_handling.h"
#include "core/types.h"
#include "core/camera.h"
#include "core/boundingbox.h"
#include "backends/base/deferredreducebase.h"
#include "backends/cpu/devicecpu.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/buffercpu.h"
#include "backends/cpu/meshcpu.h"

class ResidualRRCPU
    : public ResidualRRBase<TextureViewReadCPU,
                            TextureViewWriteCPU>
{
public:
    using Base = ResidualRRBase<TextureViewReadCPU, TextureViewWriteCPU>;
    using Fragment = typename Base::Fragment;

    float compute(const MeshCPU &mesh,
                  const SE3<float> &pose,
                  const PinholeCamera<float> &cam,
                  int lvl,
                  const TextureCPU<ImageType> &kf_texture,
                  const TextureCPU<ImageType> &f_texture,
                  float huber_thresh)
    {
        const int W = static_cast<int>(f_texture.width(lvl));
        const int H = static_cast<int>(f_texture.height(lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        const std::size_t n = static_cast<std::size_t>(W) * static_cast<std::size_t>(H);

        // Resize once, reuse capacity across calls
        fragment_buffer_.resize(n);
        depth_buffer_.resize(n);

        // Initialize buffers
        //const Fragment nodata_frag = Base::fragment_nodata(outtextures);
        Fragment nodata_frag;
        nodata_frag.kfpos = Vec3<float>(0.0f, 0.0f, 0.0f);
        nodata_frag.bcid = Vec3<float>(0.0f, 0.0f, 0.0f);
        nodata_frag.fpos = Vec3<float>(0.0f, 0.0f, 0.0f);

        std::fill(fragment_buffer_.begin(), fragment_buffer_.end(), nodata_frag);
        std::fill(depth_buffer_.begin(), depth_buffer_.end(), -1.0f);

        Fragment *fragment_buffer = fragment_buffer_.data();
        float *depth_buffer = depth_buffer_.data();

        return Base::compute(viewport,
                             mesh.vertex_buffer_.MapRead(),
                             mesh.ebo_buffer_.MapRead(),
                             pose,
                             cam,
                             kf_texture.MapRead(lvl),
                             f_texture.MapRead(lvl),
                             fragment_buffer,
                             depth_buffer,
                             huber_thresh);
    }

private:
    std::vector<Fragment> fragment_buffer_;
    std::vector<float> depth_buffer_;
};
