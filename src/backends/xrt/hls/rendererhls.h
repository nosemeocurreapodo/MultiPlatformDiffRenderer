#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
// #include "core/types.h"
#include "linalg/linalg.h"
#include "core/camera.h"
#include "core/boundingbox.h"
#include "core/render_constants.h"
#include "backends/base/rendererbase.h"
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/bufferhls.h"
#include "backends/xrt/hls/meshhls.h"

#include "hls_math.h"
#include "ap_int.h"
#include "ap_fixed.h"

using MathType = float; // ap_fixed<32, 16>;
using MeshType = float;
using DepthType = float;
using ImageType = float;
using ErrorType = float;
using DType = float;
using IdType = float;

class DepthRendererRAM
    : public DepthRendererBase<MathType, DepthType, MeshHLS<MeshType, BufferRAM, TextureRAM>, TextureRAM>
{
public:
    DepthRendererRAM() = default;
    ~DepthRendererRAM() = default;

    void Render(const MeshHLS<MeshType, BufferRAM, TextureRAM> &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int out_lvl,
                TextureRAM<float> &out_texture)
    {
        DepthRendererBase::Render(mesh, pose, cam, out_lvl, out_texture);
    }

private:
};

class DepthRendererBRAM
    : public DepthRendererBase<MathType, DepthType, MeshHLS<MeshType, BufferRAM, TextureRAM>, TextureBRAM>
{
public:
    DepthRendererBRAM() = default;
    ~DepthRendererBRAM() = default;

    void Render(const MeshHLS<MeshType, BufferRAM, TextureRAM> &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int out_lvl,
                TextureRAM<float> &out_texture)
    {
        out_lvl_ = out_lvl;

        unsigned int W = out_texture.width(out_lvl);
        unsigned int H = out_texture.height(out_lvl);

        unsigned int crop_W = TextureBRAM<DepthType>::max_x;
        unsigned int crop_H = TextureBRAM<DepthType>::max_y;

        unsigned int x_size = W / crop_W;
        unsigned int y_size = H / crop_H;

        MathType scale_W = MathType(W) / MathType(crop_W);
        MathType scale_H = MathType(H) / MathType(crop_H);

        BoundingBox<int> viewport(0, crop_W, 0, crop_H);
        TextureBRAM<DepthType> out_texture_part(crop_W, crop_H, out_texture.nodata());
        Textures textures{out_texture_part};

    depth_renderer_loop_y:
        for (int py = 0; py < y_size; py++)
        {
#pragma HLS loop_tripcount min = 15 max = 15 avg = 15

        depth_renderer_loop_x:
            for (int px = 0; px < x_size; px++)
            {
#pragma HLS loop_tripcount min = 20 max = 20 avg = 20

                unsigned int start_W = px * crop_W;
                // Int Wf = (px + 1) * Wn;
                unsigned int start_H = py * crop_H;
                // Int Hf = (py + 1) * Hn;

                linalg::Vec4<MathType> cam_params = cam.GetParams();
                cam_params(0) = cam_params(0) * MathType(W) / MathType(crop_W);
                cam_params(1) = cam_params(1) * MathType(H) / MathType(crop_H);
                cam_params(2) = (cam_params(2) * MathType(W) - MathType(start_W)) / MathType(crop_W);
                cam_params(3) = (cam_params(3) * scale_H - MathType(y_size - 1 - py));
                Camera<MathType> new_cam;
                new_cam.SetParams(cam_params);

                t_matrix_ = new_cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_ * pose.matrix();

                out_texture_part.fill(out_lvl, out_texture_part.nodata());

                RendererBase::Render(viewport, mesh, textures);

            depth_renderer_copy_loop_y:
                for (int y = 0; y < crop_H; y++)
                {
                depth_renderer_copy_loop_x:
                    for (int x = 0; x < crop_W; x++)
                    {
                        DepthType data = out_texture_part.texel_(y, x, out_lvl);
                        out_texture.set_texel_(data, start_H + y, start_W + x, out_lvl);
                    }
                }
            }
        }
    }

private:
};

class ImageRendererRAM
    : public ImageRendererBase<MathType, ImageType, MeshHLS<MeshType, BufferRAM, TextureRAMCached>, TextureRAMCached>
{
public:
    ImageRendererRAM() = default;
    ~ImageRendererRAM() = default;

    void Render(MeshHLS<MeshType, BufferRAM, TextureRAMCached> &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                unsigned int in_lvl,
                unsigned int out_lvl,
                TextureRAMCached<MathType> &depth_texture,
                TextureRAMCached<ImageType> &out_texture)
    {
        ImageRendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, depth_texture, out_texture);
    }

private:
};

class ImageRendererBRAM
    : public ImageRendererBase<MathType, ImageType, MeshHLS<MeshType, BufferRAM, TextureRAM>, TextureBRAM>
{
public:
    ImageRendererBRAM() = default;
    ~ImageRendererBRAM() = default;

    void Render(MeshHLS<MeshType, BufferRAM, TextureRAM> &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                unsigned int in_lvl,
                unsigned int out_lvl,
                TextureRAM<ImageType> &depth_texture,
                TextureRAM<ImageType> &out_texture)
    {
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;

        unsigned int in_W = mesh.diffuse_.width(out_lvl);
        unsigned int in_H = mesh.diffuse_.height(out_lvl);

        unsigned int out_W = out_texture.width(out_lvl);
        unsigned int out_H = out_texture.height(out_lvl);

        unsigned int crop_W = TextureBRAM<ImageType>::max_x;
        unsigned int crop_H = TextureBRAM<ImageType>::max_y;

        unsigned int x_size = out_W / crop_W;
        unsigned int y_size = out_H / crop_H;

        MathType scale_W = MathType(out_W) / MathType(crop_W);
        MathType scale_H = MathType(out_H) / MathType(crop_H);

        BoundingBox<int> viewport(0, crop_W, 0, crop_H);

        TextureBRAM<ImageType> diffuse_texture_part(crop_W, crop_H, mesh.diffuse_.nodata());
        TextureBRAM<MathType> depth_texture_part(crop_W, crop_H, depth_texture.nodata());
        TextureBRAM<ImageType> out_texture_part(crop_W, crop_H, out_texture.nodata());

        Textures textures{depth_texture_part, diffuse_texture_part, out_texture_part};

    depth_renderer_loop_y:
        for (int py = 0; py < y_size; py++)
        {
#pragma HLS loop_tripcount min = 15 max = 15 avg = 15

        depth_renderer_loop_x:
            for (int px = 0; px < x_size; px++)
            {
#pragma HLS loop_tripcount min = 15 max = 15 avg = 15

                unsigned int start_W = px * crop_W;
                // Int Wf = (px + 1) * Wn;
                unsigned int start_H = py * crop_H;
                // Int Hf = (py + 1) * Hn;

                linalg::Vec4<MathType> cam_params = cam.GetParams();
                cam_params(0) = cam_params(0) * MathType(out_W) / MathType(crop_W);
                cam_params(1) = cam_params(1) * MathType(out_H) / MathType(crop_H);
                cam_params(2) = (cam_params(2) * MathType(out_W) - MathType(start_W)) / MathType(crop_W);
                cam_params(3) = (cam_params(3) * scale_H - MathType(y_size - 1 - py));
                Camera<MathType> new_cam;
                new_cam.SetParams(cam_params);

                t_matrix_ = new_cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_ * pose.matrix();

                depth_texture_part.fill(out_lvl, depth_texture_part.nodata());
                out_texture_part.fill(out_lvl, out_texture_part.nodata());

            image_renderer_in_copy_loop_y:
                for (int y = 0; y < crop_H; y++)
                {
                image_renderer_in_copy_loop_x:
                    for (int x = 0; x < crop_W; x++)
                    {
                        ImageType data = mesh.diffuse_.texel_(start_H + y, start_W + x, out_lvl);
                        out_texture_part.set_texel_(data, y, x, out_lvl);
                    }
                }

                RendererBase::Render(viewport, mesh, textures);

            image_renderer_out_copy_loop_y:
                for (int y = 0; y < crop_H; y++)
                {
                image_renderer_out_copy_loop_x:
                    for (int x = 0; x < crop_W; x++)
                    {
                        MathType depth = depth_texture_part.texel_(y, x, out_lvl);
                        depth_texture.set_texel_(depth, start_H + y, start_W + x, out_lvl);

                        ImageType data = out_texture_part.texel_(y, x, out_lvl);
                        out_texture.set_texel_(data, start_H + y, start_W + x, out_lvl);
                    }
                }
            }
        }
    }

private:
};
/*
    class DiffRendererRAM
    : public DiffRendererBase<MathType, ImageType, DepthType, DType, IdType, MeshHLS<MeshType>, TextureRAM>
{
public:
    DiffRendererRAM() = default;
    ~DiffRendererRAM() = default;

    void Render(MeshHLS<MeshType> &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                unsigned int in_lvl,
                unsigned int out_lvl,
                TextureRAM<ImageType> &image_texture,
                TextureRAM<DepthType> &depth_texture,
                TextureRAM<linalg::Vec3<DType>> &jtra_texture,
                TextureRAM<linalg::Vec3<DType>> &jrot_texture,
                TextureRAM<linalg::Vec3<DType>> &jmap_texture,
                TextureRAM<linalg::Vec3<IdType>> &pids_texture)
    {

        // ErrorHandling::ValidateTextureDimensions(r_texture.width(out_lvl), r_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        DiffRendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, image_texture, depth_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture);
    }

private:
};
*/
