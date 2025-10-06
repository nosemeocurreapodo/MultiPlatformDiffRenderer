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
        out_lvl_ = out_lvl;

        UInt W = out_texture.width(out_lvl);
        UInt H = out_texture.height(out_lvl);

        UInt y_size = 4;
        UInt x_size = 4;

        // Textures textures{out_texture};
        // BoundingBox<Int> viewport(0, W, 0, H);
        // RendererBase<DepthRendererBase<MeshHLS, TextureRAM>>::Render(mesh, viewport, textures);

        UInt crop_W = W / x_size;
        UInt crop_H = H / y_size;

        Scalar scale_W = Scalar(W) / Scalar(crop_W);
        Scalar scale_H = Scalar(H) / Scalar(crop_H);

        BoundingBox<Int> viewport(0, crop_W, 0, crop_H);
        TextureBRAM<Scalar> out_texture_part(crop_W, crop_H, out_texture.nodata());
        Textures textures{out_texture_part};

    depth_renderer_loop_y:
        for (int py = 0; py < y_size; py++)
        {
        depth_renderer_loop_x:
            for (int px = 0; px < x_size; px++)
            {
                Int start_W = px * crop_W;
                // Int Wf = (px + 1) * Wn;
                Int start_H = py * crop_H;
                // Int Hf = (py + 1) * Hn;

                Vec4 cam_params = cam.GetParams();
                cam_params(0) = cam_params(0) * Scalar(W) / Scalar(crop_W);
                cam_params(1) = cam_params(1) * Scalar(H) / Scalar(crop_H);
                cam_params(2) = (cam_params(2) * Scalar(W) - Scalar(start_W)) / Scalar(crop_W);
                cam_params(3) = (cam_params(3) * scale_H - Scalar(y_size - 1 - py));
                Camera new_cam;
                new_cam.SetParams(cam_params);

                t_matrix_ = new_cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * RendererBase<DepthRendererBase<MeshHLS, TextureBRAM>>::opencv2opengl_ * pose.matrix();

                out_texture_part.fill(out_lvl, out_texture_part.nodata());

                RendererBase<DepthRendererBase<MeshHLS, TextureBRAM>>::Render(mesh, viewport, textures);

            depth_renderer_copy_loop_y:
                for (int y = 0; y < crop_H; y++)
                {
                depth_renderer_copy_loop_x:
                    for (int x = 0; x < crop_W; x++)
                    {
                        Scalar data = out_texture_part.texel_(y, x, out_lvl);
                        out_texture.set_texel_(data, start_H + y, start_W + x, out_lvl);
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
