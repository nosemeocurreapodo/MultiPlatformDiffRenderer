#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "backends/base/rendererbase.h"
#include "backends/cpu/devicecpu.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/buffercpu.h"
#include "backends/cpu/meshcpu.h"
#include "core/render_constants.h"
#include "core/error_handling.h"

// -----------------------------------------------------------------------------
// BaseRendererCPU (improved)
// -----------------------------------------------------------------------------
template <class Derived>
class BaseRendererCPU : public BaseRenderer<Derived, int, float, Vec2, Vec3, Vec4, Vec3i, Mat4>
{
public:
    BaseRendererCPU() = default;
    virtual ~BaseRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const BoundingBoxType<int> &viewport)
    {
        // ---- Map mesh buffers (no copies) ----
        auto pos = mesh.MapReadPositions(); // 3 floats/vertex
        auto tex = mesh.MapReadTexcoords(); // 2 floats/vertex
        auto wei = mesh.MapReadWeights();   // 1 float /vertex
        auto idx = mesh.MapReadIndices();   // uint32_t indices

        // Loop over triangles
        for (std::size_t i = 0; i + 2 < idx.size(); i += 3)
        {
            const uint32_t i0 = idx[i + 0];
            const uint32_t i1 = idx[i + 1];
            const uint32_t i2 = idx[i + 2];

            Vec3 v[3];
            Vec2 uv[3];
            float wght[3];
            unsigned int id[3];

            id[0] = i0;
            id[1] = i1;
            id[2] = i2;

            // gather
            for (int k = 0; k < 3; ++k)
            {
                const uint32_t vi = (k == 0 ? i0 : k == 1 ? i1
                                                          : i2);
                v[k](0) = pos[vi * 3 + 0];
                v[k](1) = pos[vi * 3 + 1];
                v[k](2) = pos[vi * 3 + 2];
                uv[k](0) = tex[vi * 2 + 0];
                uv[k](1) = tex[vi * 2 + 1];
                wght[k] = wei[vi];
            }

            this->draw_triangle_(v, uv, wght, id, viewport);
        }
    }
};

// -----------------------------------------------------------------------------
// DepthRendererCPU
//   Example derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

class DepthRendererCPU
    : public BaseRendererCPU<DepthRendererCPU>
{
public:
    struct Varyings
    {
        float depth;
    };

    DepthRendererCPU() = default;
    ~DepthRendererCPU() override = default;

    void Render(const MeshCPU &mesh,
                const SE3 &pose,
                const Camera &cam,
                int out_lvl,
                TextureCPU<float> &out_texture)
    {
        // Validate inputs
        ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        out_texture.fill(out_lvl, out_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl_ * pose.matrix();
        out_lvl_ = out_lvl;
        out_texture_ = &out_texture;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBoxType<int> viewport(0, W, 0, H);

        BaseRendererCPU::Render(mesh, viewport);
    }

    Varyings interpolate_varyings(const float w0, const float w1, const float w2,
                                  const float invW0, const float invW1, const float invW2,
                                  const float invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.depth =
            (w0 * varying_px0.depth * invW0 +
             w1 * varying_px1.depth * invW1 +
             w2 * varying_px2.depth * invW2) *
            (1.0f / invW_px);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const unsigned int &vertexid,
                       Vec4 &gl_Position,
                       Varyings &outVarying)
    {
        gl_Position = t_matrix_ * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        outVarying.depth = inVertex(2);
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Varyings &in_varying)
    {
        out_texture_->set_texel_(in_varying.depth, int(gl_FragCoord(1)), int(gl_FragCoord(0)), out_lvl_);
    }

private:
    Mat4 t_matrix_;
    int out_lvl_;
    TextureCPU<float> *out_texture_;
};

// -----------------------------------------------------------------------------
// ImageRendererCPU
//   Another example derived class that might output color
// -----------------------------------------------------------------------------

class ImageRendererCPU
    : public BaseRendererCPU<ImageRendererCPU>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
    };

    ImageRendererCPU() = default;
    ~ImageRendererCPU() override = default;

    void Render(const MeshCPU &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<float> &in_texture,
                TextureCPU<float> &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl_ * pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        in_texture_ = &in_texture;
        out_texture_ = &out_texture;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBoxType<int> viewport(0, W, 0, H);

        BaseRendererCPU::Render(mesh, viewport);
    }

    Varyings interpolate_varyings(const float w0, const float w1, const float w2,
                                  const float invW0, const float invW1, const float invW2,
                                  const float invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (1.0f / invW_px);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const unsigned int &vertexid,
                       Vec4 &gl_Position,
                       Varyings &outVarying)
    {
        gl_Position = t_matrix_ * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Varyings &in_varying)
    {
        float pix = in_texture_->sample_(in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        if (pix == in_texture_->nodata())
            return;
        out_texture_->set_texel_(pix, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    Mat4 t_matrix_;
    int in_lvl_;
    int out_lvl_;
    const TextureCPU<float> *in_texture_;
    TextureCPU<float> *out_texture_;
};

// -----------------------------------------------------------------------------
// ResidualRendererCPU
//   Example derived renderer that computes the residual between two frames.
// -----------------------------------------------------------------------------

class ResidualRendererCPU
    : public BaseRendererCPU<ResidualRendererCPU>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
    };

    ResidualRendererCPU() = default;
    ~ResidualRendererCPU() override = default;

    void Render(const MeshCPU &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<float> &kf_texture,
                const TextureCPU<float> &f_texture,
                TextureCPU<float> &r_texture)
    {
        r_texture.fill(out_lvl, r_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl_ * pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        kf_texture_ = &kf_texture;
        f_texture_ = &f_texture;
        r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBoxType<int> viewport(0, W, 0, H);

        BaseRendererCPU::Render(mesh, viewport);
    }

    Varyings interpolate_varyings(const float w0, const float w1, const float w2,
                                  const float invW0, const float invW1, const float invW2,
                                  const float invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (1.0f / invW_px);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const unsigned int &vertexid,
                       Vec4 &gl_Position,
                       Varyings &outVarying)
    {
        gl_Position = t_matrix_ * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Varyings &in_varying)
    {
        int width = kf_texture_->width(out_lvl_);
        int height = kf_texture_->height(out_lvl_);

        Vec2 screen_texcoord(gl_FragCoord(0) / float(width), gl_FragCoord(1) / float(height));

        float kf = kf_texture_->sample_(in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        // float f = f_texture_->texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        float f = f_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);

        if (kf == kf_texture_->nodata() || f == f_texture_->nodata())
            return;

        float e = f - kf;
        r_texture_->set_texel_(e, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    Mat4 t_matrix_;
    int in_lvl_;
    int out_lvl_;
    const TextureCPU<float> *kf_texture_;
    const TextureCPU<float> *f_texture_;
    TextureCPU<float> *r_texture_;
};

// -----------------------------------------------------------------------------
// ImageRendererCPU
//   Another example derived class that might output color
// -----------------------------------------------------------------------------

class L2RendererCPU
    : public BaseRendererCPU<L2RendererCPU>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
    };

    L2RendererCPU() = default;
    ~L2RendererCPU() override = default;

    void Render(const MeshCPU &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<float> &kf_texture,
                const TextureCPU<float> &f_texture,
                TextureCPU<float> &r_texture)
    {
        r_texture.fill(out_lvl, r_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl_ * pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        kf_texture_ = &kf_texture;
        f_texture_ = &f_texture;
        r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBoxType<int> viewport(0, W, 0, H);

        BaseRendererCPU::Render(mesh, viewport);
    }

    Varyings interpolate_varyings(const float w0, const float w1, const float w2,
                                  const float invW0, const float invW1, const float invW2,
                                  const float invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (1.0f / invW_px);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const unsigned int &vertexid,
                       Vec4 &gl_Position,
                       Varyings &outVarying)
    {
        gl_Position = t_matrix_ * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Varyings &in_varying)
    {
        int width = kf_texture_->width(out_lvl_);
        int height = kf_texture_->height(out_lvl_);

        Vec2 screen_texcoord(gl_FragCoord(0) / float(width), gl_FragCoord(1) / float(height));

        float kf = kf_texture_->sample_(in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        // float f = f_texture_->texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        float f = f_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);

        if (kf == kf_texture_->nodata() || f == f_texture_->nodata())
            return;

        float e = f - kf;
        r_texture_->set_texel_(e * e, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    Mat4 t_matrix_;
    int in_lvl_;
    int out_lvl_;
    const TextureCPU<float> *kf_texture_;
    const TextureCPU<float> *f_texture_;
    TextureCPU<float> *r_texture_;
};

class DIDxyRendererCPU
    : public BaseRendererCPU<DIDxyRendererCPU>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
    };

    DIDxyRendererCPU() = default;
    ~DIDxyRendererCPU() override = default;

    void Render(const MeshCPU &mesh,
                int in_lvl,
                int out_lvl,
                const TextureCPU<float> &in_texture,
                TextureCPU<Vec3> &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        in_texture_ = &in_texture;
        out_texture_ = &out_texture;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBoxType<int> viewport(0, W, 0, H);

        BaseRendererCPU::Render(mesh, viewport);
    }

    Varyings interpolate_varyings(const float w0, const float w1, const float w2,
                                  const float invW0, const float invW1, const float invW2,
                                  const float invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (1.0f / invW_px);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const unsigned int &vertexid,
                       Vec4 &gl_Position,
                       Varyings &outVarying)
    {
        // gl_Position = (view_matrix * pose_matrix) * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = Vec4(2.0 * inTexCoord(0) - 1.0, 2.0 * inTexCoord(1) - 1.0, 0.0, 1.0);
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Varyings &in_varying)
    {
        // outFragment = inVarying;

        int height = in_texture_->height(out_lvl_);
        int width = in_texture_->width(out_lvl_);
        float nodata = in_texture_->nodata();

        int x = int(in_varying.texcoord(0) * (width - 1));
        int y = int(in_varying.texcoord(1) * (height - 1));
        int x_p = x + 1;
        int x_m = x - 1;
        int y_p = y + 1;
        int y_m = y - 1;

        if (x_p >= width || x_m < 0 || y_p >= height || y_m < 0)
        {
            // No need to explicitly set to nodata, it is already in the background color
            // outFragment(0) = 0.0f;
            // outFragment(1) = 0.0f;
            // outFragment(2) = 0.0f;
            return;
        }

        float f = in_texture_->texel_(y, x, out_lvl_);
        float f_y_p = in_texture_->texel_(y_p, x, out_lvl_);
        float f_y_m = in_texture_->texel_(y_m, x, out_lvl_);
        float f_x_p = in_texture_->texel_(y, x_p, out_lvl_);
        float f_x_m = in_texture_->texel_(y, x_m, out_lvl_);

        if (f_x_p == nodata || f_x_m == nodata ||
            f_y_p == nodata || f_y_m == nodata || f == nodata)
        {
            // No need to explicitly set to nodata, it is already in the background color
            // outFragment(0) = 0.0f;
            // outFragment(1) = 0.0f;
            // outFragment(2) = 0.0f;
            return;
        }

        Vec3 out_fragment;
        out_fragment(0) = (f_x_p - f_x_m) / 2.0f;
        out_fragment(1) = (f_y_p - f_y_m) / 2.0f;
        out_fragment(2) = 0.0; // f; // save the projected frame for later processing

        out_texture_->set_texel_(out_fragment, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    int in_lvl_;
    int out_lvl_;
    const TextureCPU<float> *in_texture_;
    TextureCPU<Vec3> *out_texture_;
};

class JPoseRendererCPU
    : public BaseRendererCPU<JPoseRendererCPU>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
        Vec3 f_ver;
    };

    JPoseRendererCPU() = default;
    ~JPoseRendererCPU() override = default;

    void Render(const MeshCPU &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<float> &kf_texture,
                const TextureCPU<float> &f_texture,
                const TextureCPU<Vec3> &dfdxy_texture,
                TextureCPU<Vec3> &jtra_texture,
                TextureCPU<Vec3> &jrot_texture,
                TextureCPU<float> &r_texture)
    {
        jtra_texture.fill(out_lvl, jtra_texture.nodata());
        jrot_texture.fill(out_lvl, jrot_texture.nodata());
        r_texture.fill(out_lvl, r_texture.nodata());

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        fx_ = cam.GetParams()(0);
        fy_ = cam.GetParams()(1);
        view_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl_;
        pose_matrix_ = pose.matrix();
        kf_texture_ = &kf_texture;
        f_texture_ = &f_texture;
        dfdxy_texture_ = &dfdxy_texture;
        jtra_texture_ = &jtra_texture;
        jrot_texture_ = &jrot_texture;
        r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBoxType<int> viewport(0, W, 0, H);

        BaseRendererCPU::Render(mesh, viewport);
    }

    Varyings interpolate_varyings(const float w0, const float w1, const float w2,
                                  const float invW0, const float invW1, const float invW2,
                                  const float invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (1.0f / invW_px);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver * invW0 +
             w1 * varying_px1.f_ver * invW1 +
             w2 * varying_px2.f_ver * invW2) *
            (1.0f / invW_px);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const unsigned int &vertexid,
                       Vec4 &gl_Position,
                       Varyings &outVarying)
    {
        Vec4 f_ver = pose_matrix_ * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = view_matrix_ * f_ver;

        outVarying.f_ver = Vec3(f_ver(0), f_ver(1), f_ver(2));
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Varyings &in_varying)
    {
        int width = kf_texture_->width(out_lvl_);
        int height = kf_texture_->height(out_lvl_);

        Vec2 screen_texcoord(gl_FragCoord(0) / float(width), gl_FragCoord(1) / float(height));

        Vec3 f_ver = in_varying.f_ver;
        Vec2 texcoord = in_varying.texcoord;

        float kf = kf_texture_->sample_(texcoord(1), texcoord(0), in_lvl_);
        // float f = f_texture_->texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // Vec3 f_der = dfdxy_texture_->texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        float f = f_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);
        Vec3 f_der = dfdxy_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);

        if (kf == kf_texture_->nodata() || f == f_texture_->nodata() || f_der == dfdxy_texture_->nodata())
            return;

        float r = f - kf;

        float v0 = f_der(0) * fx_ * width / f_ver(2);
        float v1 = f_der(1) * fy_ * height / f_ver(2);
        float v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        Vec3 d_f_i_d_tra = Vec3(v0, v1, v2);
        Vec3 d_f_i_d_rot = Vec3(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        jtra_texture_->set_texel_(d_f_i_d_tra, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        jrot_texture_->set_texel_(d_f_i_d_rot, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        r_texture_->set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    int in_lvl_;
    int out_lvl_;
    float fx_;
    float fy_;
    Mat4 view_matrix_;
    Mat4 pose_matrix_;
    const TextureCPU<float> *kf_texture_;
    const TextureCPU<float> *f_texture_;
    const TextureCPU<Vec3> *dfdxy_texture_;
    TextureCPU<Vec3> *jtra_texture_;
    TextureCPU<Vec3> *jrot_texture_;
    TextureCPU<float> *r_texture_;
};

class JMapRendererCPU
    : public BaseRendererCPU<JMapRendererCPU>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
        Vec3 f_ver;
        Vec3 kf_ray;
        float depth;
        Vec3 barycentric;
        unsigned int vertexId;
        Vec3i pids;
    };

    JMapRendererCPU() = default;
    ~JMapRendererCPU() override = default;

    void Render(const MeshCPU &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureCPU<float> &kf_texture,
                const TextureCPU<float> &f_texture,
                const TextureCPU<Vec3> &dfdxy_texture,
                TextureCPU<Vec3> &jmap_texture,
                TextureCPU<Vec3> &pids_texture,
                TextureCPU<float> &r_texture)
    {
        jmap_texture.fill(out_lvl, jmap_texture.nodata());
        pids_texture.fill(out_lvl, pids_texture.nodata());
        r_texture.fill(out_lvl, r_texture.nodata());

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        fx_ = cam.GetParams()(0);
        fy_ = cam.GetParams()(1);
        view_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl_;
        pose_matrix_ = pose.matrix();
        kf_texture_ = &kf_texture;
        f_texture_ = &f_texture;
        dfdxy_texture_ = &dfdxy_texture;
        jmap_texture_ = &jmap_texture;
        pids_texture_ = &pids_texture;
        r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBoxType<int> viewport(0, W, 0, H);

        BaseRendererCPU::Render(mesh, viewport);
    }

    Varyings interpolate_varyings(const float w0, const float w1, const float w2,
                                  const float invW0, const float invW1, const float invW2,
                                  const float invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (1.0f / invW_px);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver * invW0 +
             w1 * varying_px1.f_ver * invW1 +
             w2 * varying_px2.f_ver * invW2) *
            (1.0f / invW_px);
        var_over_w_px.kf_ray =
            (w0 * varying_px0.kf_ray * invW0 +
             w1 * varying_px1.kf_ray * invW1 +
             w2 * varying_px2.kf_ray * invW2) *
            (1.0f / invW_px);
        var_over_w_px.barycentric = Vec3(w0 * invW0 * varying_px0.depth,
                                         w1 * invW1 * varying_px1.depth,
                                         w2 * invW2 * varying_px2.depth) *
                                    (1.0f / invW_px);

        var_over_w_px.pids = Vec3i(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const unsigned int &vertexid,
                       Vec4 &gl_Position,
                       Varyings &outVarying)
    {
        Vec4 f_ver = pose_matrix_ * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = view_matrix_ * f_ver;

        Vec3 kf_ray(inVertex(0) / inVertex(2), inVertex(1) / inVertex(2), 1.0);
        Vec4 d_f_ver_d_kf_depth_ = pose_matrix_ * Vec4(kf_ray(0), kf_ray(1), kf_ray(2), 0.0);
        Vec3 d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = Vec3(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = inVertex(2);
        outVarying.vertexId = vertexid;
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Varyings &in_varying)
    {
        int width = jmap_texture_->width(out_lvl_);
        int height = jmap_texture_->height(out_lvl_);

        Vec2 screen_texcoord(gl_FragCoord(0) / float(width), gl_FragCoord(1) / float(height));

        Vec3 f_ver = in_varying.f_ver;
        Vec3 kf_ray = in_varying.kf_ray;
        Vec2 texcoord = in_varying.texcoord;
        Vec3 barycentric = in_varying.barycentric;
        Vec3i vertexid = in_varying.pids;

        float kf = kf_texture_->sample_(texcoord(1), texcoord(0), in_lvl_);
        // float f = f_texture_->texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // Vec3 f_der = dfdxy_texture_->texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        float f = f_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);
        Vec3 f_der = dfdxy_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);

        if (kf == kf_texture_->nodata() || f == f_texture_->nodata() || f_der == dfdxy_texture_->nodata())
            return;

        float r = f - kf;

        Vec3 d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = f_der(0) * fx_ * width / f_ver(2);
        d_f_i_d_f_ver(1) = f_der(1) * fy_ * height / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // Vec3 d_f_i_d_tra = Vec3(v0, v1, v2);
        // Vec3 d_f_i_d_rot = Vec3(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        Vec3 d_f_ver_d_kf_depth = kf_ray; // kfTofPose.rotationMatrix() * kf_ray;
        float d_f_i_d_kf_depth = d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth;

        Vec3 d_depth_d_vert_depth = barycentric;

        Vec3 jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        Vec3 ids = Vec3(vertexid(0), vertexid(1), vertexid(2));

        jmap_texture_->set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        pids_texture_->set_texel_(ids, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        r_texture_->set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    int in_lvl_;
    int out_lvl_;
    float fx_;
    float fy_;
    Mat4 view_matrix_;
    Mat4 pose_matrix_;
    const TextureCPU<float> *kf_texture_;
    const TextureCPU<float> *f_texture_;
    const TextureCPU<Vec3> *dfdxy_texture_;
    TextureCPU<Vec3> *jmap_texture_;
    TextureCPU<Vec3> *pids_texture_;
    TextureCPU<float> *r_texture_;
};
