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

template <template <class> class TextureViewWrite, class Base>
class DeferredBase
{
public:
    using DBase = DeferredRendererBase<TextureViewWrite>;
    using DFragment = typename DBase::Fragment;
    using DUniforms = typename DBase::Uniforms;
    using DInTextures = typename DBase::InTextures;

    using BFragment = typename Base::Fragment;
    using BUniforms = typename Base::Uniforms;
    using BInTextures = typename Base::InTextures;
    using BOutTextures = typename Base::OutTextures;

    template <typename VertexBufferView, typename EboBufferView>
    static void RenderTileDeferred(const BoundingBox<IntType> &viewport,
                                   const BoundingBox<IntType> &tile_viewport,
                                   const VertexBufferView &vertex_buffer,
                                   const EboBufferView &ebo_buffer,
                                   const BUniforms &buniforms,
                                   const BInTextures &bintextures,
                                   BOutTextures &bouttextures,
                                   DFragment dfragment_buffer[],
                                   BFragment bfragment_buffer[],
                                   RealType depth_buffer[])
    {
        DUniforms duniforms;
        duniforms.pose_matrix = buniforms.pose_matrix;
        duniforms.view_matrix = buniforms.view_matrix;

        DInTextures dintextures{0};

        draw_tile<VertexBufferView,
                  EboBufferView,
                  DBase>(viewport,
                         tile_viewport,
                         vertex_buffer,
                         ebo_buffer,
                         duniforms,
                         dintextures,
                         dfragment_buffer,
                         depth_buffer);
        draw_tile_deferred<DFragment,
                           Base>(tile_viewport,
                                 dfragment_buffer,
                                 buniforms,
                                 bintextures,
                                 bfragment_buffer);
        sync_outtextures<Base>(bouttextures,
                               tile_viewport,
                               bfragment_buffer);
    }

private:
};
