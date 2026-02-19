#pragma once

#include "mpdr/common/types.h"
#include "mpdr/common/camera.h"

template <class Texture>
static void DepthRendererRef(const Texture &depth_texture,
                             const SE3<float> &pose,
                             const PinholeCamera<float> &cam,
                             int out_lvl,
                             Texture &out_texture)
{
    out_texture.fill(out_lvl, out_texture.nodata());

    auto depth_map = depth_texture.MapRead(out_lvl);
    auto out_map = out_texture.MapWrite(out_lvl);

    for (int y = 0; y < out_texture.height(out_lvl); y++)
    {
        for (int x = 0; x < out_texture.width(out_lvl); x++)
        {
            float kf_depth = depth_map(y, x);
            Vec2<float> kf_pix((float(x) + 0.5f) / out_texture.width(out_lvl), (float(y) + 0.5f) / out_texture.height(out_lvl));
            Vec3<float> kf_ray = cam.PixToRay(kf_pix);
            Vec3<float> kf_vec = kf_ray * kf_depth;
            Vec3<float> f_vec = pose * kf_vec;
            float f_depth = f_vec(2);
            if (f_depth <= 0.0f)
                continue;
            Vec3<float> f_ray = f_vec / f_vec(2);
            Vec2<float> f_pix = cam.RayToPix(f_ray);
            if (!cam.IsPixVisible(f_pix))
                continue;
            f_pix(0) = min(float(round(f_pix(0) * out_texture.width(out_lvl))), float(out_texture.width(out_lvl) - 1));
            f_pix(1) = min(float(round(f_pix(1) * out_texture.height(out_lvl))), float(out_texture.height(out_lvl) - 1));
            float prev_depth = out_map(f_pix(1), f_pix(0));
            if (prev_depth == out_texture.nodata() || (f_depth < prev_depth))
                out_map(f_pix(1), f_pix(0)) = f_depth;
        }
    }
}

template <class DepthTexture, class ImageTexture>
static void ImageRendererRef(const DepthTexture &depth_texture,
                             const ImageTexture &image_texture,
                             const SE3<float> &pose,
                             const PinholeCamera<float> &cam,
                             int out_lvl,
                             ImageTexture &out_texture)
{
    out_texture.fill(out_lvl, out_texture.nodata());

    auto depth_map = depth_texture.MapRead(out_lvl);
    auto image_map = image_texture.MapRead(out_lvl);
    auto out_map = out_texture.MapWrite(out_lvl);

    for (int y = 0; y < out_texture.height(out_lvl); y++)
    {
        for (int x = 0; x < out_texture.width(out_lvl); x++)
        {
            float kf_depth = depth_map(y, x);
            ImageType kf = image_map(y, x);
            Vec2<float> kf_pix((float(x) + 0.5f) / out_texture.width(out_lvl), (float(y) + 0.5f) / out_texture.height(out_lvl));
            Vec3<float> kf_ray = cam.PixToRay(kf_pix);
            Vec3<float> kf_vec = kf_ray * kf_depth;
            Vec3<float> f_vec = pose * kf_vec;
            float f_depth = f_vec(2);
            if (f_depth <= 0.0f)
                continue;
            Vec3<float> f_ray = f_vec / f_vec(2);
            Vec2<float> f_pix = cam.RayToPix(f_ray);
            if (!cam.IsPixVisible(f_pix))
                continue;
            f_pix(0) = min(float(round(f_pix(0) * out_texture.width(out_lvl))), float(out_texture.width(out_lvl) - 1));
            f_pix(1) = min(float(round(f_pix(1) * out_texture.height(out_lvl))), float(out_texture.height(out_lvl) - 1));
            out_map(f_pix(1), f_pix(0)) = kf;
        }
    }
}