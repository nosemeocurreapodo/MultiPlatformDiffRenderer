#pragma once

#include "backends/base/mappedviewbase.h"
#include "backends/base/rendererbase.h"
#include "core/types.h"
#include "core/boundingbox.h"
#include "core/huber.h"

#ifdef USE_VITIS
#include "backends/xrt/hls/math_common.h"
#else
#include "backends/cpu/math_common.h"
#endif

template <template <class> class TextureViewRead, typename Fragment>
static RealType residual_reduce(const BoundingBox<IntType> &viewport,
                                const PinholeCamera<RealType> cam,
                                const TextureViewRead<ImageType> &kf_texture,
                                const TextureViewRead<ImageType> &f_texture,
                                const Fragment fragment_buffer[],
                                const RealType depth_buffer[],
                                RealType huber_thresh)
{
    RealType sum = 0;
    IntType count = 0;
    for (IntType iy = 0; iy < viewport.height_; iy++)
    {
        for (IntType ix = 0; ix < viewport.width_; ix++)
        {
            IntType x = ix + viewport.min_x_;
            IntType y = iy + viewport.min_y_;
            IntType address = iy * viewport.width_ + ix;

            if (depth_buffer[address] <= 0.0f)
                continue;

            Fragment deffrag = fragment_buffer[address];
            Vec2<RealType> texcoord = cam.pointToPix(deffrag.kfpos);

            if (texcoord(0) < 0 || texcoord(0) > 1.0 || texcoord(1) < 0 || texcoord(1) > 1.0)
                continue;

            RealType kf = sample<RealType, TextureViewRead<ImageType>>(kf_texture,
                                                                       texcoord(1), texcoord(0));

            ImageType f = f_texture(y, x);
            if (kf == kf_texture.nodata() || f == f_texture.nodata())
                continue;
            RealType diff = RealType(f) - RealType(kf);
            RealType w = huber_weight(diff, huber_thresh);
            sum += w * diff * diff;
            count++;
        }
    }
    return sum / RealType(count);
}

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class ResidualRRBase
{
public:
    using Base = DeferredRendererBase<TextureViewWrite>;
    using Fragment = typename Base::Fragment;
    using Uniforms = typename Base::Uniforms;
    using InTextures = typename Base::InTextures;

    template <typename VertexBufferView, typename EboBufferView>
    static RealType compute(const BoundingBox<IntType> &viewport,
                            const VertexBufferView &vertex_buffer,
                            const EboBufferView &ebo_buffer,
                            const SE3<RealType> pose,
                            const PinholeCamera<RealType> cam,
                            const TextureViewRead<ImageType> kf_texture,
                            const TextureViewRead<ImageType> f_texture,
                            Fragment fragment_buffer[],
                            RealType depth_buffer[],
                            RealType huber_thresh)
    {
        Uniforms uniforms;
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        InTextures intextures{0};

        draw_tile<VertexBufferView, EboBufferView, Base>(viewport, vertex_buffer, ebo_buffer, uniforms, intextures, fragment_buffer, depth_buffer);
        return residual_reduce<TextureViewRead, Fragment>(viewport, cam, kf_texture, f_texture, fragment_buffer, depth_buffer, huber_thresh);
    }

private:
};
