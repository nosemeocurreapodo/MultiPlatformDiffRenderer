#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
#include "core/types.h"
#include "core/render_constants.h"
// #include "core/error_handling.h"

template <typename Scalar, typename Vec2>
inline Scalar cross(const Vec2 &a, const Vec2 &b) { return a(0) * b(1) - a(1) * b(0); }

template <typename Scalar, typename Vec2>
inline Scalar triangle_area(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2) { return cross<Scalar, Vec2>(p1 - p0, p2 - p0); }

// Edge function E_ab(p) = (yb-ya)*px + (xa-xb)*py + (xb*ya - xa*yb)
template <typename Scalar>
inline Scalar edge_func(Scalar ax, Scalar ay, Scalar bx, Scalar by, Scalar px, Scalar py)
{
    return (by - ay) * px + (ax - bx) * py + (bx * ay - ax * by);
}

// Top-left test: returns true if edge is a "top" or "left" edge
template <typename Scalar>
inline bool is_top_left(Scalar ax, Scalar ay, Scalar bx, Scalar by)
{
    return (ay == by) ? (bx < ax) : (ay < by);
}

// -----------------------------------------------------------------------------
// RendererBase
// -----------------------------------------------------------------------------
template <class Derived>
class RendererBase
{
public:
    RendererBase()
    {
        opencv2opengl_ = Mat4::Identity();
        opencv2opengl_(2, 2) = -1.0; // flip Z like your original intent
    };
    // virtual ~RendererBase() = default;
    ~RendererBase() = default;

    template <typename Mesh, typename Textures>
    void Render(const Mesh &mesh,
                const BoundingBox<Int> &viewport,
                Textures &textures)
    {
    // ---- Map mesh buffers (no copies) ----
    // auto pos = mesh.MapReadPositions(); // 3 floats/vertex
    // auto tex = mesh.MapReadTexcoords(); // 2 floats/vertex
    // auto wei = mesh.MapReadWeights();   // 1 float /vertex
    // auto idx = mesh.MapReadIndices();   // uint32_t indices

    // const auto pos = mesh.Positions(); // 3 floats/vertex
    // const auto tex = mesh.Texcoords(); // 2 floats/vertex
    // const auto wei = mesh.Weights();   // 1 float /vertex
    // const auto idx = mesh.Indices();   // uint32_t indices

    // Loop over triangles
    renderbase_triangle_loop:
        for (std::size_t i = 0; i + 2 < mesh.ebo_buffer_.size(); i += 3)
        {
            const UInt i0 = mesh.ebo_buffer_[i + 0];
            const UInt i1 = mesh.ebo_buffer_[i + 1];
            const UInt i2 = mesh.ebo_buffer_[i + 2];

            Vec3 v[3];
            Vec2 uv[3];
            Scalar wght[3];
            UInt id[3];

            id[0] = i0;
            id[1] = i1;
            id[2] = i2;

        renderbase_gather_loop:
            for (int k = 0; k < 3; ++k)
            {
                UInt vi = (k == 0 ? i0 : k == 1 ? i1
                                                : i2);
                v[k](0) = mesh.pos_buffer_[vi * 3 + 0];
                v[k](1) = mesh.pos_buffer_[vi * 3 + 1];
                v[k](2) = mesh.pos_buffer_[vi * 3 + 2];
                uv[k](0) = mesh.tex_buffer_[vi * 2 + 0];
                uv[k](1) = mesh.tex_buffer_[vi * 2 + 1];
                wght[k] = mesh.wei_buffer_[vi];
            }

            this->draw_triangle_(v, uv, wght, id, viewport, textures);
        }
    }

protected:
    // Triangle rasterizer (top-left rule, perspective correct)
    template <typename Textures>
    void draw_triangle_(const Vec3 *verts,
                        const Vec2 *texcoords,
                        const Scalar *weights,
                        const UInt *vertexid,
                        const BoundingBox<Int> &viewport,
                        Textures &textures)
    {
        // Vertex shading & clip → NDC → screen
        struct VSOut
        {
            Vec2 screen;  // x,y in pixel space (float)
            Scalar depth; // z in [0,1] if your projection is like GL_ZERO_TO_ONE
            Scalar invW;  // 1 / clip.w
            // std::tuple<Varyings...> var_over_w; // varyings multiplied by invW
            typename Derived::Varyings var; // original varyings (for convenience)
        } vout[3];

        const Scalar vp_w = static_cast<Scalar>(viewport.max_x_ - viewport.min_x_);
        const Scalar vp_h = static_cast<Scalar>(viewport.max_y_ - viewport.min_y_);

    draw_triangle_vertex_loop:
        for (int i = 0; i < 3; ++i)
        {
            Vec4 gl_Position;
            typename Derived::Varyings varyings;
            derived_().vertex_shader(verts[i], texcoords[i], weights[i], vertexid[i], gl_Position, varyings);

            const Scalar invW = 1.0f / gl_Position(3);
            const Scalar ndc_x = gl_Position(0) * invW; // [-1,1]
            const Scalar ndc_y = gl_Position(1) * invW;
            const Scalar ndc_z = gl_Position(2) * invW; // assumed 0..1 after proj (adjust if -1..1)

            // pixel-space (don’t clamp here) — match GL rasterization (remove +1/-0.5 adjustment)
            vout[i].screen(0) = Scalar(0.5) * (ndc_x + Scalar(1)) * vp_w + viewport.min_x_;
            vout[i].screen(1) = Scalar(0.5) * (ndc_y + Scalar(1)) * vp_h + viewport.min_y_;
            vout[i].depth = ndc_z;
            vout[i].invW = invW;
            vout[i].var = varyings;
            // vout[i].var_over_w = varyings * invW; // requires scalar*VaryingType
        }

        // Back-face cull (optional). Keep CCW (area > 0) – adjust sign to your convention
        const Scalar area = triangle_area<Scalar, Vec2>(vout[0].screen,
                                                        vout[1].screen,
                                                        vout[2].screen);
        // ErrorHandling::ValidateTriangleArea(area);
        //  if (area <= 0) return;            // enable to cull backfaces

        // Triangle bounding box (float → int, clamp to viewport)
        Scalar minx = min(min(vout[0].screen(0), vout[1].screen(0)), vout[2].screen(0));
        Scalar maxx = max(max(vout[0].screen(0), vout[1].screen(0)), vout[2].screen(0));
        Scalar miny = min(min(vout[0].screen(1), vout[1].screen(1)), vout[2].screen(1));
        Scalar maxy = max(max(vout[0].screen(1), vout[1].screen(1)), vout[2].screen(1));

        Int x0 = max(viewport.min_x_, static_cast<Int>(floor(minx)));
        Int x1 = min(viewport.max_x_, static_cast<Int>(ceil(maxx)));
        Int y0 = max(viewport.min_y_, static_cast<Int>(floor(miny)));
        Int y1 = min(viewport.max_y_, static_cast<Int>(ceil(maxy)));
        if (x0 >= x1 || y0 >= y1)
            return;

        // Edge setup (top-left rule)
        const Scalar xA = vout[0].screen(0), yA = vout[0].screen(1);
        const Scalar xB = vout[1].screen(0), yB = vout[1].screen(1);
        const Scalar xC = vout[2].screen(0), yC = vout[2].screen(1);

        // const Scalar area2 = edge_func(xA, yA, xB, yB, xC, yC); // 2*area with sign
        // ErrorHandling::ValidateTriangleArea(area2);
        // ErrorHandling::ValidateNonZero(area2, "triangle area calculation");

        const Scalar inv_area = 1.0f / area;

        const bool tlAB = is_top_left(xA, yA, xB, yB);
        const bool tlBC = is_top_left(xB, yB, xC, yC);
        const bool tlCA = is_top_left(xC, yC, xA, yA);

        // Evaluate edge functions at top-left corner of each pixel (add +0.5)
        const Scalar px0 = static_cast<Scalar>(x0) + RenderConstants::PIXEL_CENTER_OFFSET;
        const Scalar py0 = static_cast<Scalar>(y0) + RenderConstants::PIXEL_CENTER_OFFSET;

        Scalar eAB_row = edge_func(xA, yA, xB, yB, px0, py0);
        Scalar eBC_row = edge_func(xB, yB, xC, yC, px0, py0);
        Scalar eCA_row = edge_func(xC, yC, xA, yA, px0, py0);

        // Step increments when moving +1 in X or +1 in Y
        const Scalar eAB_dx = (yB - yA);
        const Scalar eAB_dy = (xA - xB);
        const Scalar eBC_dx = (yC - yB);
        const Scalar eBC_dy = (xB - xC);
        const Scalar eCA_dx = (yA - yC);
        const Scalar eCA_dy = (xC - xA);

    // Rasterize
    draw_triangle_raster_loop_y:
        for (Int y = y0; y < y1; ++y)
        {
            Scalar eAB = eAB_row;
            Scalar eBC = eBC_row;
            Scalar eCA = eCA_row;

        draw_triangle_raster_loop_x:
            for (int x = x0; x < x1; ++x)
            {
                // Top-left rule adjustments (include pixels on top/left edges)
                const bool inside =
                    (eAB > 0 || (eAB == 0 && tlAB)) &&
                    (eBC > 0 || (eBC == 0 && tlBC)) &&
                    (eCA > 0 || (eCA == 0 && tlCA));

                if (inside)
                {
                    // Barycentric weights normalized
                    const Scalar w0 = eBC * inv_area;
                    const Scalar w1 = eCA * inv_area;
                    const Scalar w2 = eAB * inv_area;

                    // Perspective: 1/w at pixel
                    const Scalar invW_px = w0 * vout[0].invW + w1 * vout[1].invW + w2 * vout[2].invW;

                    typename Derived::Varyings varying_px = derived_().interpolate_varyings(w0, w1, w2,
                                                                                            vout[0].invW, vout[1].invW, vout[2].invW,
                                                                                            invW_px,
                                                                                            vout[0].var, vout[1].var, vout[2].var);

                    // Interpolate varyings divided by w, then divide by invW_px
                    // Varyings var_over_w_px =
                    //    w0 * vout[0].var_over_w +
                    //    w1 * vout[1].var_over_w +
                    //    w2 * vout[2].var_over_w;
                    // Varyings varying_px = var_over_w_px * (1.0f / invW_px);

                    // Depth (if needed; same trick)
                    Scalar depth_px = w0 * (vout[0].depth * vout[0].invW) +
                                      w1 * (vout[1].depth * vout[1].invW) +
                                      w2 * (vout[2].depth * vout[2].invW);
                    depth_px *= (1.0f / invW_px);
                    // Depth test could go here

                    Vec4 gl_FragCoord;
                    gl_FragCoord(0) = static_cast<Scalar>(x) + RenderConstants::PIXEL_CENTER_OFFSET;
                    gl_FragCoord(1) = static_cast<Scalar>(y) + RenderConstants::PIXEL_CENTER_OFFSET;
                    gl_FragCoord(2) = depth_px;
                    gl_FragCoord(3) = 1.0f / invW_px;

                    derived_().fragment_shader(gl_FragCoord, varying_px, textures);
                }

                // advance to x+1
                eAB += eAB_dx;
                eBC += eBC_dx;
                eCA += eCA_dx;
            }

            // next row y+1: add dy increments and reset x terms
            eAB_row += eAB_dy;
            eBC_row += eBC_dy;
            eCA_row += eCA_dy;
        }
    }

    // Derived derived_() { return static_cast<Derived &>(*this); }

    Derived &derived_() { return *static_cast<Derived *>(this); }
    const Derived &derived_() const { return *static_cast<const Derived *>(this); }

    Mat4 opencv2opengl_;
};

// -----------------------------------------------------------------------------
// DepthRenderer
//   Example derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

template <class Mesh, template <class> class Texture>
class DepthRendererBase
    : public RendererBase<DepthRendererBase<Mesh, Texture>>
{
public:
    struct Varyings
    {
        float depth;
    };

    struct Textures
    {
        Texture<Scalar> &out_texture;
    };

    DepthRendererBase() = default;
    ~DepthRendererBase() = default;

    void Render(const Mesh &mesh,
                const SE3 &pose,
                const Camera &cam,
                UInt out_lvl,
                Texture<Scalar> &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_ * pose.matrix();
        out_lvl_ = out_lvl;

        // out_texture_ = &out_texture;

        const Int W = static_cast<Int>(out_texture.width(out_lvl));
        const Int H = static_cast<Int>(out_texture.height(out_lvl));
        BoundingBox<Int> viewport(0, W, 0, H);

        Textures textures{out_texture};

        RendererBase<DepthRendererBase<Mesh, Texture>>::Render(mesh, viewport, textures);
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
                         const Varyings &in_varying,
                         Textures &textures)
    {
        textures.out_texture.set_texel_(in_varying.depth, Int(gl_FragCoord(1)), Int(gl_FragCoord(0)), out_lvl_);
    }

    Mat4 t_matrix_;
    UInt out_lvl_;
};

// -----------------------------------------------------------------------------
// ImageRendererBase
//   Another example derived class that might output color
// -----------------------------------------------------------------------------

template <class Mesh, template <class> class Texture>
class ImageRendererBase
    : public RendererBase<ImageRendererBase<Mesh, Texture>>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
    };

    struct Textures
    {
        const Texture<Scalar> &in_texture;
        Texture<Scalar> &out_texture;
    };

    ImageRendererBase() = default;
    ~ImageRendererBase() = default;

    void Render(const Mesh &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const Texture<Scalar> &in_texture,
                Texture<Scalar> &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                    this->opencv2opengl_ *
                    pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        // in_texture_ = &in_texture;
        // out_texture_ = &out_texture;

        const Int W = static_cast<Int>(out_texture.width(out_lvl));
        const Int H = static_cast<Int>(out_texture.height(out_lvl));
        BoundingBox<Int> viewport(0, W, 0, H);

        Textures textures{in_texture, out_texture};

        RendererBase<ImageRendererBase<Mesh, Texture>>::Render(mesh, viewport, textures);
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
                         const Varyings &in_varying,
                         Textures &textures)
    {
        // Scalar pix = textures.in_texture.sample_(in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        Scalar pix = sample<Scalar, Texture<Scalar>>(textures.in_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        // Vec2 coord;
        // coord(0) = in_varying.texcoord(0) * textures.in_texture.width(in_lvl_) - Scalar(0.5);
        // coord(1) = in_varying.texcoord(1) * textures.in_texture.height(in_lvl_) - Scalar(0.5);
        // Scalar pix = bilinear<Scalar, Texture<Scalar>>(textures.in_texture, coord(1), coord(0), in_lvl_);

        if (pix == textures.in_texture.nodata())
            return;
        textures.out_texture.set_texel_(pix, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

    Mat4 t_matrix_;
    int in_lvl_;
    int out_lvl_;
    // const Texture<Scalar> *in_texture_;
    // Texture<Scalar> *out_texture_;
};

template <class Mesh, template <class> class Texture>
class ResidualRendererBase
    : public RendererBase<ResidualRendererBase<Mesh, Texture>>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
    };

    struct Textures
    {
        const Texture<Scalar> &kf_texture;
        const Texture<Scalar> &f_texture;
        TextureCPU<Scalar> &r_texture;
    };

    ResidualRendererBase() = default;
    ~ResidualRendererBase() = default;

    void Render(const Mesh &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const Texture<Scalar> &kf_texture,
                const Texture<Scalar> &f_texture,
                Texture<Scalar> &r_texture)
    {
        r_texture.fill(out_lvl, r_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                    this->opencv2opengl_ * pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        // kf_texture_ = &kf_texture;
        // f_texture_ = &f_texture;
        // r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Textures textures{kf_texture, f_texture, r_texture};

        RendererBase<ResidualRendererBase<Mesh, Texture>>::Render(mesh, viewport, textures);
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
                         const Varyings &in_varying,
                         Textures &textures)
    {
        int width = textures.kf_texture.width(out_lvl_);
        int height = textures.kf_texture.height(out_lvl_);

        Vec2 screen_texcoord(gl_FragCoord(0) / float(width), gl_FragCoord(1) / float(height));

        float kf = sample<float, TextureCPU<float>>(textures.kf_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        float f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = f_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);

        if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata())
            return;

        float e = f - kf;
        textures.r_texture.set_texel_(e, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    Mat4 t_matrix_;
    int in_lvl_;
    int out_lvl_;
    // const TextureCPU<float> *kf_texture_;
    // const TextureCPU<float> *f_texture_;
    // TextureCPU<float> *r_texture_;
};

template <class Mesh, template <class> class Texture>
class L2RendererBase
    : public RendererBase<L2RendererBase<Mesh, Texture>>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
    };

    struct Textures
    {
        const Texture<Scalar> &kf_texture;
        const Texture<Scalar> &f_texture;
        Texture<Scalar> &r_texture;
    };

    L2RendererBase() = default;
    ~L2RendererBase() = default;

    void Render(const Mesh &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const Texture<Scalar> &kf_texture,
                const Texture<Scalar> &f_texture,
                Texture<Scalar> &r_texture)
    {
        r_texture.fill(out_lvl, r_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_ * pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        // kf_texture_ = &kf_texture;
        // f_texture_ = &f_texture;
        // r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Textures textures{kf_texture, f_texture, r_texture};

        RendererBase<L2RendererBase<Mesh, Texture>>::Render(mesh, viewport, textures);
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
                         const Varyings &in_varying,
                         Textures &textures)
    {
        UInt width = textures.kf_texture.width(out_lvl_);
        UInt height = textures.kf_texture.height(out_lvl_);

        Vec2 screen_texcoord(gl_FragCoord(0) / float(width), gl_FragCoord(1) / float(height));

        Scalar kf = sample<Scalar, Texture<Scalar>>(textures.kf_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        Scalar f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // Scalar f = sample<Scalar, Texture<Scalar>>(textures.f_texture, screen_texcoord(1), screen_texcoord(0), in_lvl_);

        if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata())
            return;

        Scalar e = f - kf;
        textures.r_texture.set_texel_(e * e, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    Mat4 t_matrix_;
    int in_lvl_;
    int out_lvl_;
    // const TextureCPU<float> *kf_texture_;
    // const TextureCPU<float> *f_texture_;
    // TextureCPU<float> *r_texture_;
};

template <class Mesh, template <class> class Texture>
class DIDxyRendererBase
    : public RendererBase<DIDxyRendererBase<Mesh, Texture>>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
    };

    struct Textures
    {
        const Texture<Scalar> &in_texture;
        Texture<Vec3> &out_texture;
    };

    DIDxyRendererBase() = default;
    ~DIDxyRendererBase() = default;

    void Render(const Mesh &mesh,
                int in_lvl,
                int out_lvl,
                const Texture<Scalar> &in_texture,
                Texture<Vec3> &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        // in_texture_ = &in_texture;
        // out_texture_ = &out_texture;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Textures textures{in_texture, out_texture};

        RendererBase<DIDxyRendererBase<Mesh, Texture>>::Render(mesh, viewport, textures);
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
                         const Varyings &in_varying,
                         Textures &textures)
    {
        // outFragment = inVarying;

        int height = textures.in_texture.height(out_lvl_);
        int width = textures.in_texture.width(out_lvl_);
        float nodata = textures.in_texture.nodata();

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

        Scalar f = textures.in_texture.texel_(y, x, out_lvl_);
        Scalar f_y_p = textures.in_texture.texel_(y_p, x, out_lvl_);
        Scalar f_y_m = textures.in_texture.texel_(y_m, x, out_lvl_);
        Scalar f_x_p = textures.in_texture.texel_(y, x_p, out_lvl_);
        Scalar f_x_m = textures.in_texture.texel_(y, x_m, out_lvl_);

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

        textures.out_texture.set_texel_(out_fragment, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    int in_lvl_;
    int out_lvl_;
    // const TextureCPU<float> *in_texture_;
    // TextureCPU<Vec3> *out_texture_;
};

template <class Mesh, template <class> class Texture>
class JPoseRendererBase
    : public RendererBase<JPoseRendererBase<Mesh, Texture>>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
        Vec3 f_ver;
    };

    struct Textures
    {
        const Texture<Scalar> &kf_texture;
        const Texture<Scalar> &f_texture;
        const Texture<Vec3> &dfdxy_texture;
        Texture<Vec3> &jtra_texture;
        Texture<Vec3> &jrot_texture;
        Texture<Scalar> &r_texture;
    };

    JPoseRendererBase() = default;
    ~JPoseRendererBase() = default;

    void Render(const Mesh &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const Texture<Scalar> &kf_texture,
                const Texture<Scalar> &f_texture,
                const Texture<Vec3> &dfdxy_texture,
                Texture<Vec3> &jtra_texture,
                Texture<Vec3> &jrot_texture,
                Texture<Scalar> &r_texture)
    {
        jtra_texture.fill(out_lvl, jtra_texture.nodata());
        jrot_texture.fill(out_lvl, jrot_texture.nodata());
        r_texture.fill(out_lvl, r_texture.nodata());

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        fx_ = cam.GetParams()(0);
        fy_ = cam.GetParams()(1);
        view_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_;
        pose_matrix_ = pose.matrix();
        // kf_texture_ = &kf_texture;
        // f_texture_ = &f_texture;
        // dfdxy_texture_ = &dfdxy_texture;
        // jtra_texture_ = &jtra_texture;
        // jrot_texture_ = &jrot_texture;
        // r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Textures textures{kf_texture, f_texture, dfdxy_texture, jtra_texture, jrot_texture, r_texture};

        RendererBase<JPoseRendererBase<Mesh, Texture>>::Render(mesh, viewport, textures);
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
                         const Varyings &in_varying,
                         Textures &textures)
    {
        int width = textures.kf_texture.width(out_lvl_);
        int height = textures.kf_texture.height(out_lvl_);

        Vec2 screen_texcoord(gl_FragCoord(0) / float(width), gl_FragCoord(1) / float(height));

        Vec3 f_ver = in_varying.f_ver;
        Vec2 texcoord = in_varying.texcoord;

        float kf = sample<Scalar, Texture<Scalar>>(textures.kf_texture, texcoord(1), texcoord(0), in_lvl_);
        float f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        Vec3 f_der = textures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = sample<Scalar, Texture<Scalar>>(textures.f_texture, screen_texcoord(1), screen_texcoord(0), in_lvl_);
        // Vec3 f_der = sample<Vec3, Texture<Vec3>>(textures.dfdxy_texture, screen_texcoord(1), screen_texcoord(0), in_lvl_);

        if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata() || f_der == textures.dfdxy_texture.nodata())
            return;

        float r = f - kf;

        float v0 = f_der(0) * fx_ * width / f_ver(2);
        float v1 = f_der(1) * fy_ * height / f_ver(2);
        float v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        Vec3 d_f_i_d_tra = Vec3(v0, v1, v2);
        Vec3 d_f_i_d_rot = Vec3(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        textures.jtra_texture.set_texel_(d_f_i_d_tra, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.jrot_texture.set_texel_(d_f_i_d_rot, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.r_texture.set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    int in_lvl_;
    int out_lvl_;
    float fx_;
    float fy_;
    Mat4 view_matrix_;
    Mat4 pose_matrix_;
    // const TextureCPU<float> *kf_texture_;
    // const TextureCPU<float> *f_texture_;
    // const TextureCPU<Vec3> *dfdxy_texture_;
    // TextureCPU<Vec3> *jtra_texture_;
    // TextureCPU<Vec3> *jrot_texture_;
    // TextureCPU<float> *r_texture_;
};

template <class Mesh, template <class> class Texture>
class JMapRendererBase
    : public RendererBase<JMapRendererBase<Mesh, Texture>>
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

    struct Textures
    {
        const Texture<Scalar> &kf_texture;
        const Texture<Scalar> &f_texture;
        const Texture<Vec3> &dfdxy_texture;
        Texture<Vec3> &jmap_texture;
        Texture<Vec3> &pids_texture;
        Texture<Scalar> &r_texture;
    };

    JMapRendererBase() = default;
    ~JMapRendererBase() = default;

    void Render(const Mesh &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const Texture<Scalar> &kf_texture,
                const Texture<Scalar> &f_texture,
                const Texture<Vec3> &dfdxy_texture,
                Texture<Vec3> &jmap_texture,
                Texture<Vec3> &pids_texture,
                Texture<Scalar> &r_texture)
    {
        jmap_texture.fill(out_lvl, jmap_texture.nodata());
        pids_texture.fill(out_lvl, pids_texture.nodata());
        r_texture.fill(out_lvl, r_texture.nodata());

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        fx_ = cam.GetParams()(0);
        fy_ = cam.GetParams()(1);
        view_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_;
        pose_matrix_ = pose.matrix();
        // kf_texture_ = &kf_texture;
        // f_texture_ = &f_texture;
        // dfdxy_texture_ = &dfdxy_texture;
        // jmap_texture_ = &jmap_texture;
        // pids_texture_ = &pids_texture;
        // r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Textures textures{kf_texture, f_texture, dfdxy_texture, jmap_texture, pids_texture, r_texture};

        RendererBase<JMapRendererBase<Mesh, Texture>>::Render(mesh, viewport, textures);
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
                         const Varyings &in_varying,
                         Textures &textures)
    {
        int width = textures.jmap_texture.width(out_lvl_);
        int height = textures.jmap_texture.height(out_lvl_);

        Vec2 screen_texcoord(gl_FragCoord(0) / float(width), gl_FragCoord(1) / float(height));

        Vec3 f_ver = in_varying.f_ver;
        Vec3 kf_ray = in_varying.kf_ray;
        Vec2 texcoord = in_varying.texcoord;
        Vec3 barycentric = in_varying.barycentric;
        Vec3i vertexid = in_varying.pids;

        float kf = sample<Scalar, Texture<Scalar>>(textures.kf_texture, texcoord(1), texcoord(0), in_lvl_);
        float f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        Vec3 f_der = textures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = f_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);
        // Vec3 f_der = dfdxy_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);

        if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata() || f_der == textures.dfdxy_texture.nodata())
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

        textures.jmap_texture.set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.pids_texture.set_texel_(ids, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.r_texture.set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    int in_lvl_;
    int out_lvl_;
    float fx_;
    float fy_;
    Mat4 view_matrix_;
    Mat4 pose_matrix_;
    // const TextureCPU<float> *kf_texture_;
    // const TextureCPU<float> *f_texture_;
    // const TextureCPU<Vec3> *dfdxy_texture_;
    // TextureCPU<Vec3> *jmap_texture_;
    // TextureCPU<Vec3> *pids_texture_;
    // TextureCPU<float> *r_texture_;
};

template <class Mesh, template <class> class Texture>
class DiffRendererBase
    : public RendererBase<DiffRendererBase<Mesh, Texture>>
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

    struct Textures
    {
        const Texture<Scalar> &f_texture;
        Texture<Vec3> &jmap_texture;
        Texture<Vec3> &pids_texture;
        Texture<Scalar> &r_texture;
    };

    DiffRendererBase() = default;
    ~DiffRendererBase() = default;

    void Render(const Mesh &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const Texture<Scalar> &f_texture,
                Texture<Vec3> &jtra_texture,
                Texture<Vec3> &jrot_texture,
                Texture<Vec3> &jmap_texture,
                Texture<Vec3i> &pids_texture)
    {
        jtra_texture.fill(out_lvl, jtra_texture.nodata());
        jrot_texture.fill(out_lvl, jrot_texture.nodata());
        jmap_texture.fill(out_lvl, jmap_texture.nodata());
        pids_texture.fill(out_lvl, pids_texture.nodata());

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        fx_ = cam.GetParams()(0);
        fy_ = cam.GetParams()(1);
        view_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_;
        pose_matrix_ = pose.matrix();
        // kf_texture_ = &kf_texture;
        // f_texture_ = &f_texture;
        // dfdxy_texture_ = &dfdxy_texture;
        // jmap_texture_ = &jmap_texture;
        // pids_texture_ = &pids_texture;
        // r_texture_ = &r_texture;

        const int W = static_cast<int>(jtra_texture.width(out_lvl));
        const int H = static_cast<int>(jtra_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Textures textures{f_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture};

        RendererBase<DiffRendererBase<Mesh, Texture>>::Render(mesh, viewport, textures);
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
                         const Varyings &in_varying,
                         Textures &textures)
    {
        int width = textures.jmap_texture.width(out_lvl_);
        int height = textures.jmap_texture.height(out_lvl_);

        Vec2 screen_texcoord(gl_FragCoord(0) / float(width), gl_FragCoord(1) / float(height));

        Vec3 f_ver = in_varying.f_ver;
        Vec3 kf_ray = in_varying.kf_ray;
        Vec2 texcoord = in_varying.texcoord;
        Vec3 barycentric = in_varying.barycentric;
        Vec3i vertexid = in_varying.pids;

        float f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        Vec3 f_der = compute_didxy(textures.f_texture, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);

        if (f == textures.f_texture.nodata() || f_der == textures.dfdxy_texture.nodata())
            return;

        Vec3 d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = f_der(0) * fx_ * width / f_ver(2);
        d_f_i_d_f_ver(1) = f_der(1) * fy_ * height / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // Vec3 d_f_i_d_tra = Vec3(v0, v1, v2);
        Vec3 d_f_i_d_rot = Vec3(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2), f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2), -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        Vec3 d_f_ver_d_kf_depth = kf_ray; // kfTofPose.rotationMatrix() * kf_ray;
        float d_f_i_d_kf_depth = d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth;

        Vec3 d_depth_d_vert_depth = barycentric;

        Vec3 jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        Vec3 ids = Vec3(vertexid(0), vertexid(1), vertexid(2));

        textures.jtra_texture.set_texel_(d_f_i_d_f_ver, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.jrot_texture.set_texel_(d_f_i_d_rot, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.jmap_texture.set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.pids_texture.set_texel_(ids, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

private:
    int in_lvl_;
    int out_lvl_;
    float fx_;
    float fy_;
    Mat4 view_matrix_;
    Mat4 pose_matrix_;
    // const TextureCPU<float> *kf_texture_;
    // const TextureCPU<float> *f_texture_;
    // const TextureCPU<Vec3> *dfdxy_texture_;
    // TextureCPU<Vec3> *jmap_texture_;
    // TextureCPU<Vec3> *pids_texture_;
    // TextureCPU<float> *r_texture_;
};
