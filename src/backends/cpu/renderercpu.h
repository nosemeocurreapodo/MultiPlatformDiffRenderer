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
            Vec2<float> kf_pix(float(x) / out_texture.width(out_lvl), float(y) / out_texture.height(out_lvl));
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
            f_pix(0) = f_pix(0) * out_texture.width(out_lvl);
            f_pix(1) = f_pix(1) * out_texture.height(out_lvl);
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
            Vec2<float> kf_pix(float(x) / out_texture.width(out_lvl), float(y) / out_texture.height(out_lvl));
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
            f_pix(0) = f_pix(0) * out_texture.width(out_lvl);
            f_pix(1) = f_pix(1) * out_texture.height(out_lvl);
            out_texture.set_texel_(kf, f_pix(1), f_pix(0), out_lvl);
        }
    }
}

template <class Base>
class RendererBaseCPU
    : public RendererBase<Base>
{
public:
    // RendererBaseHLS() = default;
    //~RendererBaseHLS() = default;

    template <typename Mesh, typename Uniforms, typename InTextures, typename OutTextures>
    void RenderNaive(const BoundingBox<int> &viewport,
                     const Mesh &mesh,
                     const Uniforms &uniforms,
                     InTextures &intextures,
                     OutTextures &outtextures)
    {
        // MathType depth_buffer[viewport.width_ * viewport.height_] = {MathType(-1)};
        float depth_buffer[1024 * 1024] = {-1.0f};

    // Loop over triangles
    renderbase_render_triangles_loop:
        for (unsigned int i = 0; i + 2 < mesh.ebo_buffer_.size(); i += 3)
        {
#pragma HLS loop_tripcount min = 768 max = 768 avg = 768

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

            // directly write to dram
            this->draw_triangle_(triangle, viewport, depth_buffer, uniforms, intextures, outtextures);
        }
    }
};

/*
class GouraudRendererCPU
    : public GouraudRendererBase<float, float, MeshCPU, TextureCPU>
{
public:
    GouraudRendererCPU() = default;
    ~GouraudRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                const linalg::Vec3<float> &light_pos,
                const linalg::Vec3<float> &light_color,
                const linalg::Vec3<float> &ambient_reflectance,
                const linalg::Vec3<float> &diffuse_reflectance,
                const linalg::Vec3<float> &specular_reflectance,
                const float shininess,
                const linalg::Vec3<float> &ambient_light,
                unsigned int out_lvl,
                TextureCPU<linalg::Vec3<float>> &out_texture)
    {
        // Validate inputs
        ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        Render(mesh, pose, cam, light_pos, light_color, ambient_reflectance, diffuse_reflectance, specular_reflectance, shininess, ambient_light, out_lvl, out_texture);
    }

private:
    void Render(const MeshCPU &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                const linalg::Vec3<float> &light_pos,
                const linalg::Vec3<float> &light_color,
                const linalg::Vec3<float> &ambient_reflectance,
                const linalg::Vec3<float> &diffuse_reflectance,
                const linalg::Vec3<float> &specular_reflectance,
                const float shininess,
                const linalg::Vec3<float> &ambient_light,
                unsigned int out_lvl,
                TextureCPU<linalg::Vec3<float>> &out_texture)
    {
        linalg::SE3<float> cam2world = pose.inverse();

        // model already in world space
        uModel_ = linalg::Mat4<float>::Identity();
        uView_ = this->opencv2opengl_ * pose.matrix();
        uProjection_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uNormalMatrix_ = linalg::Mat3<float>::Identity(); // linalg::Mat3<MathType>(uModel_).inverse().transpose();

        uLightPos_ = light_pos;
        uViewPos_ = cam2world.translation(); // camera position in world space

        uKa_ = ambient_reflectance;
        uKd_ = diffuse_reflectance;
        uKs_ = specular_reflectance;
        uShininess_ = shininess;
        uLightColor_ = light_color;
        uAmbientLight_ = ambient_light;

        out_lvl_ = out_lvl;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        InTextures intextures{0};
        OutTextures outtextures{out_texture};

        RendererBase<float, GouraudRendererBase>::RenderNaive(viewport, mesh, intextures, outtextures);
    }
};
*/
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
        // Validate inputs
        // ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        out_texture.fill(out_lvl, out_texture.nodata());

        Mat4<float> opencv2opengl = linalg::Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl * pose.matrix();
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
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &diffuse_texture,
                TextureCPU<ImageType> &out_texture)
    {
        // Validate inputs
        // ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        out_texture.fill(out_lvl, out_texture.nodata());

        Mat4<float> opencv2opengl = linalg::Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl * pose.matrix();
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;

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
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &kf_texture,
                const TextureCPU<ImageType> &f_texture,
                TextureCPU<float> &r_texture)
    {
        // Validate inputs
        // ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        r_texture.fill(out_lvl, r_texture.nodata());

        Mat4<float> opencv2opengl = linalg::Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl * pose.matrix();
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;

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
        // Validate inputs
        // ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        out_texture.fill(out_lvl, out_texture.nodata());

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

class JPoseRendererCPU
    : public RendererBaseCPU<JPoseRendererBase<TextureCPU>>
{
public:
    using Base = JPoseRendererBase<TextureCPU>;

    JPoseRendererCPU() = default;
    ~JPoseRendererCPU() = default;

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
        // Validate inputs
        // ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        jtra_texture.fill(out_lvl, jtra_texture.nodata());
        jrot_texture.fill(out_lvl, jrot_texture.nodata());
        r_texture.fill(out_lvl, r_texture.nodata());

        Mat4<float> opencv2opengl = linalg::Mat4<float>::Identity();
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

        Base::InTextures intextures{kf_texture, f_texture, dfdxy_texture};
        Base::OutTextures outtextures{jtra_texture, jrot_texture, r_texture};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};

class JMapRendererCPU
    : public RendererBaseCPU<JMapRendererBase<TextureCPU>>
{
public:
    using Base = JMapRendererBase<TextureCPU>;

    JMapRendererCPU() = default;
    ~JMapRendererCPU() = default;

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
        // Validate inputs
        // ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        jmap_texture.fill(out_lvl, jmap_texture.nodata());
        pids_texture.fill(out_lvl, pids_texture.nodata());
        r_texture.fill(out_lvl, r_texture.nodata());

        linalg::Mat4<float> opencv2opengl = linalg::Mat4<float>::Identity();
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

        Base::InTextures intextures{kf_texture, f_texture, dfdxy_texture};
        Base::OutTextures outtextures{jmap_texture, pids_texture, r_texture};

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
    : public RendererBaseCPU<DiffRendererBase<TextureCPU>>
{
public:
    using Base = DiffRendererBase<TextureCPU>;

    DiffRendererCPU() = default;
    ~DiffRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<ImageType> &diffuse_texture,
                TextureCPU<ImageType> &image_texture,
                TextureCPU<float> &depth_texture,
                TextureCPU<linalg::Vec3<float>> &jtra_texture,
                TextureCPU<linalg::Vec3<float>> &jrot_texture,
                TextureCPU<linalg::Vec3<float>> &jmap_texture,
                TextureCPU<linalg::Vec3<PidType>> &pids_texture)
    {
        // Validate inputs
        // ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        // nodata_.depth = out_texture.nodata();
        image_texture.fill(out_lvl, image_texture.nodata());
        depth_texture.fill(out_lvl, depth_texture.nodata());
        jtra_texture.fill(out_lvl, jtra_texture.nodata());
        jrot_texture.fill(out_lvl, jrot_texture.nodata());
        jmap_texture.fill(out_lvl, jmap_texture.nodata());
        pids_texture.fill(out_lvl, pids_texture.nodata());

        linalg::Mat4<float> opencv2opengl = linalg::Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;

        const int W = static_cast<int>(image_texture.width(out_lvl));
        const int H = static_cast<int>(image_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{diffuse_texture};
        Base::OutTextures outtextures{image_texture, depth_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture};

        RendererBaseCPU<Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

private:
};
