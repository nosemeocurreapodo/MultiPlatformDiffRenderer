#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
#include "core/types.h"
#include "core/camera.h"
#include "core/boundingbox.h"
#include "core/render_constants.h"
#include "backends/base/rendererbase.h"
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/bufferhls.h"
#include "backends/xrt/hls/meshhls.h"

// -----------------------------------------------------------------------------
// DepthRendererCPU
//   Example derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

class DepthRendererHLS
    : public DepthRendererBase<MeshHLS, TextureBRAM>
{
public:
    DepthRendererHLS() = default;
    ~DepthRendererHLS() = default;

    void Render(const MeshHLS &mesh,
                const SE3 &pose,
                const Camera &cam,
                UInt out_lvl,
                TextureRAM<Scalar> &out_texture)
    {
        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * RendererBase<DepthRendererBase<MeshHLS, TextureBRAM>>::opencv2opengl_ * pose.matrix();
        out_lvl_ = out_lvl;

        const Int W = static_cast<Int>(out_texture.width(out_lvl));
        const Int H = static_cast<Int>(out_texture.height(out_lvl));

        // Textures textures{out_texture};
        // BoundingBox<Int> viewport(0, W, 0, H);
        // RendererBase<DepthRendererBase<MeshHLS, TextureRAM>>::Render(mesh, viewport, textures);

        TextureBRAM<Scalar> out_texture_part(160, 120, out_texture.nodata());
        Textures textures{out_texture_part};

        for (int py = 0; py < 4; py++)
        {
            for (int px = 0; px < 4; px++)
            {
                Int Ws = px * W / 4;
                Int Wf = (px + 1) * W / 4;
                Int Hs = py * H / 4;
                Int Hf = (py + 1) * H / 4;

                BoundingBox<Int> viewport(Ws, Wf, Hs, Hf);

                out_texture_part.fill(out_lvl, out_texture_part.nodata());

                RendererBase<DepthRendererBase<MeshHLS, TextureBRAM>>::Render(mesh, viewport, textures);

                for (int y = 0; y < H / 4; y++)
                {
                    for (int x = 0; x < W / 4; x++)
                    {
                        Scalar data = out_texture_part.texel_(y, x, out_lvl);
                        out_texture.set_texel_(data, Hs + y, Ws + x, out_lvl);
                    }
                }
            }
        }
    }

private:
};

// -----------------------------------------------------------------------------
// ImageRendererCPU
//   Another example derived class that might output color
// -----------------------------------------------------------------------------

class ImageRendererHLS
    : public ImageRendererBase<MeshHLS, TextureRAM>
{
public:
    ImageRendererHLS() = default;
    ~ImageRendererHLS() = default;

    void Render(const MeshHLS &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureRAM<Scalar> &in_texture,
                TextureRAM<Scalar> &out_texture)
    {
        ImageRendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, in_texture, out_texture);
    }

private:
};
