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
    : public DepthRendererBase<MeshHLS, TextureHLS>
{
public:
    DepthRendererHLS() = default;
    ~DepthRendererHLS() = default;

    void Render(const MeshHLS &mesh,
                const SE3 &pose,
                const Camera &cam,
                int out_lvl,
                TextureHLS<Scalar> &out_texture)
    {
        TextureHLS<Scalar> out_texture_part(160, 120, );
        out_texture_part.fill(out_lvl, out_texture_part.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * RendererBase<DepthRendererBase<Mesh, Texture>>::opencv2opengl_ * pose.matrix();
        out_lvl_ = out_lvl;

        // out_texture_ = &out_texture;

        const Int W = static_cast<Int>(out_texture.width(out_lvl));
        const Int H = static_cast<Int>(out_texture.height(out_lvl));
        for (int py = 0; py < 2; py++)
        {
            for (int px = 0; px < 2; px++)
            {
                Int Ws = px * W / 2;
                Int Wf = (px + 1) * W / 2;
                Int Hs = py * H / 2;
                Int Hf = (py + 1) * H / 2;

                BoundingBox<Int> viewport(Ws, Wf, Hs, Hf);

                Textures textures{out_texture_part};

                RendererBase<DepthRendererBase<Mesh, Texture>>::Render(mesh, viewport, textures);

                for(int y = 0; y < H / 2; y++)
                {
                    for(int x = 0; x < W / 2; x++)
                    {
                        out_texture_part
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
    : public ImageRendererBase<MeshHLS, TextureHLS>
{
public:
    ImageRendererHLS() = default;
    ~ImageRendererHLS() = default;

    void Render(const MeshHLS &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureHLS<Scalar> &in_texture,
                TextureHLS<Scalar> &out_texture)
    {
        ImageRendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, in_texture, out_texture);
    }

private:
};
