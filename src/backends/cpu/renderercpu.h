#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
#include "core/render_constants.h"
#include "core/error_handling.h"
#include "core/types.h"
#include "core/camera.h"
#include "core/boundingbox.h"
#include "backends/base/rendererbase.h"
#include "backends/cpu/devicecpu.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/buffercpu.h"
#include "backends/cpu/meshcpu.h"

template <class Base>
class RendererBaseCPU
{
public:
    using Fragment = typename Base::Fragment;

    template <typename Mesh>
    void RenderNaive(const BoundingBox<int> &viewport,
                     const Mesh &mesh,
                     const typename Base::Uniforms &uniforms,
                     const typename Base::InTextures &intextures,
                     typename Base::OutTextures &outtextures)
    {
        const int W = viewport.width_;
        const int H = viewport.height_;
        const std::size_t n = static_cast<std::size_t>(W) * static_cast<std::size_t>(H);

        // Resize once, reuse capacity across calls
        fragment_buffer_.resize(n);
        depth_buffer_.resize(n);

        // Initialize buffers
        const Fragment nodata_frag = Base::fragment_nodata(outtextures);
        std::fill(fragment_buffer_.begin(), fragment_buffer_.end(), nodata_frag);
        std::fill(depth_buffer_.begin(), depth_buffer_.end(), -1.0f);

        Fragment *fragment_buffer = fragment_buffer_.data();
        float *depth_buffer = depth_buffer_.data();

        draw_tile<Mesh, Base>(viewport, mesh, uniforms, intextures, fragment_buffer, depth_buffer);

        Base::sync_outtextures(outtextures, viewport, fragment_buffer, uniforms);
    }

private:
    std::vector<Fragment> fragment_buffer_;
    std::vector<float> depth_buffer_;
};

class DeferredRendererCPU
    : public RendererBaseCPU<DeferredRendererBase<TextureViewReadCPU,
                                                  TextureViewWriteCPU>>
{
public:
    using Base = DeferredRendererBase<TextureViewReadCPU,
                                      TextureViewWriteCPU>;

    DeferredRendererCPU() = default;
    ~DeferredRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int lvl,
                TextureCPU<Vec3<float>> &fpose_texture,
                TextureCPU<Vec3<float>> &kfpose_texture,
                TextureCPU<Vec3<float>> &bcid_texture)
    {
        const int W = static_cast<int>(fpose_texture.width(lvl));
        const int H = static_cast<int>(fpose_texture.height(lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        Base::InTextures intextures{0};
        Base::OutTextures outtextures{fpose_texture.MapWrite(lvl),
                                      kfpose_texture.MapWrite(lvl),
                                      bcid_texture.MapWrite(lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};


// -----------------------------------------------------------------------------
// DepthRendererCPU
//   Example derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

class DepthRendererCPU
    : public RendererBaseCPU<DepthRendererBase<TextureViewWriteCPU>>
{
public:
    using Base = DepthRendererBase<TextureViewWriteCPU>;

    DepthRendererCPU() = default;
    ~DepthRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int out_lvl,
                TextureCPU<float> &out_texture)
    {
        Base::Uniforms uniforms;
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{0};
        Base::OutTextures outtextures{out_texture.MapWrite(out_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

// -----------------------------------------------------------------------------
// ImageRendererCPU
//   Another example derived class that might output color
// -----------------------------------------------------------------------------

class ImageRendererCPU
    : public RendererBaseCPU<ImageRendererBase<TextureViewReadCPU,
                                               TextureViewWriteCPU>>
{
public:
    using Base = ImageRendererBase<TextureViewReadCPU,
                                   TextureViewWriteCPU>;

    ImageRendererCPU() = default;
    ~ImageRendererCPU() = default;

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

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class DIDxyRendererCPU
    : public RendererBaseCPU<DIDxyRendererBase<TextureViewReadCPU,
                                               TextureViewWriteCPU>>
{
public:
    using Base = DIDxyRendererBase<TextureViewReadCPU,
                                   TextureViewWriteCPU>;

    DIDxyRendererCPU() = default;
    ~DIDxyRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &in_texture,
                TextureCPU<Vec3<float>> &out_texture)
    {
        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;

        Base::InTextures intextures{in_texture.MapRead(in_lvl)};
        Base::OutTextures outtextures{out_texture.MapWrite(in_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class DIDexpRendererCPU
    : public RendererBaseCPU<DIDexpRendererBase<TextureViewReadCPU,
                                                TextureViewWriteCPU>>
{
public:
    using Base = DIDexpRendererBase<TextureViewReadCPU,
                                    TextureViewWriteCPU>;

    DIDexpRendererCPU() = default;
    ~DIDexpRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const Vec2<float> &exposure,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &in_texture,
                TextureCPU<Vec3<float>> &out_texture)
    {
        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;
        uniforms.exposure = exposure;

        Base::InTextures intextures{in_texture.MapRead(in_lvl)};
        Base::OutTextures outtextures{out_texture.MapWrite(out_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class JPoseFDRendererCPU
{
public:
    JPoseFDRendererCPU() = default;
    ~JPoseFDRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<ImageType> &image_texture,
                TextureCPU<Vec3<float>> &jtra_texture,
                TextureCPU<Vec3<float>> &jrot_texture)
    {
        jtra_texture.fill(out_lvl, jtra_texture.nodata());
        jrot_texture.fill(out_lvl, jrot_texture.nodata());
        image_texture.fill(out_lvl, image_texture.nodata());

        int W = kf_texture.width(0);
        int H = kf_texture.height(0);

        TextureCPU<ImageType> image_1(W, H, 0);
        TextureCPU<ImageType> image_2(W, H, 0);

        auto image_1_view = image_1.MapRead(in_lvl);
        auto image_2_view = image_2.MapRead(in_lvl);
        auto jtra_view = jtra_texture.MapWrite(out_lvl);
        auto jrot_view = jrot_texture.MapWrite(out_lvl);

        for (int i = 0; i < 6; i++)
        {
            float delta;

            if (i < 3)
                delta = 1e-4;
            else
                delta = 1e-3;

            Vec6<float> inc(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
            inc(i) = delta;

            SE3<float> pose_transform_1 = SE3<float>::exp(inc) * pose;
            SE3<float> pose_transform_2 = SE3<float>::exp(-inc) * pose;

            image_renderer.Render(mesh,
                                  pose_transform_1,
                                  Vec2<float>(0.0, 0.0),
                                  cam,
                                  in_lvl, out_lvl,
                                  kf_texture, image_1);
            image_renderer.Render(mesh,
                                  pose_transform_2,
                                  Vec2<float>(0.0, 0.0),
                                  cam,
                                  in_lvl, out_lvl,
                                  kf_texture, image_2);

            for (int y = 0; y < image_1_view.height(); y++)
            {
                for (int x = 0; x < image_1_view.width(); x++)
                {
                    // ImageType kf = image_0.texel_(y, x, out_lvl);
                    // ImageType f = f_texture.texel_(y, x, out_lvl);
                    ImageType data_1 = image_1_view(y, x);
                    ImageType data_2 = image_2_view(y, x);

                    if (data_1 == image_1_view.nodata() || data_2 == image_2_view.nodata())
                        continue;

                    RealType der = -(RealType(data_1) - RealType(data_2)) / (2 * delta);

                    // if (diff == image_diff.nodata())
                    //     continue;

                    // RealType der = diff / (2 * delta);

                    // RealType e = RealType(f) - RealType(kf);
                    // r_texture.set_texel_(e, y, x, out_lvl);

                    if (i < 3)
                    {
                        Vec3<float> data = jtra_view(y, x);
                        data(i) = der;
                        jtra_view(y, x) = data;
                    }
                    else
                    {
                        Vec3<float> data = jrot_view(y, x);
                        data(i - 3) = der;
                        jrot_view(y, x) = data;
                    }
                }
            }
        }
    }

private:
    ImageRendererCPU image_renderer;
};

class JPoseExpRendererCPU
    : public RendererBaseCPU<JPoseExpRendererBase<TextureViewReadCPU,
                                                  TextureViewWriteCPU>>
{
public:
    using Base = JPoseExpRendererBase<TextureViewReadCPU,
                                      TextureViewWriteCPU>;

    JPoseExpRendererCPU() = default;
    ~JPoseExpRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<ImageType> &image_texture,
                TextureCPU<Vec3<float>> &jtra_texture,
                TextureCPU<Vec3<float>> &jrot_texture,
                TextureCPU<Vec3<float>> &jexp_texture)
    {
        const int W = static_cast<int>(image_texture.width(out_lvl));
        const int H = static_cast<int>(image_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uniforms.camera = cam;
        uniforms.exposure = exposure;
        uniforms.out_width = W;
        uniforms.out_height = H;

        Base::InTextures intextures{kf_texture.MapRead(in_lvl),
                                    dfdxy_texture.MapRead(in_lvl)};
        Base::OutTextures outtextures{jtra_texture.MapWrite(out_lvl),
                                      jrot_texture.MapWrite(out_lvl),
                                      jexp_texture.MapWrite(out_lvl),
                                      image_texture.MapWrite(out_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class JPoseVelExpRendererCPU
    : public RendererBaseCPU<JPoseVelExpRendererBase<TextureViewReadCPU,
                                                     TextureViewWriteCPU>>
{
public:
    using Base = JPoseVelExpRendererBase<TextureViewReadCPU,
                                         TextureViewWriteCPU>;

    JPoseVelExpRendererCPU() = default;
    ~JPoseVelExpRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec6<float> &vel,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                const float readout_time,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<ImageType> &image_texture,
                TextureCPU<Vec3<float>> &jtra_texture,
                TextureCPU<Vec3<float>> &jrot_texture,
                TextureCPU<Vec3<float>> &jtravel_texture,
                TextureCPU<Vec3<float>> &jrotvel_texture,
                TextureCPU<Vec3<float>> &jexp_texture)
    {
        const int W = static_cast<int>(image_texture.width(out_lvl));
        const int H = static_cast<int>(image_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uniforms.camera = cam;
        uniforms.vel_matrix = vel;
        uniforms.exposure = exposure;
        uniforms.readout_time = readout_time;
        uniforms.out_width = W;
        uniforms.out_height = H;

        Base::InTextures intextures{kf_texture.MapRead(in_lvl),
                                    dfdxy_texture.MapRead(in_lvl)};
        Base::OutTextures outtextures{jtra_texture.MapWrite(out_lvl),
                                      jrot_texture.MapWrite(out_lvl),
                                      jtravel_texture.MapWrite(out_lvl),
                                      jrotvel_texture.MapWrite(out_lvl),
                                      jexp_texture.MapWrite(out_lvl),
                                      image_texture.MapWrite(out_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class PidsRendererCPU
    : public RendererBaseCPU<PidsRendererBase<TextureViewWriteCPU>>
{
public:
    using Base = PidsRendererBase<TextureViewWriteCPU>;

    PidsRendererCPU() = default;
    ~PidsRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int out_lvl,
                TextureCPU<Vec3<PidType>> &pids_texture)
    {
        const int W = static_cast<int>(pids_texture.width(out_lvl));
        const int H = static_cast<int>(pids_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        Base::InTextures intextures{0};
        Base::OutTextures outtextures{pids_texture.MapWrite(out_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class JDepthFDRendererCPU
{
public:
    JDepthFDRendererCPU() = default;
    ~JDepthFDRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<ImageType> &image_texture,
                TextureCPU<Vec3<float>> &jdepth_texture,
                TextureCPU<Vec3<PidType>> &pids_texture)
    {
        jdepth_texture.fill(out_lvl, jdepth_texture.nodata());
        pids_texture.fill(out_lvl, pids_texture.nodata());
        image_texture.fill(out_lvl, image_texture.nodata());

        const int W = kf_texture.width(0);
        const int H = kf_texture.height(0);

        // TextureCPU<ImageType> image_0(W, H, 0);
        TextureCPU<ImageType> image_1(W, H, 0);
        TextureCPU<ImageType> image_2(W, H, 0);

        auto image_1_view = image_1.MapRead(in_lvl);
        auto image_2_view = image_2.MapRead(in_lvl);
        auto jdepth_view = jdepth_texture.MapWrite(out_lvl);
        auto pids_view = pids_texture.MapRead(out_lvl);

        // image_renderer.Render(mesh, pose, Vec2<float>(0.0, 0.0), cam, in_lvl, out_lvl, kf_texture, image_0);

        pids_renderer.Render(mesh, pose, cam, out_lvl, pids_texture);

        std::vector<Vec3<float>> vertices = get_vertices(mesh);

        MeshCPU mesh_delta(mesh);

        float delta = 1e-3;

        for (int i = 0; i < mesh.vertex_count(); i++)
        {
            Vec3<float> vertex = vertices[i];
            Vec3<float> ray = vertex / vertex(2);
            float depth = vertex(2);

            Vec3<float> vertex_p;
            Vec3<float> vertex_m;

            float depth_p = depth - delta;
            float depth_m = depth + delta;

            vertex_p = ray * depth_p;
            vertex_m = ray * depth_m;

            vertices[i] = vertex_p;

            set_vertices(mesh_delta, vertices);

            image_renderer.Render(mesh_delta,
                                  pose,
                                  Vec2<float>(0.0, 0.0),
                                  cam,
                                  in_lvl, out_lvl,
                                  kf_texture, image_1);

            vertices[i] = vertex_m;

            set_vertices(mesh_delta, vertices);

            image_renderer.Render(mesh_delta,
                                  pose,
                                  Vec2<float>(0.0, 0.0),
                                  cam,
                                  in_lvl, out_lvl,
                                  kf_texture, image_2);

            vertices[i] = vertex;

            set_vertices(mesh_delta, vertices);

            for (int y = 0; y < image_1_view.height(); y++)
            {
                for (int x = 0; x < image_1_view.width(); x++)
                {
                    // ImageType f = f_texture.texel_(y, x, out_lvl);
                    // ImageType kf = image_0.texel_(y, x, out_lvl);
                    ImageType data_1 = image_1_view(y, x);
                    ImageType data_2 = image_2_view(y, x);

                    if (data_1 == image_1_view.nodata() || data_2 == image_2_view.nodata())
                        continue;

                    // RealType e = RealType(f) - RealType(kf);

                    // r_texture.set_texel_(e, y, x, out_lvl);

                    RealType der = (RealType(data_1) - RealType(data_2)) / (2 * delta);

                    if (der == RealType(0.0))
                        continue;

                    Vec3<float> data = jdepth_view(y, x);

                    Vec3<PidType> pids = pids_view(y, x);

                    for (int k = 0; k < 3; k++)
                    {
                        if (pids(k) == i)
                        {
                            data(k) = der;
                        }
                    }

                    jdepth_view(y, x) = data;
                }
            }
        }
    }

private:
    ImageRendererCPU image_renderer;
    PidsRendererCPU pids_renderer;
};

class JDepthExpRendererCPU
    : public RendererBaseCPU<JDepthExpRendererBase<TextureViewReadCPU,
                                                   TextureViewWriteCPU>>
{
public:
    using Base = JDepthExpRendererBase<TextureViewReadCPU,
                                       TextureViewWriteCPU>;

    JDepthExpRendererCPU() = default;
    ~JDepthExpRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<ImageType> &image_texture,
                TextureCPU<Vec3<float>> &jdepth_texture,
                TextureCPU<Vec3<float>> &jexp_texture,
                TextureCPU<Vec3<PidType>> &pids_texture)
    {
        const int W = static_cast<int>(image_texture.width(out_lvl));
        const int H = static_cast<int>(image_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uniforms.camera = cam;
        uniforms.out_width = W;
        uniforms.out_height = H;
        uniforms.exposure = exposure;

        Base::InTextures intextures{kf_texture.MapRead(in_lvl),
                                    dfdxy_texture.MapRead(in_lvl)};
        Base::OutTextures outtextures{jdepth_texture.MapWrite(out_lvl),
                                      jexp_texture.MapWrite(out_lvl),
                                      pids_texture.MapWrite(out_lvl),
                                      image_texture.MapWrite(out_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class JRayDepthExpRendererCPU
    : public RendererBaseCPU<JRayDepthExpRendererBase<TextureViewReadCPU,
                                                      TextureViewWriteCPU>>
{
public:
    using Base = JRayDepthExpRendererBase<TextureViewReadCPU,
                                          TextureViewWriteCPU>;

    JRayDepthExpRendererCPU() = default;
    ~JRayDepthExpRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<ImageType> &image_texture,
                TextureCPU<Vec3<float>> &jdepth_texture,
                TextureCPU<Vec3<float>> &jray0_texture,
                TextureCPU<Vec3<float>> &jray1_texture,
                TextureCPU<Vec3<float>> &jray2_texture,
                TextureCPU<Vec3<float>> &jexp_texture,
                TextureCPU<Vec3<PidType>> &pids_texture)
    {
        const int W = static_cast<int>(image_texture.width(out_lvl));
        const int H = static_cast<int>(image_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uniforms.camera = cam;
        uniforms.out_width = W;
        uniforms.out_height = H;
        uniforms.exposure = exposure;

        Base::InTextures intextures{kf_texture.MapRead(in_lvl),
                                    dfdxy_texture.MapRead(in_lvl)};
        Base::OutTextures outtextures{jdepth_texture.MapWrite(out_lvl),
                                      jray0_texture.MapWrite(out_lvl),
                                      jray1_texture.MapWrite(out_lvl),
                                      jray2_texture.MapWrite(out_lvl),
                                      jexp_texture.MapWrite(out_lvl),
                                      pids_texture.MapWrite(out_lvl),
                                      image_texture.MapWrite(out_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class JVertexExpRendererCPU
    : public RendererBaseCPU<JVertexExpRendererBase<TextureViewReadCPU,
                                                    TextureViewWriteCPU>>
{
public:
    using Base = JVertexExpRendererBase<TextureViewReadCPU,
                                        TextureViewWriteCPU>;

    JVertexExpRendererCPU() = default;
    ~JVertexExpRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<ImageType> &image_texture,
                TextureCPU<Vec3<float>> &jv0_texture,
                TextureCPU<Vec3<float>> &jv1_texture,
                TextureCPU<Vec3<float>> &jv2_texture,
                TextureCPU<Vec3<float>> &jexp_texture,
                TextureCPU<Vec3<PidType>> &pids_texture)
    {
        const int W = static_cast<int>(image_texture.width(out_lvl));
        const int H = static_cast<int>(image_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uniforms.inv_rot_matrix = pose.so3().matrix().transpose();
        uniforms.camera = cam;
        uniforms.out_width = W;
        uniforms.out_height = H;
        uniforms.exposure = exposure;

        Base::InTextures intextures{kf_texture.MapRead(in_lvl),
                                    dfdxy_texture.MapRead(in_lvl)};
        Base::OutTextures outtextures{jv0_texture.MapWrite(out_lvl),
                                      jv1_texture.MapWrite(out_lvl),
                                      jv2_texture.MapWrite(out_lvl),
                                      jexp_texture.MapWrite(out_lvl),
                                      pids_texture.MapWrite(out_lvl),
                                      image_texture.MapWrite(out_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class JPoseExpDepthRendererCPU
    : public RendererBaseCPU<JPoseExpDepthRendererBase<TextureViewReadCPU,
                                                       TextureViewWriteCPU>>
{
public:
    using Base = JPoseExpDepthRendererBase<TextureViewReadCPU,
                                           TextureViewWriteCPU>;

    JPoseExpDepthRendererCPU() = default;
    ~JPoseExpDepthRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<ImageType> &image_texture,
                TextureCPU<Vec3<float>> &jtra_texture,
                TextureCPU<Vec3<float>> &jrot_texture,
                TextureCPU<Vec3<float>> &jexp_texture,
                TextureCPU<Vec3<float>> &jdepth_texture,
                TextureCPU<Vec3<PidType>> &pids_texture)
    {
        const int W = static_cast<int>(image_texture.width(out_lvl));
        const int H = static_cast<int>(image_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uniforms.camera = cam;
        uniforms.exposure = exposure;
        uniforms.out_width = W;
        uniforms.out_height = H;

        Base::InTextures intextures{kf_texture.MapRead(in_lvl),
                                    dfdxy_texture.MapRead(in_lvl)};
        Base::OutTextures outtextures{jtra_texture.MapWrite(out_lvl),
                                      jrot_texture.MapWrite(out_lvl),
                                      jexp_texture.MapWrite(out_lvl),
                                      jdepth_texture.MapWrite(out_lvl),
                                      pids_texture.MapWrite(out_lvl),
                                      image_texture.MapWrite(out_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class JPoseVelExpDepthRendererCPU
    : public RendererBaseCPU<JPoseVelExpDepthRendererBase<TextureViewReadCPU,
                                                          TextureViewWriteCPU>>
{
public:
    using Base = JPoseVelExpDepthRendererBase<TextureViewReadCPU,
                                              TextureViewWriteCPU>;

    JPoseVelExpDepthRendererCPU() = default;
    ~JPoseVelExpDepthRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec6<float> &vel,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                const float readout_time,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<ImageType> &image_texture,
                TextureCPU<Vec3<float>> &jtra_texture,
                TextureCPU<Vec3<float>> &jrot_texture,
                TextureCPU<Vec3<float>> &jtravel_texture,
                TextureCPU<Vec3<float>> &jrotvel_texture,
                TextureCPU<Vec3<float>> &jexp_texture,
                TextureCPU<Vec3<float>> &jdepth_texture,
                TextureCPU<Vec3<PidType>> &pids_texture)
    {
        const int W = static_cast<int>(image_texture.width(out_lvl));
        const int H = static_cast<int>(image_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uniforms.camera = cam;
        uniforms.vel_matrix = vel;
        uniforms.readout_time = readout_time;
        uniforms.exposure = exposure;
        uniforms.out_width = W;
        uniforms.out_height = H;

        Base::InTextures intextures{kf_texture.MapRead(in_lvl),
                                    dfdxy_texture.MapRead(in_lvl)};
        Base::OutTextures outtextures{jtra_texture.MapWrite(out_lvl),
                                      jrot_texture.MapWrite(out_lvl),
                                      jtravel_texture.MapWrite(out_lvl),
                                      jrotvel_texture.MapWrite(out_lvl),
                                      jexp_texture.MapWrite(out_lvl),
                                      jdepth_texture.MapWrite(out_lvl),
                                      pids_texture.MapWrite(out_lvl),
                                      image_texture.MapWrite(out_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class DiffRendererCPU
    : public RendererBaseCPU<DiffRendererBase<TextureViewReadCPU,
                                              TextureViewWriteCPU>>
{
public:
    using Base = DiffRendererBase<TextureViewReadCPU,
                                  TextureViewWriteCPU>;

    DiffRendererCPU() = default;
    ~DiffRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &diffuse_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<ImageType> &image_texture,
                TextureCPU<Vec3<float>> &jtra_texture,
                TextureCPU<Vec3<float>> &jrot_texture,
                TextureCPU<Vec3<float>> &jexp_texture,
                TextureCPU<Vec3<float>> &jdepth_texture,
                TextureCPU<Vec3<PidType>> &pids_texture)
    {
        const int W = static_cast<int>(image_texture.width(out_lvl));
        const int H = static_cast<int>(image_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uniforms.exposure = exposure;
        uniforms.out_width = W;
        uniforms.out_height = H;

        Base::InTextures intextures{diffuse_texture.MapRead(in_lvl),
                                    dfdxy_texture.MapRead(in_lvl)};
        Base::OutTextures outtextures{jtra_texture.MapWrite(out_lvl),
                                      jrot_texture.MapWrite(out_lvl),
                                      jexp_texture.MapWrite(out_lvl),
                                      jdepth_texture.MapWrite(out_lvl),
                                      pids_texture.MapWrite(out_lvl),
                                      image_texture.MapWrite(out_lvl)};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};
