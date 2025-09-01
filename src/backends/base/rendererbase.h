#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "core/render_constants.h"
#include "core/error_handling.h"

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
// BaseRendererCPU (improved)
// -----------------------------------------------------------------------------
template <class Derived, class Int, class Scalar, class Vec2, class Vec3, class Vec4, class Vec3i, class Mat4>
class BaseRenderer
{
public:
    BaseRenderer()
    {
        opencv2opengl_ = Mat4::Identity();
        opencv2opengl_(2, 2) = -1.0; // flip Z like your original intent
    };
    virtual ~BaseRenderer() = default;

protected:
    Mat4 opencv2opengl_;

    // Triangle rasterizer (top-left rule, perspective correct)
    void draw_triangle_(const Vec3 *verts,
                        const Vec2 *texcoords,
                        const Scalar *weights,
                        const Vec3i &vertexid,
                        const BoundingBoxType<Int> &viewport)
    {
        // Vertex shading & clip → NDC → screen
        struct VSOut
        {
            Vec2 screen; // x,y in pixel space (float)
            Scalar depth; // z in [0,1] if your projection is like GL_ZERO_TO_ONE
            Scalar invW;  // 1 / clip.w
            // std::tuple<Varyings...> var_over_w; // varyings multiplied by invW
            typename Derived::Varyings var; // original varyings (for convenience)
        } vout[3];

        for (int i = 0; i < 3; ++i)
        {
            Vec4 gl_Position;
            typename Derived::Varyings varyings;
            derived_().vertex_shader(verts[i], texcoords[i], weights[i], vertexid, gl_Position, varyings);

            const Scalar invW = 1.0f / gl_Position(3);
            const Scalar ndc_x = gl_Position(0) * invW; // [-1,1]
            const Scalar ndc_y = gl_Position(1) * invW;
            const Scalar ndc_z = gl_Position(2) * invW; // assumed 0..1 after proj (adjust if -1..1)

            // pixel-space (don’t clamp here) — match GL rasterization (remove +1/-0.5 adjustment)
            vout[i].screen(0) = 0.5f * (ndc_x + 1.0f) * (viewport.max_x_ - viewport.min_x_);
            vout[i].screen(1) = 0.5f * (ndc_y + 1.0f) * (viewport.max_y_ - viewport.min_y_);
            vout[i].depth = ndc_z;
            vout[i].invW = invW;
            vout[i].var = varyings;
            // vout[i].var_over_w = varyings * invW; // requires scalar*VaryingType
        }

        // Back-face cull (optional). Keep CCW (area > 0) – adjust sign to your convention
        const Scalar area = triangle_area<Scalar, Vec2>(
            Vec2(vout[0].screen(0), vout[0].screen(1)),
            Vec2(vout[1].screen(0), vout[1].screen(1)),
            Vec2(vout[2].screen(0), vout[2].screen(1)));
        ErrorHandling::ValidateTriangleArea(area);
        // if (area <= 0) return;            // enable to cull backfaces

        // Triangle bounding box (float → int, clamp to viewport)
        Scalar minx = std::min({vout[0].screen(0), vout[1].screen(0), vout[2].screen(0)});
        Scalar maxx = std::max({vout[0].screen(0), vout[1].screen(0), vout[2].screen(0)});
        Scalar miny = std::min({vout[0].screen(1), vout[1].screen(1), vout[2].screen(1)});
        Scalar maxy = std::max({vout[0].screen(1), vout[1].screen(1), vout[2].screen(1)});

        Int x0 = std::max(viewport.min_x_, static_cast<Int>(std::floor(minx)));
        Int x1 = std::min(viewport.max_x_, static_cast<Int>(std::ceil(maxx)));
        Int y0 = std::max(viewport.min_y_, static_cast<Int>(std::floor(miny)));
        Int y1 = std::min(viewport.max_y_, static_cast<Int>(std::ceil(maxy)));
        if (x0 > x1 || y0 > y1)
            return;

        // Edge setup (top-left rule)
        const Scalar xA = vout[0].screen(0), yA = vout[0].screen(1);
        const Scalar xB = vout[1].screen(0), yB = vout[1].screen(1);
        const Scalar xC = vout[2].screen(0), yC = vout[2].screen(1);

        const Scalar area2 = edge_func(xA, yA, xB, yB, xC, yC); // 2*area with sign
        ErrorHandling::ValidateTriangleArea(area2);
        ErrorHandling::ValidateNonZero(area2, "triangle area calculation");

        const Scalar inv_area2 = 1.0f / area2;

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
        for (int y = y0; y <= y1; ++y)
        {
            Scalar eAB = eAB_row;
            Scalar eBC = eBC_row;
            Scalar eCA = eCA_row;

            for (int x = x0; x <= x1; ++x)
            {
                // Top-left rule adjustments (include pixels on top/left edges)
                const bool inside =
                    (eAB > 0 || (eAB == 0 && tlAB)) &&
                    (eBC > 0 || (eBC == 0 && tlBC)) &&
                    (eCA > 0 || (eCA == 0 && tlCA));

                if (inside)
                {
                    // Barycentric weights normalized
                    const Scalar w0 = eBC * inv_area2;
                    const Scalar w1 = eCA * inv_area2;
                    const Scalar w2 = eAB * inv_area2;

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
};
