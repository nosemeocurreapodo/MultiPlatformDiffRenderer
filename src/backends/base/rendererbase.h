#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
// #include "linalg/linalg.h"
#include "backends/base/texturebase.h"
#include "core/render_constants.h"
#include "core/types.h"

#ifdef USE_VITIS
#include "backends/xrt/hls/math_common.h"
#else
#include "backends/cpu/math_common.h"
#endif

// template <typename T>
// inline T cross(const linalg::Vec2<T> &a, const linalg::Vec2<T> &b) { return a(0) * b(1) - a(1) * b(0); }

// template <typename T>
// inline T triangle_area(const linalg::Vec2<T> &p0, const linalg::Vec2<T> &p1, const linalg::Vec2<T> &p2) { return cross<T>(p1 - p0, p2 - p0); }

// Edge function E_ab(p) = (vout[1].screen(1)-vout[0].screen(1))*px + (vout[0].screen(0)-vout[1].screen(0))*py + (vout[1].screen(0)*vout[0].screen(1) - vout[0].screen(0)*vout[1].screen(1))
template <typename T>
T edge_func(const Vec2<T> &v0, const Vec2<T> &v1, const Vec2<T> &v2)
{
    // #pragma HLS INLINE
    //   return (y1 - y0) * (px - x0) + (x0 - x1) * (py - y0);
    //    return (by - ay) * px + (ax - bx) * py + (bx * ay - ax * by);
    Vec2<T> v10 = v1 - v0;
    Vec2<T> v20 = v2 - v0;
    // for y up
    // return v10.cross(v20);
    // for y down
    return v20.cross(v10);
    // return (v2(1) - v0(1)) * (v1(0) - v0(0)) - (v2(0) - v0(0)) * (v1(1) - v0(1));
}

// Top-left test: returns true if edge is a "top" or "left" edge
template <typename T>
bool is_top_left(const Vec2<T> &v0, const Vec2<T> &v1)
{
    // #pragma HLS INLINE
    //   return (v0(1) == v1(1)) ? (v1(0) < v0(0)) : (v0(1) < v1(1));
    //   for y up
    //   return (v0(1) < v1(1)) || (v0(1) == v1(1) && v0(0) > v1(0));
    //   for y down
    return (v0(1) > v1(1)) || (v0(1) == v1(1) && v0(0) > v1(0));
}

// -----------------------------------------------------------------------------
// RendererBase
// -----------------------------------------------------------------------------
template <class Derived>
class RendererBase
{
public:
    // Vertex shading & clip → NDC → screen
    struct VSOut
    {
        Vec2<RealType> screen; // x,y in pixel space (float)
        RealType depth;        // z in [0,1] if your projection is like GL_ZERO_TO_ONE
        RealType invW;         // 1 / clip.w
        // std::tuple<Varyings...> var_over_w; // varyings multiplied by invW
        typename Derived::Varyings var; // original varyings (for convenience)
    };

    struct Triangle
    {
        VSOut vout[3];
    };

    // RendererBase()
    //{
    //     opencv2opengl_ = linalg::Mat4<MathType>::Identity();
    //     opencv2opengl_(1, 1) = -1.0;
    //     opencv2opengl_(2, 2) = -1.0;
    // };

    // virtual ~RendererBase() = default;
    //~RendererBase() = default;

protected:
    template <class Mesh, typename VertexData, typename Uniforms>
    static void get_vertex_data(const Mesh &mesh,
                                VertexData *vertex_data,
                                IntType *vertex_ids)
    {
        // #pragma HLS INLINE off

        // Loop over triangles
    renderbase_render_triangles_loop:
        for (IntType i = 0; i + 2 < mesh.ebo_buffer_.size(); i += 3)
        {

#pragma HLS loop_tripcount min = 768 max = 768 avg = 768
            // #pragma HLS PIPELINE II = 1

            IntType vertexids[3];

            IntType vertexids_0 = mesh.ebo_buffer_[i + 0];
            IntType vertexids_1 = mesh.ebo_buffer_[i + 1];
            IntType vertexids_2 = mesh.ebo_buffer_[i + 2];

            vertex_data[i * 3 + 0] = Derived::get_vertex_data(mesh, vertexids_0);
            vertex_data[i * 3 + 1] = Derived::get_vertex_data(mesh, vertexids_1);
            vertex_data[i * 3 + 2] = Derived::get_vertex_data(mesh, vertexids_2);

            vertex_ids[i * 3 + 0] = vertexids_0;
            vertex_ids[i * 3 + 1] = vertexids_1;
            vertex_ids[i * 3 + 2] = vertexids_2;
        }
    }

    template <class Mesh, typename Uniforms>
    static void get_triangles(const Mesh &mesh,
                              const BoundingBox<IntType> &viewport,
                              const Uniforms &uniforms,
                              Triangle *triangles,
                              IntType max_num_triangles,
                              IntType &num_triangles)
    {
        // #pragma HLS INLINE off

        num_triangles = 0;

        // Loop over triangles
    renderbase_render_triangles_loop:
        for (IntType i = 0; i + 2 < mesh.ebo_buffer_.size(); i += 3)
        {

#pragma HLS loop_tripcount min = 768 max = 768 avg = 768
            // #pragma HLS PIPELINE II = 1

            IntType vertexids[3];

            vertexids[0] = mesh.ebo_buffer_[i + 0];
            vertexids[1] = mesh.ebo_buffer_[i + 1];
            vertexids[2] = mesh.ebo_buffer_[i + 2];

            typename Derived::VertexData vertexdata[3];

            vertexdata[0] = Derived::get_vertex_data(mesh, vertexids[0]);
            vertexdata[1] = Derived::get_vertex_data(mesh, vertexids[1]);
            vertexdata[2] = Derived::get_vertex_data(mesh, vertexids[2]);

            Triangle triangle;

            create_triangle_(vertexdata, vertexids, viewport, uniforms, triangle);

            // use tile binning
            // Back-face cull (optional). Keep CCW (area > 0) – adjust sign to your convention
            RealType area2 = edge_func(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen); // 2*area with sign

            if (area2 < RealType(0))
                continue; // enable to cull backfaces

            // Triangle bounding box (float → int, clamp to viewport)
            BoundingBox<RealType> tri_bb(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen);

            // IntType viewport_min_x = max(viewport.min_x_, static_cast<IntType>(floor(tri_bb.min_x_)));
            // IntType viewport_max_x = min(viewport.max_x_, static_cast<IntType>(ceil(tri_bb.max_x_)));
            // IntType viewport_min_y = max(viewport.min_y_, static_cast<IntType>(floor(tri_bb.min_y_)));
            // IntType viewport_max_y = min(viewport.max_y_, static_cast<IntType>(ceil(tri_bb.max_y_)));

            IntType viewport_min_x = max(viewport.min_x_, static_cast<IntType>(tri_bb.min_x_));
            IntType viewport_max_x = min(viewport.max_x_, static_cast<IntType>(tri_bb.max_x_ + 1));
            IntType viewport_min_y = max(viewport.min_y_, static_cast<IntType>(tri_bb.min_y_));
            IntType viewport_max_y = min(viewport.max_y_, static_cast<IntType>(tri_bb.max_y_ + 1));

            if (viewport_min_x >= viewport_max_x || viewport_min_y >= viewport_max_y)
                continue;

            triangles[num_triangles] = triangle;
            num_triangles++;
            if (num_triangles >= max_num_triangles)
                break;
        }
    }

    static void create_tile_viewports_(BoundingBox<IntType> *viewport_tiles, const BoundingBox<IntType> &viewport, IntType num_tiles_x, IntType num_tiles_y)
    {
        // #pragma HLS INLINE off

    create_tile_viewport_y_loop:
        for (int y = 0; y < num_tiles_y; y++)
        {
#pragma HLS loop_tripcount min = 8 max = 8 avg = 8
#pragma HLS pipeline off

        create_tile_viewport_x_loop:
            for (int x = 0; x < num_tiles_x; x++)
            {
#pragma HLS loop_tripcount min = 8 max = 8 avg = 8
#pragma HLS loop_flatten off

                IntType min_x_ = IntType(RealType(viewport.width_ * x) / RealType(num_tiles_x)) + viewport.min_x_;
                IntType max_x_ = IntType(RealType(viewport.width_ * (x + 1)) / RealType(num_tiles_x)) + viewport.min_x_;
                IntType min_y_ = IntType(RealType(viewport.height_ * y) / RealType(num_tiles_y)) + viewport.min_y_;
                IntType max_y_ = IntType(RealType(viewport.height_ * (y + 1)) / RealType(num_tiles_y)) + viewport.min_y_;

                viewport_tiles[y * num_tiles_x + x] = BoundingBox<IntType>(min_x_, max_x_, min_y_, max_y_);
            }
        }
    }

    template <typename VertexData, typename Uniforms>
    static void create_triangle_(const VertexData *vertexdata, const IntType *vertexids, const BoundingBox<IntType> &viewport, const Uniforms &uniforms, Triangle &triangle)
    {
        // #pragma HLS INLINE off

    create_triangle_loop:
        for (int j = 0; j < 3; ++j)
        {
            // #pragma HLS UNROLL

            Vec4<RealType> gl_Position;
            typename Derived::Varyings outvaryings;
            Derived::vertex_shader(vertexdata[j], vertexids[j], uniforms, gl_Position, outvaryings);

            const RealType invW = RealType(1) / gl_Position(3);
            const RealType ndc_x = gl_Position(0) * invW; // [-1,1]
            const RealType ndc_y = gl_Position(1) * invW;
            const RealType ndc_z = gl_Position(2) * invW; // assumed 0..1 after proj (adjust if -1..1)

            // pixel-space (don’t clamp here) — match GL rasterization (remove +1/-0.5 adjustment)
            triangle.vout[j].screen(0) = RealType(0.5) * (ndc_x + RealType(1)) * viewport.width_ + viewport.min_x_;
            triangle.vout[j].screen(1) = RealType(0.5) * (ndc_y + RealType(1)) * viewport.height_ + viewport.min_y_;
            // triangle.vout[i].screen(0) = MathType(0.5) * (ndc_x + MathType(1));
            // triangle.vout[i].screen(1) = MathType(0.5) * (ndc_y + MathType(1));
            triangle.vout[j].depth = RealType(0.5) * (ndc_z + RealType(1));
            triangle.vout[j].invW = invW;
            triangle.vout[j].var = outvaryings;
            // vout[i].var_over_w = varyings * invW; // requires T*VaryingType
        }
    }

    // Triangle rasterizer (top-left rule, perspective correct)
    template <typename InTextures, typename Uniforms, typename Fragment>
    static void draw_triangle_(const Triangle &triangle, const BoundingBox<IntType> &tile_bb, RealType *depth_buffer, const Uniforms &uniforms, const InTextures &intextures, Fragment *fragment_buffer)
    {
#pragma HLS inline

        BoundingBox<RealType> tri_bb(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen);

        // IntType min_x = max(tile_bb.min_x_, static_cast<IntType>(floor(tri_bb.min_x_)));
        // IntType max_x = min(tile_bb.max_x_, static_cast<IntType>(ceil(tri_bb.max_x_)));
        // IntType min_y = max(tile_bb.min_y_, static_cast<IntType>(floor(tri_bb.min_y_)));
        // IntType max_y = min(tile_bb.max_y_, static_cast<IntType>(ceil(tri_bb.max_y_)));

        IntType min_x = max(tile_bb.min_x_, static_cast<IntType>(tri_bb.min_x_));
        IntType max_x = min(tile_bb.max_x_, static_cast<IntType>(tri_bb.max_x_ + RealType(1)));
        IntType min_y = max(tile_bb.min_y_, static_cast<IntType>(tri_bb.min_y_));
        IntType max_y = min(tile_bb.max_y_, static_cast<IntType>(tri_bb.max_y_ + RealType(1)));

        BoundingBox<IntType> triangle_bb(min_x, max_x, min_y, max_y);

        // Back-face cull (optional). Keep CCW (area > 0) – adjust sign to your convention
        RealType area2 = edge_func(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen); // 2*area with sign

        if (area2 <= RealType(0))
            return; // enable to cull backfaces

        const RealType inv_area2 = RealType(1) / area2;

        const bool tlAB = is_top_left(triangle.vout[0].screen, triangle.vout[1].screen);
        const bool tlBC = is_top_left(triangle.vout[1].screen, triangle.vout[2].screen);
        const bool tlCA = is_top_left(triangle.vout[2].screen, triangle.vout[0].screen);

        // Evaluate edge functions at top-left corner of each pixel (add +0.5)
        Vec2<RealType> p;
        p(0) = static_cast<RealType>(triangle_bb.min_x_) + RealType(RenderConstants::PIXEL_CENTER_OFFSET);
        p(1) = static_cast<RealType>(triangle_bb.min_y_) + RealType(RenderConstants::PIXEL_CENTER_OFFSET);

        RealType eAB_row = edge_func(triangle.vout[0].screen, triangle.vout[1].screen, p);
        RealType eBC_row = edge_func(triangle.vout[1].screen, triangle.vout[2].screen, p);
        RealType eCA_row = edge_func(triangle.vout[2].screen, triangle.vout[0].screen, p);

        // Step increments when moving +1 in X or +1 in Y
        // const MathType eAB_dx = (vout[0].screen(1) - vout[1].screen(1));
        // const MathType eAB_dy = (vout[1].screen(0) - vout[0].screen(0));
        // const MathType eBC_dx = (vout[1].screen(1) - vout[2].screen(1));
        // const MathType eBC_dy = (vout[2].screen(0) - vout[1].screen(0));
        // const MathType eCA_dx = (vout[2].screen(1) - vout[0].screen(1));
        // const MathType eCA_dy = (vout[0].screen(0) - vout[2].screen(0));
        // for y down, the - is needed
        const RealType eAB_dx = (triangle.vout[1].screen(1) - triangle.vout[0].screen(1));
        const RealType eAB_dy = (triangle.vout[0].screen(0) - triangle.vout[1].screen(0));
        const RealType eBC_dx = (triangle.vout[2].screen(1) - triangle.vout[1].screen(1));
        const RealType eBC_dy = (triangle.vout[1].screen(0) - triangle.vout[2].screen(0));
        const RealType eCA_dx = (triangle.vout[0].screen(1) - triangle.vout[2].screen(1));
        const RealType eCA_dy = (triangle.vout[2].screen(0) - triangle.vout[0].screen(0));

    // Rasterize
    draw_triangle_y_loop:
        for (IntType iy = 0; iy < triangle_bb.height_; ++iy)
        {
#pragma HLS loop_tripcount min = 70 max = 70 avg = 70

            IntType texture_y = iy + triangle_bb.min_y_;
            IntType tile_y = texture_y - tile_bb.min_y_;

            const RealType eAB_row_local = RealType(iy) * eAB_dy + eAB_row;
            const RealType eBC_row_local = RealType(iy) * eBC_dy + eBC_row;
            const RealType eCA_row_local = RealType(iy) * eCA_dy + eCA_row;

        draw_triangle_x_loop:
            for (IntType ix = 0; ix < triangle_bb.width_; ++ix)
            {
#pragma HLS loop_tripcount min = 70 max = 70 avg = 70
#pragma HLS loop_flatten
                //    #pragma HLS PIPELINE II = 1

#pragma HLS dependence variable = depth_buffer type = inter false
                //  #pragma HLS dependence variable = depth_buffer type = intra false

#pragma HLS dependence variable = fragment_buffer type = inter false

                IntType texture_x = ix + triangle_bb.min_x_;
                IntType tile_x = texture_x - tile_bb.min_x_;
                IntType tile_address = tile_y * tile_bb.width_ + tile_x;

                RealType prev_depth = depth_buffer[tile_address];

                const RealType eAB = RealType(ix) * eAB_dx + eAB_row_local;
                const RealType eBC = RealType(ix) * eBC_dx + eBC_row_local;
                const RealType eCA = RealType(ix) * eCA_dx + eCA_row_local;

                // Top-left rule adjustments (include pixels on top/left edges)
                const bool inside =
                    (eAB > 0 || (eAB == 0 && tlAB)) &&
                    (eBC > 0 || (eBC == 0 && tlBC)) &&
                    (eCA > 0 || (eCA == 0 && tlCA));

                if (!inside)
                    continue;

                // Baricentric weights normalized
                // const MathType w0 = eBC * inv_area2;
                // const MathType w1 = eCA * inv_area2;
                // const MathType w2 = eAB * inv_area2;
                // Baricentric weights normalized (perpective)
                RealType w0 = eBC * inv_area2 * triangle.vout[0].invW;
                RealType w1 = eCA * inv_area2 * triangle.vout[1].invW;
                RealType w2 = eAB * inv_area2 * triangle.vout[2].invW;

                // Perspective: 1/w at pixel
                const RealType inv_invW_px = RealType(1) / (w0 + w1 + w2);

                w0 *= inv_invW_px;
                w1 *= inv_invW_px;
                w2 *= inv_invW_px;

                typename Derived::Varyings varying_px = Derived::interpolate_varyings(w0, w1, w2,
                                                                                      triangle.vout[0].var,
                                                                                      triangle.vout[1].var,
                                                                                      triangle.vout[2].var);

                // Depth (if needed; same trick)
                RealType depth_px = w0 * triangle.vout[0].depth +
                                    w1 * triangle.vout[1].depth +
                                    w2 * triangle.vout[2].depth;

                // Depth test
                if (prev_depth > RealType(0) && prev_depth < depth_px)
                    continue;

                linalg::Vec4<RealType> gl_FragCoord;
                gl_FragCoord(0) = static_cast<RealType>(texture_x); // + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
                gl_FragCoord(1) = static_cast<RealType>(texture_y); // + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
                gl_FragCoord(2) = depth_px;
                gl_FragCoord(3) = inv_invW_px;

                Fragment fragment;
                Derived::fragment_shader(gl_FragCoord,
                                         uniforms,
                                         varying_px,
                                         intextures,
                                         fragment);

                fragment_buffer[tile_address] = fragment;
                depth_buffer[tile_address] = depth_px;
            }
        }
    }

    // Triangle rasterizer (top-left rule, perspective correct)
    template <typename InTextures, typename Uniforms, typename OutTextures>
    static void draw_triangle_(const Triangle &triangle, const BoundingBox<IntType> &tile_bb, RealType *depth_buffer, const Uniforms &uniforms, const InTextures &intextures, OutTextures &outtextures)
    {
#pragma HLS inline

        BoundingBox<RealType> tri_bb(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen);

        // IntType min_x = max(tile_bb.min_x_, static_cast<IntType>(floor(tri_bb.min_x_)));
        // IntType max_x = min(tile_bb.max_x_, static_cast<IntType>(ceil(tri_bb.max_x_)));
        // IntType min_y = max(tile_bb.min_y_, static_cast<IntType>(floor(tri_bb.min_y_)));
        // IntType max_y = min(tile_bb.max_y_, static_cast<IntType>(ceil(tri_bb.max_y_)));

        IntType min_x = max(tile_bb.min_x_, static_cast<IntType>(tri_bb.min_x_));
        IntType max_x = min(tile_bb.max_x_, static_cast<IntType>(tri_bb.max_x_ + 1));
        IntType min_y = max(tile_bb.min_y_, static_cast<IntType>(tri_bb.min_y_));
        IntType max_y = min(tile_bb.max_y_, static_cast<IntType>(tri_bb.max_y_ + 1));

        BoundingBox<IntType> triangle_bb(min_x, max_x, min_y, max_y);

        // Back-face cull (optional). Keep CCW (area > 0) – adjust sign to your convention
        RealType area2 = edge_func(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen); // 2*area with sign

        if (area2 <= RealType(0))
            return; // enable to cull backfaces

        const RealType inv_area2 = RealType(1) / area2;

        const bool tlAB = is_top_left(triangle.vout[0].screen, triangle.vout[1].screen);
        const bool tlBC = is_top_left(triangle.vout[1].screen, triangle.vout[2].screen);
        const bool tlCA = is_top_left(triangle.vout[2].screen, triangle.vout[0].screen);

        // Evaluate edge functions at top-left corner of each pixel (add +0.5)
        linalg::Vec2<RealType> p;
        p(0) = static_cast<RealType>(triangle_bb.min_x_) + RealType(RenderConstants::PIXEL_CENTER_OFFSET);
        p(1) = static_cast<RealType>(triangle_bb.min_y_) + RealType(RenderConstants::PIXEL_CENTER_OFFSET);

        RealType eAB_row = edge_func(triangle.vout[0].screen, triangle.vout[1].screen, p);
        RealType eBC_row = edge_func(triangle.vout[1].screen, triangle.vout[2].screen, p);
        RealType eCA_row = edge_func(triangle.vout[2].screen, triangle.vout[0].screen, p);

        // Step increments when moving +1 in X or +1 in Y
        // const MathType eAB_dx = (vout[0].screen(1) - vout[1].screen(1));
        // const MathType eAB_dy = (vout[1].screen(0) - vout[0].screen(0));
        // const MathType eBC_dx = (vout[1].screen(1) - vout[2].screen(1));
        // const MathType eBC_dy = (vout[2].screen(0) - vout[1].screen(0));
        // const MathType eCA_dx = (vout[2].screen(1) - vout[0].screen(1));
        // const MathType eCA_dy = (vout[0].screen(0) - vout[2].screen(0));
        // for y down, the - is needed
        const RealType eAB_dx = (triangle.vout[1].screen(1) - triangle.vout[0].screen(1));
        const RealType eAB_dy = (triangle.vout[0].screen(0) - triangle.vout[1].screen(0));
        const RealType eBC_dx = (triangle.vout[2].screen(1) - triangle.vout[1].screen(1));
        const RealType eBC_dy = (triangle.vout[1].screen(0) - triangle.vout[2].screen(0));
        const RealType eCA_dx = (triangle.vout[0].screen(1) - triangle.vout[2].screen(1));
        const RealType eCA_dy = (triangle.vout[2].screen(0) - triangle.vout[0].screen(0));

    // Rasterize
    draw_triangle_y_loop:
        for (IntType iy = 0; iy < triangle_bb.height_; ++iy)
        {
#pragma HLS loop_tripcount min = 70 max = 70 avg = 70

            IntType texture_y = iy + triangle_bb.min_y_;
            IntType tile_y = texture_y - tile_bb.min_y_;

            const RealType eAB_row_local = RealType(iy) * eAB_dy + eAB_row;
            const RealType eBC_row_local = RealType(iy) * eBC_dy + eBC_row;
            const RealType eCA_row_local = RealType(iy) * eCA_dy + eCA_row;

        draw_triangle_x_loop:
            for (IntType ix = 0; ix < triangle_bb.width_; ++ix)
            {
#pragma HLS loop_tripcount min = 70 max = 70 avg = 70
#pragma HLS loop_flatten
                //    #pragma HLS PIPELINE II = 1

#pragma HLS dependence variable = depth_buffer type = inter false
                // #pragma HLS dependence variable = depth_buffer type = intra false

                IntType texture_x = ix + triangle_bb.min_x_;
                IntType tile_x = texture_x - tile_bb.min_x_;
                IntType tile_address = tile_y * tile_bb.width_ + tile_x;

                RealType prev_depth = depth_buffer[tile_address];

                const RealType eAB = RealType(ix) * eAB_dx + eAB_row_local;
                const RealType eBC = RealType(ix) * eBC_dx + eBC_row_local;
                const RealType eCA = RealType(ix) * eCA_dx + eCA_row_local;

                // Top-left rule adjustments (include pixels on top/left edges)
                const bool inside =
                    (eAB > 0 || (eAB == 0 && tlAB)) &&
                    (eBC > 0 || (eBC == 0 && tlBC)) &&
                    (eCA > 0 || (eCA == 0 && tlCA));

                if (!inside)
                    continue;

                // Baricentric weights normalized
                // const MathType w0 = eBC * inv_area2;
                // const MathType w1 = eCA * inv_area2;
                // const MathType w2 = eAB * inv_area2;
                // Baricentric weights normalized (perpective)
                RealType w0 = eBC * inv_area2 * triangle.vout[0].invW;
                RealType w1 = eCA * inv_area2 * triangle.vout[1].invW;
                RealType w2 = eAB * inv_area2 * triangle.vout[2].invW;

                // Perspective: 1/w at pixel
                const RealType inv_invW_px = RealType(1) / (w0 + w1 + w2);

                w0 *= inv_invW_px;
                w1 *= inv_invW_px;
                w2 *= inv_invW_px;

                typename Derived::Varyings varying_px = Derived::interpolate_varyings(w0, w1, w2,
                                                                                      triangle.vout[0].var,
                                                                                      triangle.vout[1].var,
                                                                                      triangle.vout[2].var);

                // Depth (if needed; same trick)
                RealType depth_px = w0 * triangle.vout[0].depth +
                                    w1 * triangle.vout[1].depth +
                                    w2 * triangle.vout[2].depth;

                // Depth test
                if (prev_depth > RealType(0) && prev_depth < depth_px)
                    continue;

                Vec4<RealType> gl_FragCoord;
                gl_FragCoord(0) = static_cast<RealType>(texture_x); // + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
                gl_FragCoord(1) = static_cast<RealType>(texture_y); // + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
                gl_FragCoord(2) = depth_px;
                gl_FragCoord(3) = inv_invW_px;

                Derived::fragment_shader(gl_FragCoord,
                                         uniforms,
                                         varying_px,
                                         intextures,
                                         outtextures);

                depth_buffer[tile_address] = depth_px;
            }
        }
    }

    // Derived &derived_() { return *static_cast<Derived *>(this); }
    // const Derived &derived_() const { return *static_cast<const Derived *>(this); }

    // linalg::Mat4<MathType> opencv2opengl_;
};
/*
template <typename MathType, typename OutType, template <class> class Texture>
class GouraudRendererBase
{
public:
    struct InTextures
    {
        const MathType notused;
    };

    struct OutTextures
    {
        Texture<linalg::Vec3<OutType>> &out_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec3<MathType> normal;
    };

    struct InVaryings
    {
        linalg::Vec3<OutType> vColor;
    };

    struct OutVaryings
    {
        linalg::Vec3<OutType> vColor;
    };

    struct Fragment
    {
        linalg::Vec3<OutType> color;
    };

    GouraudRendererBase() = default;
    ~GouraudRendererBase() = default;

    template <class Mesh>
    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.normal(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.nor_offset_ + 0];
        vertexdata.normal(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.nor_offset_ + 1];
        vertexdata.normal(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.nor_offset_ + 2];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        // #pragma HLS inline

        Varyings var_over_w_px;
        var_over_w_px.vColor =
            (w0 * varying_px0.vColor +
             w1 * varying_px1.vColor +
             w2 * varying_px2.vColor);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        // Transform to world space
        linalg::Vec3<MathType> fragPos = linalg::Vec3<MathType>(uModel_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1)));
        linalg::Vec3<MathType> N = (uNormalMatrix_ * vertexdata.normal).normalized();

        // Lighting vectors
        linalg::Vec3<MathType> L = (uLightPos_ - fragPos).normalized();
        linalg::Vec3<MathType> V = (uViewPos_ - fragPos).normalized();
        MathType n_dot_l = N.dot(L);
        linalg::Vec3<MathType> R = L - MathType(2) * n_dot_l * N; // opengls reflect

        // Phong reflectance model (computed per-vertex)
        MathType NdotL = max(n_dot_l, MathType(0));
        MathType spec = 0.0;
        if (NdotL > 0.0)
        {
            spec = pow(max(V.dot(R), MathType(0)), uShininess_);
        }

        linalg::Vec3<MathType> ambient = uAmbientLight_ * uKa_;
        linalg::Vec3<MathType> diffuse = uLightColor_ * uKd_ * NdotL;
        linalg::Vec3<MathType> specular = uLightColor_ * uKs_ * spec;

        outVarying.vColor = ambient + diffuse + specular;

        gl_Position = uProjection_ * uView_ * linalg::Vec4<MathType>(fragPos, MathType(1));
    }

    void fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         const InTextures &intextures,
                         Fragment &fragment)
    {
        // #pragma HLS INLINE
        //  std::cout << "calling fragment shader " << std::endl;
        //  if (!inside)
        //     return;

        fragment.color = in_varying.vColor;
    }

    void fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         const InTextures &intextures,
                         OutTextures &outtextures)
    {
        // #pragma HLS INLINE

        outtextures.out_texture.set_texel_(in_varying.vColor, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

    linalg::Mat4<MathType> uModel_;        // model to world space
    linalg::Mat4<MathType> uView_;         // world space to camera space
    linalg::Mat4<MathType> uProjection_;   // camera space to clip space
    linalg::Mat3<MathType> uNormalMatrix_; // transpose(inverse(mat3(uModel))) computed on CPU

    linalg::Vec3<MathType> uLightPos_; // world space
    linalg::Vec3<MathType> uViewPos_;  // camera position in world space

    // Material and light
    linalg::Vec3<MathType> uKa_;           // ambient reflectance (rgb)
    linalg::Vec3<MathType> uKd_;           // diffuse reflectance (rgb)
    linalg::Vec3<MathType> uKs_;           // specular reflectance (rgb)
    MathType uShininess_;                  // specular exponent
    linalg::Vec3<MathType> uLightColor_;   // light color/intensity (rgb)
    linalg::Vec3<MathType> uAmbientLight_; // ambient light (rgb)

    unsigned int out_lvl_;

    Fragment nodata_;
};
*/
// -----------------------------------------------------------------------------
// DepthRenderer
//   Evout[0].screen(0)mple derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

template <template <class> class Texture>
class DepthRendererBase
{
public:
    DepthRendererBase() = delete;
    //~DepthRendererBase() = delete;

    struct InTextures
    {
        const RealType unused;
    };

    struct OutTextures
    {
        Texture<float> &out_texture;
    };

    struct VertexData
    {
        linalg::Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        linalg::Mat4<RealType> t_matrix;
        int out_lvl;
    };

    struct Varyings
    {
        RealType depth;
    };

    struct Fragment
    {
        RealType depth;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS INLINE

        return Fragment{RealType(textures.out_texture.nodata())};
    }

    template <class Mesh>
    static VertexData get_vertex_data(const Mesh &mesh, const IntType vertexid)
    {
#pragma HLS INLINE

        VertexData vertexdata;
        IntType base = vertexid * mesh.stride_ + mesh.pos_offset_;
        vertexdata.vertex(0) = mesh.vertex_buffer_[base + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[base + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
#pragma HLS INLINE

        Varyings var_over_w_px;
        var_over_w_px.depth =
            (w0 * varying_px0.depth +
             w1 * varying_px1.depth +
             w2 * varying_px2.depth);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              linalg::Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
#pragma HLS INLINE
        //   std::cout << "calling vertex shader " << std::endl;
        gl_Position = uniforms.t_matrix * linalg::Vec4<RealType>(vertexdata.vertex, RealType(1));
        outVarying.depth = gl_Position(2);
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
#pragma HLS INLINE

        RealType depth = in_varying.depth;
        fragment.depth = depth;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                OutTextures &outtextures)
    {
#pragma HLS INLINE
        outtextures.out_texture.set_texel_(in_varying.depth, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
    }
};

// -----------------------------------------------------------------------------
// ImageRendererBase
//   Another evout[0].screen(0)mple derived class that might output color
// -----------------------------------------------------------------------------

template <template <class> class Texture>
class ImageRendererBase
{
public:
    ImageRendererBase() = delete;
    //~ImageRendererBase() = default;

    struct InTextures
    {
        const Texture<ImageType> &in_texture;
    };

    struct OutTextures
    {
        Texture<ImageType> &out_texture;
    };

    struct VertexData
    {
        linalg::Vec3<RealType> vertex;
        linalg::Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        linalg::Mat4<RealType> t_matrix;
        IntType in_lvl;
        IntType out_lvl;
    };

    struct Varyings
    {
        linalg::Vec2<RealType> texcoord;
    };

    struct Fragment
    {
        RealType color;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        return Fragment{RealType(textures.out_texture.nodata())};
    }

    template <class Mesh>
    static VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
#pragma HLS inline

        VertexData vertexdata;

        IntType base = vertexid * mesh.stride_;

        vertexdata.vertex(0) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
#pragma HLS inline

        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              linalg::Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
#pragma HLS inline

        gl_Position = uniforms.t_matrix * linalg::Vec4<RealType>(vertexdata.vertex, RealType(1));
        outVarying.texcoord = vertexdata.texcoord;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
#pragma HLS inline

        RealType pix = sample(intextures.in_texture, in_varying.texcoord(1), in_varying.texcoord(0), uniforms.in_lvl);
        fragment.color = pix;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                OutTextures &outtextures)
    {
#pragma HLS inline

        RealType pix = sample(intextures.in_texture, in_varying.texcoord(1), in_varying.texcoord(0), uniforms.in_lvl);
        outtextures.out_texture.set_texel_(pix, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
    }
};

template <template <class> class Texture>
class ResidualRendererBase
{
public:
    ResidualRendererBase() = delete;
    //~ResidualRendererBase() = default;

    struct InTextures
    {
        const Texture<ImageType> &kf_texture;
        const Texture<ImageType> &f_texture;
    };

    struct OutTextures
    {
        Texture<float> &r_texture;
    };

    struct VertexData
    {
        linalg::Vec3<RealType> vertex;
        linalg::Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        linalg::Mat4<RealType> t_matrix;
        IntType in_lvl;
        IntType out_lvl;
    };

    struct Varyings
    {
        linalg::Vec2<RealType> texcoord;
    };

    struct Fragment
    {
        RealType error;
    };

    template <class Mesh>
    static VertexData get_vertex_data(const Mesh &mesh, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * mesh.stride_;

        vertexdata.vertex(0) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              linalg::Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        gl_Position = uniforms.t_matrix * linalg::Vec4<RealType>(vertexdata.vertex, RealType(1));
        outVarying.texcoord = vertexdata.texcoord;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        // IntType width = intextures.kf_texture.width(uniforms.in_lvl);
        // IntType height = intextures.kf_texture.height(uniforms.in_lvl);

        // linalg::Vec2<RealType> screen_texcoord(gl_FragCoord(0) / RealType(width), gl_FragCoord(1) / RealType(height));

        RealType kf = sample(intextures.kf_texture, in_varying.texcoord(1), in_varying.texcoord(0), uniforms.in_lvl);
        unsigned char f = intextures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        // float f = f_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        // if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata())
        //     return;

        RealType e = RealType(f) - RealType(kf);

        fragment.error = e;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                OutTextures &outtextures)
    {
        // #pragma HLS inline

        // IntType width = intextures.kf_texture.width(uniforms.out_lvl);
        // IntType height = intextures.kf_texture.height(uniforms.out_lvl);

        // linalg::Vec2<RealType> screen_texcoord(gl_FragCoord(0) / RealType(width), gl_FragCoord(1) / RealType(height));

        RealType kf = sample(intextures.kf_texture, in_varying.texcoord(1), in_varying.texcoord(0), uniforms.in_lvl);
        unsigned char f = intextures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        // float f = f_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        // if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata())
        //     return;

        RealType e = RealType(f) - RealType(kf);

        outtextures.r_texture.set_texel_(e, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
    }
};

template <template <class> class Texture>
class DIDxyRendererBase
{
public:
    DIDxyRendererBase() = delete;
    //~DIDxyRendererBase() = default;

    struct InTextures
    {
        const Texture<ImageType> &in_texture;
    };
    struct OutTextures
    {
        Texture<linalg::Vec3<float>> &out_texture;
    };

    struct VertexData
    {
        linalg::Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        IntType in_lvl;
        IntType out_lvl;
    };

    struct Varyings
    {
        linalg::Vec2<RealType> texcoord;
    };

    struct Fragment
    {
        linalg::Vec3<RealType> didxy;
    };

    template <class Mesh>
    static VertexData get_vertex_data(const Mesh &mesh, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * mesh.stride_;

        vertexdata.texcoord(0) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              linalg::Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        // gl_Position = (view_matrix * pose_matrix) * linalg::Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = linalg::Vec4<RealType>(RealType(2) * vertexdata.texcoord(0) - RealType(1), RealType(2) * vertexdata.texcoord(1) - RealType(1), RealType(0), RealType(1));
        outVarying.texcoord = vertexdata.texcoord;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType height = intextures.in_texture.height(uniforms.in_lvl);
        IntType width = intextures.in_texture.width(uniforms.in_lvl);
        unsigned char nodata = intextures.in_texture.nodata();

        IntType x = IntType(in_varying.texcoord(0) * RealType(width - 1));
        IntType y = IntType(in_varying.texcoord(1) * RealType(height - 1));
        IntType x_p = x + 1;
        IntType x_m = x - 1;
        IntType y_p = y + 1;
        IntType y_m = y - 1;

        if (x_p >= width || x_m < 0 || y_p >= height || y_m < 0)
        {
            //  No need to explicitly set to nodata, it is already in the background color
            return;
        }

        unsigned char f = intextures.in_texture.texel_(y, x, uniforms.in_lvl);
        unsigned char f_y_p = intextures.in_texture.texel_(y_p, x, uniforms.in_lvl);
        unsigned char f_y_m = intextures.in_texture.texel_(y_m, x, uniforms.in_lvl);
        unsigned char f_x_p = intextures.in_texture.texel_(y, x_p, uniforms.in_lvl);
        unsigned char f_x_m = intextures.in_texture.texel_(y, x_m, uniforms.in_lvl);

        // if (f_x_p == nodata || f_x_m == nodata ||
        //     f_y_p == nodata || f_y_m == nodata || f == nodata)
        //{
        //   No need to explicitly set to nodata, it is already in the background color
        //    return;
        //}

        linalg::Vec3<RealType> out_fragment;
        out_fragment(0) = (RealType(f_x_p) - RealType(f_x_m)) / RealType(2);
        out_fragment(1) = (RealType(f_y_p) - RealType(f_y_m)) / RealType(2);
        out_fragment(2) = RealType(0); // f; // save the projected frame for later processing

        fragment.didxy = out_fragment;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                OutTextures &outtextures)
    {
        // #pragma HLS inline

        IntType height = intextures.in_texture.height(uniforms.in_lvl);
        IntType width = intextures.in_texture.width(uniforms.in_lvl);
        // unsigned char nodata = intextures.in_texture.nodata();

        IntType x = IntType(in_varying.texcoord(0) * RealType(width - 1));
        IntType y = IntType(in_varying.texcoord(1) * RealType(height - 1));
        IntType x_p = x + 1;
        IntType x_m = x - 1;
        IntType y_p = y + 1;
        IntType y_m = y - 1;

        if (x_p >= width || x_m < 0 || y_p >= height || y_m < 0)
        {
            //  No need to explicitly set to nodata, it is already in the background color
            return;
        }

        unsigned char f = intextures.in_texture.texel_(y, x, uniforms.in_lvl);
        unsigned char f_y_p = intextures.in_texture.texel_(y_p, x, uniforms.in_lvl);
        unsigned char f_y_m = intextures.in_texture.texel_(y_m, x, uniforms.in_lvl);
        unsigned char f_x_p = intextures.in_texture.texel_(y, x_p, uniforms.in_lvl);
        unsigned char f_x_m = intextures.in_texture.texel_(y, x_m, uniforms.in_lvl);

        // if (f_x_p == nodata || f_x_m == nodata ||
        //     f_y_p == nodata || f_y_m == nodata || f == nodata)
        //{
        //   No need to explicitly set to nodata, it is already in the background color
        //    return;
        //}

        linalg::Vec3<RealType> out_fragment;
        out_fragment(0) = (RealType(f_x_p) - RealType(f_x_m)) / RealType(2);
        out_fragment(1) = (RealType(f_y_p) - RealType(f_y_m)) / RealType(2);
        out_fragment(2) = RealType(0); // f; // save the projected frame for later processing

        outtextures.out_texture.set_texel_(out_fragment, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
    }
};

template <template <class> class Texture>
class JPoseRendererBase
{
public:
    JPoseRendererBase() = delete;
    //~JPoseRendererBase() = default;

    struct InTextures
    {
        const Texture<ImageType> &kf_texture;
        const Texture<ImageType> &f_texture;
        const Texture<linalg::Vec3<float>> &dfdxy_texture;
    };

    struct OutTextures
    {
        Texture<linalg::Vec3<float>> &jtra_texture;
        Texture<linalg::Vec3<float>> &jrot_texture;
        Texture<float> &r_texture;
    };

    struct VertexData
    {
        linalg::Vec3<RealType> vertex;
        linalg::Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        linalg::Mat4<RealType> view_matrix;
        linalg::Mat4<RealType> pose_matrix;
        IntType in_lvl;
        IntType out_lvl;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        linalg::Vec2<RealType> texcoord;
        linalg::Vec3<RealType> f_ver;
    };

    struct Fragment
    {
        linalg::Vec3<RealType> jtra;
        linalg::Vec3<RealType> jrot;
        RealType r;
    };

    template <class Mesh>
    static VertexData get_vertex_data(const Mesh &mesh, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * mesh.stride_;

        vertexdata.vertex(0) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              linalg::Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        linalg::Vec4<RealType> f_ver = uniforms.pose_matrix * linalg::Vec4<RealType>(vertexdata.vertex, RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        outVarying.f_ver = linalg::Vec3<RealType>(f_ver);
        outVarying.texcoord = vertexdata.texcoord;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        // IntType in_width = intextures.kf_texture.width(uniforms.in_lvl);
        // IntType in_height = intextures.kf_texture.height(uniforms.in_lvl);
        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        // linalg::Vec2<RealType> screen_texcoord(gl_FragCoord(0) / RealType(width), gl_FragCoord(1) / RealType(height));

        linalg::Vec3<RealType> f_ver = in_varying.f_ver;
        linalg::Vec2<RealType> texcoord = in_varying.texcoord;

        RealType kf = sample(intextures.kf_texture, texcoord(1), texcoord(0), uniforms.in_lvl);
        unsigned char f = intextures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        linalg::Vec3<RealType> f_der = intextures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        // float f = sample<T, Texture<MathType>>(textures.f_texture, screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);
        // linalg::Vec3<MathType>f_der = sample<Vec3, Texture<Vec3>>(textures.dfdxy_texture, screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        // if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata() || f_der == textures.dfdxy_texture.nodata())
        //     return;

        RealType r = RealType(f) - RealType(kf);

        RealType v0 = f_der(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        RealType v1 = f_der(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        RealType v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        linalg::Vec3<RealType> d_f_i_d_tra = linalg::Vec3<RealType>(v0, v1, v2);
        linalg::Vec3<RealType> d_f_i_d_rot = linalg::Vec3<RealType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        fragment.jtra = d_f_i_d_tra;
        fragment.jrot = d_f_i_d_rot;
        fragment.r = r;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                OutTextures &outtextures)
    {
        // IntType in_width = intextures.kf_texture.width(uniforms.in_lvl);
        // IntType in_height = intextures.kf_texture.height(uniforms.in_lvl);

        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        // linalg::Vec2<RealType> screen_texcoord(gl_FragCoord(0) / RealType(width), gl_FragCoord(1) / RealType(height));

        linalg::Vec3<RealType> f_ver = in_varying.f_ver;
        linalg::Vec2<RealType> texcoord = in_varying.texcoord;

        RealType kf = sample(intextures.kf_texture, texcoord(1), texcoord(0), uniforms.in_lvl);
        unsigned char f = intextures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        linalg::Vec3<RealType> f_der = intextures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        // float f = sample<T, Texture<MathType>>(textures.f_texture, screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);
        // linalg::Vec3<MathType>f_der = sample<Vec3, Texture<Vec3>>(textures.dfdxy_texture, screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        // if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata() || f_der == textures.dfdxy_texture.nodata())
        //     return;

        RealType r = RealType(f) - RealType(kf);

        RealType v0 = f_der(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        RealType v1 = f_der(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        RealType v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        linalg::Vec3<RealType> d_f_i_d_tra = linalg::Vec3<RealType>(v0, v1, v2);
        linalg::Vec3<RealType> d_f_i_d_rot = linalg::Vec3<RealType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        outtextures.jtra_texture.set_texel_(d_f_i_d_tra, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        outtextures.jrot_texture.set_texel_(d_f_i_d_rot, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        outtextures.r_texture.set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
    }
};

template <template <class> class Texture>
class JMapRendererBase
{
public:
    JMapRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        const Texture<ImageType> &kf_texture;
        const Texture<ImageType> &f_texture;
        const Texture<linalg::Vec3<float>> &dfdxy_texture;
    };

    struct OutTextures
    {
        Texture<linalg::Vec3<float>> &jmap_texture;
        Texture<linalg::Vec3<PidType>> &pids_texture;
        Texture<float> &r_texture;
    };

    struct VertexData
    {
        linalg::Vec3<RealType> vertex;
        linalg::Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        linalg::Mat4<RealType> view_matrix;
        linalg::Mat4<RealType> pose_matrix;
        IntType in_lvl;
        IntType out_lvl;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        linalg::Vec2<RealType> texcoord;
        linalg::Vec3<RealType> f_ver;
        linalg::Vec3<RealType> kf_ray;
        RealType depth;
        linalg::Vec3<RealType> baricentric;
        IntType vertexId;
        linalg::Vec3<IntType> pids;
    };

    struct Fragment
    {
        linalg::Vec3<RealType> jmap;
        linalg::Vec3<IntType> pids;
        RealType r;
    };

    template <class Mesh>
    static VertexData get_vertex_data(const Mesh &mesh, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * mesh.stride_;

        vertexdata.vertex(0) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray =
            (w0 * varying_px0.kf_ray +
             w1 * varying_px1.kf_ray +
             w2 * varying_px2.kf_ray);
        // var_over_w_px.barvout[2].screen(1)entric = linalg::Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = linalg::Vec3<RealType>(w0,
                                                           w1,
                                                           w2);
        var_over_w_px.pids = linalg::Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              linalg::Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        linalg::Vec4<RealType> f_ver = uniforms.pose_matrix * linalg::Vec4<RealType>(vertexdata.vertex, RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        linalg::Vec3<RealType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), RealType(1));
        linalg::Vec4<RealType> d_f_ver_d_kf_depth_ = uniforms.pose_matrix * linalg::Vec4<RealType>(kf_ray, RealType(0));
        linalg::Vec3<RealType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = linalg::Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.texcoord = vertexdata.texcoord;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        // IntType in_width = intextures.kf_texture.width(uniforms.in_lvl);
        // IntType in_height = intextures.kf_texture.height(uniforms.in_lvl);

        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        // linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        linalg::Vec3<RealType> f_ver = in_varying.f_ver;
        linalg::Vec3<RealType> kf_ray = in_varying.kf_ray;
        linalg::Vec2<RealType> texcoord = in_varying.texcoord;
        linalg::Vec3<RealType> baricentric = in_varying.baricentric;
        linalg::Vec3<IntType> vertexid = in_varying.pids;

        RealType kf = sample(intextures.kf_texture, texcoord(1), texcoord(0), uniforms.in_lvl);
        ImageType f = intextures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        linalg::Vec3<RealType> f_der = intextures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        // float f = f_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);
        // linalg::Vec3<MathType>f_der = dfdxy_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        // if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata() || f_der == textures.dfdxy_texture.nodata())
        //     return;

        RealType r = RealType(f) - RealType(kf);

        linalg::Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = f_der(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = f_der(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // linalg::Vec3<MathType>d_f_i_d_tra = linalg::Vec3<MathType>(v0, v1, v2);
        // linalg::Vec3<MathType>d_f_i_d_rot = linalg::Vec3<MathType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        linalg::Vec3<RealType> d_f_ver_d_kf_depth = kf_ray; // kfTofPose.rotationMatrix() * kf_ray;
        RealType d_f_i_d_kf_depth = (d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth)(0, 0);

        linalg::Vec3<RealType> d_depth_d_vert_depth = baricentric;

        linalg::Vec3<RealType> jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        linalg::Vec3<IntType> ids = linalg::Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.jmap = jac;
        fragment.pids = ids;
        fragment.r = r;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                OutTextures &outtextures)
    {
        // IntType in_width = intextures.kf_texture.width(uniforms.in_lvl);
        // IntType in_height = intextures.kf_texture.height(uniforms.in_lvl);

        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        // linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        linalg::Vec3<RealType> f_ver = in_varying.f_ver;
        linalg::Vec3<RealType> kf_ray = in_varying.kf_ray;
        linalg::Vec2<RealType> texcoord = in_varying.texcoord;
        linalg::Vec3<RealType> baricentric = in_varying.baricentric;
        linalg::Vec3<IntType> vertexid = in_varying.pids;

        RealType kf = sample(intextures.kf_texture, texcoord(1), texcoord(0), uniforms.in_lvl);
        ImageType f = intextures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        linalg::Vec3<RealType> f_der = intextures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        // float f = f_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);
        // linalg::Vec3<MathType>f_der = dfdxy_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        // if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata() || f_der == textures.dfdxy_texture.nodata())
        //     return;

        RealType r = RealType(f) - RealType(kf);

        linalg::Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = f_der(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = f_der(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // linalg::Vec3<MathType>d_f_i_d_tra = linalg::Vec3<MathType>(v0, v1, v2);
        // linalg::Vec3<MathType>d_f_i_d_rot = linalg::Vec3<MathType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        linalg::Vec3<RealType> d_f_ver_d_kf_depth = kf_ray; // kfTofPose.rotationMatrix() * kf_ray;
        RealType d_f_i_d_kf_depth = (d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth)(0, 0);

        linalg::Vec3<RealType> d_depth_d_vert_depth = baricentric;

        linalg::Vec3<RealType> jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        linalg::Vec3<IntType> ids = linalg::Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        outtextures.jmap_texture.set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        outtextures.pids_texture.set_texel_(ids, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        outtextures.r_texture.set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
    }
};

template <template <class> class Texture>
class DiffRendererBase
{
public:
    DiffRendererBase() = delete;
    //~DiffRendererBase() = default;

    struct InTextures
    {
        const Texture<ImageType> &diffuse_texture;
    };

    struct OutTextures
    {
        Texture<ImageType> &image_texture;
        Texture<float> &depth_texture;
        Texture<linalg::Vec3<float>> &jtra_texture;
        Texture<linalg::Vec3<float>> &jrot_texture;
        Texture<linalg::Vec3<float>> &jmap_texture;
        Texture<linalg::Vec3<PidType>> &pids_texture;
    };

    struct VertexData
    {
        linalg::Vec3<RealType> vertex;
        linalg::Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        linalg::Mat4<RealType> view_matrix;
        linalg::Mat4<RealType> pose_matrix;
        IntType in_lvl;
        IntType out_lvl;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        linalg::Vec2<RealType> texcoord;
        linalg::Vec3<RealType> f_ver;
        linalg::Vec3<RealType> kf_ray;
        // MathType depth;
        linalg::Vec3<RealType> baricentric;
        IntType vertexId;
        linalg::Vec3<IntType> pids;
    };

    struct Fragment
    {
        RealType image;
        RealType depth;
        linalg::Vec3<RealType> jtra;
        linalg::Vec3<RealType> jrot;
        linalg::Vec3<RealType> jmap;
        linalg::Vec3<IntType> pids;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        return Fragment{RealType(textures.image_texture.nodata()),
                        RealType(textures.depth_texture.nodata()),
                        linalg::Vec3<RealType>(textures.jtra_texture.nodata()),
                        linalg::Vec3<RealType>(textures.jrot_texture.nodata()),
                        linalg::Vec3<RealType>(textures.jmap_texture.nodata()),
                        linalg::Vec3<IntType>(textures.pids_texture.nodata())};
    }

    template <class Mesh>
    static VertexData get_vertex_data(const Mesh &mesh, const IntType vertexid)
    {
#pragma HLS inline

        VertexData vertexdata;

        IntType base = vertexid * mesh.stride_;

        vertexdata.vertex(0) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[base + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[base + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
#pragma HLS inline

        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray =
            (w0 * varying_px0.kf_ray +
             w1 * varying_px1.kf_ray +
             w2 * varying_px2.kf_ray);
        var_over_w_px.baricentric = linalg::Vec3<RealType>(w0,
                                                           w1,
                                                           w2);

        var_over_w_px.pids = linalg::Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              linalg::Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
#pragma HLS inline

        Vec4<RealType> f_ver = uniforms.pose_matrix * linalg::Vec4<RealType>(vertexdata.vertex, RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        Vec3<RealType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), RealType(1));
        Vec3<RealType> d_f_ver_d_kf_depth = linalg::Vec3<RealType>(uniforms.pose_matrix * linalg::Vec4<RealType>(kf_ray, RealType(0)));
        // linalg::Vec3<MathType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = linalg::Vec3<RealType>(f_ver);
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        // outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.texcoord = vertexdata.texcoord;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
#pragma HLS inline

        IntType in_width = intextures.diffuse_texture.width(uniforms.in_lvl);
        IntType in_height = intextures.diffuse_texture.height(uniforms.in_lvl);

        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> kf_ray = in_varying.kf_ray;
        Vec2<RealType> texcoord = in_varying.texcoord;
        Vec3<RealType> baricentric = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        // MathType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        RealType f = sample(intextures.diffuse_texture, in_varying.texcoord(1), in_varying.texcoord(0), uniforms.in_lvl);
        // if (f == textures.diffuse_texture.nodata())
        //     return;

        Vec3<RealType> f_der = compute_didxy<RealType, Vec3, Texture<unsigned char>>(intextures.diffuse_texture, IntType(in_varying.texcoord(1) * RealType(in_height)), IntType(in_varying.texcoord(0) * RealType(in_width)), uniforms.in_lvl);

        // if (f_der(0) == textures.diffuse_texture.nodata() && f_der(1) == textures.diffuse_texture.nodata())
        //     return;

        linalg::Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = f_der(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = f_der(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // Vec3<MathType>d_f_i_d_tra = Vec3<MathType>(v0, v1, v2);
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2), f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2), -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        Vec3<RealType> d_f_ver_d_kf_depth = kf_ray; // kfTofPose.rotationMatrix() * kf_ray;
        RealType d_f_i_d_kf_depth = (d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth)(0, 0);

        Vec3<RealType> d_depth_d_vert_depth = baricentric;

        Vec3<RealType> jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.image = f;
        fragment.depth = f_ver(2);
        fragment.jtra = d_f_i_d_f_ver;
        fragment.jrot = d_f_i_d_rot;
        fragment.jmap = jac;
        fragment.pids = ids;
    }

    static void fragment_shader(const linalg::Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                OutTextures &outtextures)
    {
#pragma HLS inline

        IntType in_width = intextures.diffuse_texture.width(uniforms.in_lvl);
        IntType in_height = intextures.diffuse_texture.height(uniforms.in_lvl);

        IntType out_width = uniforms.out_width;   // outtextures.image_texture.width(uniforms.out_lvl);
        IntType out_height = uniforms.out_height; // outtextures.image_texture.height(uniforms.out_lvl);

        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> kf_ray = in_varying.kf_ray;
        Vec2<RealType> texcoord = in_varying.texcoord;
        Vec3<RealType> baricentric = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        // MathType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        RealType f = sample(intextures.diffuse_texture, in_varying.texcoord(1), in_varying.texcoord(0), uniforms.in_lvl);
        // if (f == textures.diffuse_texture.nodata())
        //     return;

        Vec3<RealType> f_der = compute_didxy<RealType, Vec3, Texture<ImageType>>(intextures.diffuse_texture, IntType(in_varying.texcoord(1) * RealType(in_height)), IntType(in_varying.texcoord(0) * RealType(in_width)), uniforms.in_lvl);

        // if (f_der(0) == textures.diffuse_texture.nodata() && f_der(1) == textures.diffuse_texture.nodata())
        //     return;

        Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = f_der(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = f_der(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // Vec3<MathType>d_f_i_d_tra = Vec3<MathType>(v0, v1, v2);
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2), f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2), -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        Vec3<RealType> d_f_ver_d_kf_depth = kf_ray; // kfTofPose.rotationMatrix() * kf_ray;
        RealType d_f_i_d_kf_depth = (d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth)(0, 0);

        Vec3<RealType> d_depth_d_vert_depth = baricentric;

        Vec3<RealType> jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        outtextures.image_texture.set_texel_(f, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        outtextures.depth_texture.set_texel_(f_ver(2), gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        outtextures.jtra_texture.set_texel_(d_f_i_d_f_ver, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        outtextures.jrot_texture.set_texel_(d_f_i_d_rot, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        outtextures.jmap_texture.set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        outtextures.pids_texture.set_texel_(ids, gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
    }
};
