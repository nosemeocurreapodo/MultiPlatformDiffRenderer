#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
#include "core/types.h"
#include "core/render_constants.h"
//#include "core/error_handling.h"

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

    template <typename Mesh>
    void Render(const Mesh &mesh,
                const BoundingBox<Int> &viewport)
    {
        // ---- Map mesh buffers (no copies) ----
        //auto pos = mesh.MapReadPositions(); // 3 floats/vertex
        //auto tex = mesh.MapReadTexcoords(); // 2 floats/vertex
        //auto wei = mesh.MapReadWeights();   // 1 float /vertex
        //auto idx = mesh.MapReadIndices();   // uint32_t indices

        const auto pos = mesh.Positions(); // 3 floats/vertex
        const auto tex = mesh.Texcoords(); // 2 floats/vertex
        const auto wei = mesh.Weights();   // 1 float /vertex
        const auto idx = mesh.Indices();   // uint32_t indices

        // Loop over triangles
        for (std::size_t i = 0; i + 2 < idx.size(); i += 3)
        {
            const uint32_t i0 = idx[i + 0];
            const uint32_t i1 = idx[i + 1];
            const uint32_t i2 = idx[i + 2];

            Vec3 v[3];
            Vec2 uv[3];
            Scalar wght[3];
            UInt id[3];

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

protected:
    // Triangle rasterizer (top-left rule, perspective correct)
    void draw_triangle_(const Vec3 *verts,
                        const Vec2 *texcoords,
                        const Scalar *weights,
                        const UInt *vertexid,
                        const BoundingBox<Int> &viewport)
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
            vout[i].screen(0) = viewport.min_x_ + Scalar(0.5) * (ndc_x + Scalar(1)) * vp_w;
            vout[i].screen(1) = viewport.min_y_ + Scalar(0.5) * (ndc_y + Scalar(1)) * vp_h;
            vout[i].depth = ndc_z;
            vout[i].invW = invW;
            vout[i].var = varyings;
            // vout[i].var_over_w = varyings * invW; // requires scalar*VaryingType
        }

        // Back-face cull (optional). Keep CCW (area > 0) – adjust sign to your convention
        const Scalar area = triangle_area<Scalar, Vec2>(vout[0].screen,
                                                        vout[1].screen,
                                                        vout[2].screen);
        //ErrorHandling::ValidateTriangleArea(area);
        // if (area <= 0) return;            // enable to cull backfaces

        // Triangle bounding box (float → int, clamp to viewport)
        Scalar minx = min({vout[0].screen(0), vout[1].screen(0), vout[2].screen(0)});
        Scalar maxx = max({vout[0].screen(0), vout[1].screen(0), vout[2].screen(0)});
        Scalar miny = min({vout[0].screen(1), vout[1].screen(1), vout[2].screen(1)});
        Scalar maxy = max({vout[0].screen(1), vout[1].screen(1), vout[2].screen(1)});

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
        for (Int y = y0; y < y1; ++y)
        {
            Scalar eAB = eAB_row;
            Scalar eBC = eBC_row;
            Scalar eCA = eCA_row;

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

                    // OutTexType outColor = out_texture.nodata();
                    derived_().fragment_shader(gl_FragCoord, varying_px);
                    // out_texture.set_texel_(outColor, y, x, out_lvl);
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

    Derived &derived_() { return *static_cast<Derived *>(this); }
    const Derived &derived_() const { return *static_cast<const Derived *>(this); }

    Mat4 opencv2opengl_;
};

// -----------------------------------------------------------------------------
// DepthRenderer
//   Example derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

template <class Mesh, class Texture>
class DepthRendererBase
    : public RendererBase<DepthRendererBase<Mesh, Texture>>
{
public:
    struct Varyings
    {
        float depth;
    };

    DepthRendererBase() = default;
    ~DepthRendererBase() = default;

    void Render(const Mesh &mesh,
                const SE3 &pose,
                const Camera &cam,
                int out_lvl,
                Texture &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * RendererBase<DepthRendererBase<Mesh, Texture>>::opencv2opengl_ * pose.matrix();
        out_lvl_ = out_lvl;
        out_texture_ = &out_texture;

        const Int W = static_cast<Int>(out_texture.width(out_lvl));
        const Int H = static_cast<Int>(out_texture.height(out_lvl));
        BoundingBox<Int> viewport(0, W, 0, H);

        RendererBase<DepthRendererBase<Mesh, Texture>>::Render(mesh, viewport);
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
    Texture *out_texture_;
};

// -----------------------------------------------------------------------------
// ImageRendererBase
//   Another example derived class that might output color
// -----------------------------------------------------------------------------

template <class Mesh, class TextureIn, class TextureOut>
class ImageRendererBase
    : public RendererBase<ImageRendererBase<Mesh, TextureIn, TextureOut>>
{
public:
    struct Varyings
    {
        Vec2 texcoord;
    };

    ImageRendererBase() = default;
    ~ImageRendererBase() = default;

    void Render(const Mesh &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureIn &in_texture,
                TextureOut &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                    RendererBase<ImageRendererBase<Mesh, TextureIn, TextureOut>>::opencv2opengl_ *
                    pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        in_texture_ = &in_texture;
        out_texture_ = &out_texture;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        RendererBase<ImageRendererBase<Mesh, TextureIn, TextureOut>>::Render(mesh, viewport);
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
    const TextureIn *in_texture_;
    TextureOut *out_texture_;
};
