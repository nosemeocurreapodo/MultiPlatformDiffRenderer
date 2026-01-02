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

static void DepthRendererRef(const TextureCPU<float> &depth_texture,
                             const SE3<float> &pose,
                             const PinholeCamera<float> &cam,
                             int out_lvl,
                             TextureCPU<float> &out_texture)
{
    out_texture.fill(out_lvl, out_texture.nodata());

    for (int y = 0; y < out_texture.height(out_lvl); y++)
    {
        for (int x = 0; x < out_texture.width(out_lvl); x++)
        {
            float kf_depth = depth_texture.texel_(y, x, out_lvl);
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
            float prev_depth = out_texture.texel_(f_pix(1), f_pix(0), out_lvl);
            if (prev_depth == out_texture.nodata() || (f_depth < prev_depth))
                out_texture.set_texel_(f_depth, f_pix(1), f_pix(0), out_lvl);
        }
    }
}

static void ImageRendererRef(const TextureCPU<float> &depth_texture,
                             const TextureCPU<ImageType> &image_texture,
                             const SE3<float> &pose,
                             const PinholeCamera<float> &cam,
                             int out_lvl,
                             TextureCPU<ImageType> &out_texture)
{
    out_texture.fill(out_lvl, out_texture.nodata());

    for (int y = 0; y < out_texture.height(out_lvl); y++)
    {
        for (int x = 0; x < out_texture.width(out_lvl); x++)
        {
            float kf_depth = depth_texture.texel_(y, x, out_lvl);
            ImageType kf = image_texture.texel_(y, x, out_lvl);
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
            out_texture.set_texel_(kf, f_pix(1), f_pix(0), out_lvl);
        }
    }
}

template <class Base>
class RendererBaseCPU : public RendererBase<Base>
{
public:
    using Fragment = typename Base::Fragment;

    template <typename Mesh, typename Uniforms, typename InTextures, typename OutTextures>
    void RenderNaive(const BoundingBox<int> &viewport,
                     const Mesh &mesh,
                     const Uniforms &uniforms,
                     InTextures &intextures,
                     OutTextures &outtextures)
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

        // Loop over triangles
        for (unsigned int i = 0; i + 2 < mesh.ebo_buffer_.size(); i += 3)
        {
            int vertexids[3];
            vertexids[0] = mesh.ebo_buffer_[i + 0];
            vertexids[1] = mesh.ebo_buffer_[i + 1];
            vertexids[2] = mesh.ebo_buffer_[i + 2];

            typename Base::VertexData vertexdata[3];
            vertexdata[0] = Base::get_vertex_data(mesh, vertexids[0]);
            vertexdata[1] = Base::get_vertex_data(mesh, vertexids[1]);
            vertexdata[2] = Base::get_vertex_data(mesh, vertexids[2]);

            typename RendererBase<Base>::Triangle triangle;
            this->create_triangle_(vertexdata, vertexids, viewport, uniforms, triangle);

            this->draw_triangle_(triangle, viewport, depth_buffer, uniforms, intextures, fragment_buffer);
        }

        Base::sync_outtextures(outtextures, viewport, fragment_buffer, uniforms);
    }

private:
    std::vector<Fragment> fragment_buffer_;
    std::vector<float> depth_buffer_;
};

// -----------------------------------------------------------------------------
// DepthRendererCPU
//   Example derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

class DepthRendererCPU
    : public RendererBaseCPU<DepthRendererBase<TextureCPU>>
{
public:
    using Base = DepthRendererBase<TextureCPU>;

    DepthRendererCPU() = default;
    ~DepthRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int out_lvl,
                TextureCPU<float> &out_texture)
    {
        Mat4<float> opencv2opengl = Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.out_lvl = out_lvl;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{0};
        Base::OutTextures outtextures{out_texture};

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
    : public RendererBaseCPU<ImageRendererBase<TextureCPU>>
{
public:
    using Base = ImageRendererBase<TextureCPU>;

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
        Mat4<float> opencv2opengl = Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;
        uniforms.exposure = exposure;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{diffuse_texture};
        Base::OutTextures outtextures{out_texture};

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
// ResidualRendererCPU
//   Example derived renderer that computes the residual between two frames.
// -----------------------------------------------------------------------------

class ResidualRendererCPU
    : public RendererBaseCPU<ResidualRendererBase<TextureCPU>>
{
public:
    using Base = ResidualRendererBase<TextureCPU>;

    ResidualRendererCPU() = default;
    ~ResidualRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<ImageType> &f_texture,
                TextureCPU<float> &r_texture)
    {
        Mat4<float> opencv2opengl = Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;
        uniforms.exposure = exposure;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{kf_texture, f_texture};
        Base::OutTextures outtextures{r_texture};

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
    : public RendererBaseCPU<DIDxyRendererBase<TextureCPU>>
{
public:
    using Base = DIDxyRendererBase<TextureCPU>;

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

        Base::InTextures intextures{in_texture};
        Base::OutTextures outtextures{out_texture};

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
    : public RendererBaseCPU<DIDexpRendererBase<TextureCPU>>
{
public:
    using Base = DIDexpRendererBase<TextureCPU>;

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

        Base::InTextures intextures{in_texture};
        Base::OutTextures outtextures{out_texture};

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
                const TextureCPU<ImageType> &f_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<Vec3<float>> &jtra_texture,
                TextureCPU<Vec3<float>> &jrot_texture,
                TextureCPU<float> &r_texture)
    {
        jtra_texture.fill(out_lvl, jtra_texture.nodata());
        jrot_texture.fill(out_lvl, jrot_texture.nodata());
        r_texture.fill(out_lvl, r_texture.nodata());

        int W = kf_texture.width(0);
        int H = kf_texture.height(0);

        TextureCPU<ImageType> image_1(W, H, 0);
        TextureCPU<ImageType> image_2(W, H, 0);

        residual_renderer.Render(mesh, pose,
                                 Vec2<float>(0.0, 0.0),
                                 cam,
                                 in_lvl, out_lvl,
                                 kf_texture, f_texture, r_texture);

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

            for (int y = 0; y < image_1.height(out_lvl); y++)
            {
                for (int x = 0; x < image_1.width(out_lvl); x++)
                {
                    // ImageType kf = image_0.texel_(y, x, out_lvl);
                    // ImageType f = f_texture.texel_(y, x, out_lvl);
                    ImageType data_1 = image_1.texel_(y, x, out_lvl);
                    ImageType data_2 = image_2.texel_(y, x, out_lvl);

                    if (data_1 == image_1.nodata() || data_2 == image_2.nodata())
                        continue;

                    RealType der = -(RealType(data_1) - RealType(data_2)) / (2 * delta);

                    // if (diff == image_diff.nodata())
                    //     continue;

                    // RealType der = diff / (2 * delta);

                    // RealType e = RealType(f) - RealType(kf);
                    // r_texture.set_texel_(e, y, x, out_lvl);

                    if (i < 3)
                    {
                        Vec3<float> data = jtra_texture.texel_(y, x, out_lvl);
                        data(i) = der;
                        jtra_texture.set_texel_(data, y, x, out_lvl);
                    }
                    else
                    {
                        Vec3<float> data = jrot_texture.texel_(y, x, out_lvl);
                        data(i - 3) = der;
                        jrot_texture.set_texel_(data, y, x, out_lvl);
                    }
                }
            }
        }
    }

private:
    ImageRendererCPU image_renderer;
    ResidualRendererCPU residual_renderer;
};

class JPoseExpRendererCPU
    : public RendererBaseCPU<JPoseExpRendererBase<TextureCPU>>
{
public:
    using Base = JPoseExpRendererBase<TextureCPU>;

    JPoseExpRendererCPU() = default;
    ~JPoseExpRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<ImageType> &f_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<Vec3<float>> &jtra_texture,
                TextureCPU<Vec3<float>> &jrot_texture,
                TextureCPU<Vec3<float>> &jexp_texture,
                TextureCPU<float> &r_texture)
    {
        Mat4<float> opencv2opengl = Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.exposure = exposure;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;
        uniforms.out_width = W;
        uniforms.out_height = H;

        Base::InTextures intextures{kf_texture, f_texture, dfdxy_texture};
        Base::OutTextures outtextures{jtra_texture, jrot_texture, jexp_texture, r_texture};

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
    : public RendererBaseCPU<JPoseVelExpRendererBase<TextureCPU>>
{
public:
    using Base = JPoseVelExpRendererBase<TextureCPU>;

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
                const TextureCPU<ImageType> &f_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<Vec3<float>> &jtra_texture,
                TextureCPU<Vec3<float>> &jrot_texture,
                TextureCPU<Vec3<float>> &jtravel_texture,
                TextureCPU<Vec3<float>> &jrotvel_texture,
                TextureCPU<Vec3<float>> &jexp_texture,
                TextureCPU<float> &r_texture)
    {
        Mat4<float> opencv2opengl = Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.vel_matrix = vel;
        uniforms.exposure = exposure;
        uniforms.readout_time = readout_time;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;
        uniforms.out_width = W;
        uniforms.out_height = H;

        Base::InTextures intextures{kf_texture, f_texture, dfdxy_texture};
        Base::OutTextures outtextures{jtra_texture, jrot_texture, jtravel_texture, jrotvel_texture, jexp_texture, r_texture};

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
    : public RendererBaseCPU<PidsRendererBase<TextureCPU>>
{
public:
    using Base = PidsRendererBase<TextureCPU>;

    PidsRendererCPU() = default;
    ~PidsRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int out_lvl,
                TextureCPU<Vec3<PidType>> &pids_texture)
    {
        Mat4<float> opencv2opengl = Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        const int W = static_cast<int>(pids_texture.width(out_lvl));
        const int H = static_cast<int>(pids_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.out_lvl = out_lvl;

        Base::InTextures intextures{0};
        Base::OutTextures outtextures{pids_texture};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class JMapFDRendererCPU
{
public:
    JMapFDRendererCPU() = default;
    ~JMapFDRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<ImageType> &f_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<Vec3<float>> &jmap_texture,
                TextureCPU<Vec3<PidType>> &pids_texture,
                TextureCPU<float> &r_texture)
    {
        jmap_texture.fill(out_lvl, jmap_texture.nodata());
        pids_texture.fill(out_lvl, pids_texture.nodata());
        r_texture.fill(out_lvl, r_texture.nodata());

        const int W = kf_texture.width(0);
        const int H = kf_texture.height(0);

        // TextureCPU<ImageType> image_0(W, H, 0);
        TextureCPU<ImageType> image_1(W, H, 0);
        TextureCPU<ImageType> image_2(W, H, 0);

        // image_renderer.Render(mesh, pose, Vec2<float>(0.0, 0.0), cam, in_lvl, out_lvl, kf_texture, image_0);
        residual_renderer.Render(mesh,
                                 pose,
                                 Vec2<float>(0.0, 0.0),
                                 cam,
                                 in_lvl, out_lvl,
                                 kf_texture, f_texture, r_texture);
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

            for (int y = 0; y < image_1.height(out_lvl); y++)
            {
                for (int x = 0; x < image_1.width(out_lvl); x++)
                {
                    // ImageType f = f_texture.texel_(y, x, out_lvl);
                    // ImageType kf = image_0.texel_(y, x, out_lvl);
                    ImageType data_1 = image_1.texel_(y, x, out_lvl);
                    ImageType data_2 = image_2.texel_(y, x, out_lvl);

                    if (data_1 == image_1.nodata() || data_2 == image_2.nodata())
                        continue;

                    // RealType e = RealType(f) - RealType(kf);

                    // r_texture.set_texel_(e, y, x, out_lvl);

                    RealType der = (RealType(data_1) - RealType(data_2)) / (2 * delta);

                    if (der == RealType(0.0))
                        continue;

                    Vec3<float> data = jmap_texture.texel_(y, x, out_lvl);

                    Vec3<PidType> pids = pids_texture.texel_(y, x, out_lvl);

                    for (int k = 0; k < 3; k++)
                    {
                        if (pids(k) == i)
                        {
                            data(k) = der;
                        }
                    }

                    jmap_texture.set_texel_(data, y, x, out_lvl);
                }
            }
        }
    }

private:
    ImageRendererCPU image_renderer;
    ResidualRendererCPU residual_renderer;
    PidsRendererCPU pids_renderer;
};

class JMapExpRendererCPU
    : public RendererBaseCPU<JMapExpRendererBase<TextureCPU>>
{
public:
    using Base = JMapExpRendererBase<TextureCPU>;

    JMapExpRendererCPU() = default;
    ~JMapExpRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<ImageType> &f_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<Vec3<float>> &jmap_texture,
                TextureCPU<Vec3<float>> &jexp_texture,
                TextureCPU<Vec3<PidType>> &pids_texture,
                TextureCPU<float> &r_texture)
    {
        Mat4<float> opencv2opengl = Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;
        uniforms.out_width = W;
        uniforms.out_height = H;
        uniforms.exposure = exposure;

        Base::InTextures intextures{kf_texture, f_texture, dfdxy_texture};
        Base::OutTextures outtextures{jmap_texture, jexp_texture, pids_texture, r_texture};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class JPoseExpMapRendererCPU
    : public RendererBaseCPU<JPoseExpMapRendererBase<TextureCPU>>
{
public:
    using Base = JPoseExpMapRendererBase<TextureCPU>;

    JPoseExpMapRendererCPU() = default;
    ~JPoseExpMapRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<ImageType> &f_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<Vec3<float>> &jtra_texture,
                TextureCPU<Vec3<float>> &jrot_texture,
                TextureCPU<Vec3<float>> &jexp_texture,
                TextureCPU<Vec3<float>> &jmap_texture,
                TextureCPU<Vec3<PidType>> &pids_texture,
                TextureCPU<float> &r_texture)
    {
        Mat4<float> opencv2opengl = Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.exposure = exposure;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;
        uniforms.out_width = W;
        uniforms.out_height = H;

        Base::InTextures intextures{kf_texture, f_texture, dfdxy_texture};
        Base::OutTextures outtextures{jtra_texture, jrot_texture, jexp_texture, jmap_texture, pids_texture, r_texture};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class JPoseVelExpMapRendererCPU
    : public RendererBaseCPU<JPoseVelExpMapRendererBase<TextureCPU>>
{
public:
    using Base = JPoseVelExpMapRendererBase<TextureCPU>;

    JPoseVelExpMapRendererCPU() = default;
    ~JPoseVelExpMapRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const Vec6<float> &vel,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                const float readout_time,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<ImageType> &f_texture,
                const TextureCPU<Vec3<float>> &dfdxy_texture,
                TextureCPU<Vec3<float>> &jtra_texture,
                TextureCPU<Vec3<float>> &jrot_texture,
                TextureCPU<Vec3<float>> &jtravel_texture,
                TextureCPU<Vec3<float>> &jrotvel_texture,
                TextureCPU<Vec3<float>> &jexp_texture,
                TextureCPU<Vec3<float>> &jmap_texture,
                TextureCPU<Vec3<PidType>> &pids_texture,
                TextureCPU<float> &r_texture)
    {
        Mat4<float> opencv2opengl = Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.vel_matrix = vel;
        uniforms.readout_time = readout_time;
        uniforms.exposure = exposure;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;
        uniforms.out_width = W;
        uniforms.out_height = H;

        Base::InTextures intextures{kf_texture, f_texture, dfdxy_texture};
        Base::OutTextures outtextures{jtra_texture, jrot_texture, jtravel_texture, jrotvel_texture, jexp_texture, jmap_texture, pids_texture, r_texture};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};
