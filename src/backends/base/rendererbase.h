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
#pragma HLS INLINE
    //   return (y1 - y0) * (px - x0) + (x0 - x1) * (py - y0);
    //    return (by - ay) * px + (ax - bx) * py + (bx * ay - ax * by);
    // Vec2<T> v10 = v1 - v0;
    // Vec2<T> v20 = v2 - v0;
    // for y up
    // return v10.cross(v20);
    // for y down
    // return v20.cross(v10);
    // return -((v2(1) - v0(1)) * (v1(0) - v0(0)) - (v2(0) - v0(0)) * (v1(1) - v0(1)));
    return (v2(0) - v0(0)) * (v1(1) - v0(1)) - (v2(1) - v0(1)) * (v1(0) - v0(0));
}

// Top-left test: returns true if edge is a "top" or "left" edge
template <typename T>
bool is_top_left(const Vec2<T> &v0, const Vec2<T> &v1)
{
#pragma HLS INLINE
    //   return (v0(1) == v1(1)) ? (v1(0) < v0(0)) : (v0(1) < v1(1));
    //   for y up
    //   return (v0(1) < v1(1)) || (v0(1) == v1(1) && v0(0) > v1(0));
    //   for y down
    return (v0(1) > v1(1)) || (v0(1) == v1(1) && v0(0) > v1(0));
}

// Vertex shading & clip → NDC → screen
template <typename Derived>
struct VSOut
{
    Vec2<RealType> screen; // x,y in pixel space (float)
    RealType depth;        // z in [0,1] if your projection is like GL_ZERO_TO_ONE
    RealType invW;         // 1 / clip.w
    // std::tuple<Varyings...> var_over_w; // varyings multiplied by invW
    typename Derived::Varyings var; // original varyings (for convenience)
};

template <typename Derived>
struct Triangle
{
    VSOut<Derived> vout[3];
    IntType id;
};

static void create_tile_viewports(BoundingBox<IntType> viewport_tiles[],
                                  const BoundingBox<IntType> &viewport,
                                  IntType num_tiles_x, IntType num_tiles_y)
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

template <typename Derived>
static Triangle<Derived> create_triangle(const typename Derived::VertexData vertexdata[3],
                                         const IntType vertexids[3],
                                         IntType triangle_id,
                                         const BoundingBox<IntType> &viewport,
                                         const typename Derived::Uniforms &uniforms)
{
    Triangle<Derived> triangle;

    triangle.id = triangle_id;

    // #pragma HLS INLINE off
create_triangle_loop:
    for (int j = 0; j < 3; ++j)
    {
        // #pragma HLS UNROLL

        Vec4<RealType> gl_Position;
        typename Derived::Varyings outvaryings;
        typename Derived::VertexData vertexdata_ = vertexdata[j];
        IntType vertexid_ = vertexids[j];
        Derived::vertex_shader(vertexdata_,
                               vertexid_,
                               uniforms,
                               gl_Position,
                               outvaryings);

        const RealType invW = RealType(1) / gl_Position(3);
        const RealType ndc_x = gl_Position(0) * invW; // [-1,1]
        const RealType ndc_y = gl_Position(1) * invW;
        const RealType ndc_z = gl_Position(2) * invW; // assumed 0..1 after proj (adjust if -1..1)

        // #ifndef USE_VITIS
        // assert(ndc_x >= -1 && ndc_x <= 1 && ndc_y >= -1 && ndc_y <= 1 && ndc_z >= -1 && ndc_z <= 1);
        // #endif

        VSOut<Derived> vsout;
        // pixel-space (don’t clamp here) — match GL rasterization (remove +1/-0.5 adjustment)
        vsout.screen(0) = RealType(0.5) * (ndc_x + RealType(1)) * viewport.width_ + viewport.min_x_;
        vsout.screen(1) = RealType(0.5) * (ndc_y + RealType(1)) * viewport.height_ + viewport.min_y_;
        // triangle.vout[j].depth = RealType(0.5) * (ndc_z + RealType(1));
        vsout.depth = ndc_z;
        vsout.invW = invW;
        vsout.var = outvaryings;
        // vout[i].var_over_w = varyings * invW; // requires T*VaryingType

        triangle.vout[j] = vsout;
    }

    return triangle;
}

template <typename VertexBufferView, typename EboBufferView, typename Derived>
static void get_triangles(const VertexBufferView &vertex_buffer,
                          const EboBufferView &ebo_buffer,
                          const BoundingBox<IntType> &viewport,
                          const BoundingBox<IntType> &tile_viewport,
                          const typename Derived::Uniforms &uniforms,
                          Triangle<Derived> triangles[],
                          IntType max_num_triangles,
                          IntType &num_triangles)
{
    // #pragma HLS INLINE off

    int total_num_triangles = ebo_buffer.size() / 3;

    num_triangles = 0;

    // Loop over triangles
renderbase_render_triangles_loop:
    for (IntType tri_id = 0; tri_id < total_num_triangles; tri_id++)
    {

#pragma HLS loop_tripcount min = 768 max = 768 avg = 768
        // #pragma HLS PIPELINE II = 1

        IntType vertexids[3];

        vertexids[0] = ebo_buffer[tri_id * 3 + 0];
        vertexids[1] = ebo_buffer[tri_id * 3 + 1];
        vertexids[2] = ebo_buffer[tri_id * 3 + 2];

        typename Derived::VertexData vertexdata[3];

        vertexdata[0] = Derived::get_vertex_data(vertex_buffer, vertexids[0]);
        vertexdata[1] = Derived::get_vertex_data(vertex_buffer, vertexids[1]);
        vertexdata[2] = Derived::get_vertex_data(vertex_buffer, vertexids[2]);

        Triangle<Derived> triangle = create_triangle<Derived>(vertexdata, vertexids, tri_id, viewport, uniforms);

        // use tile binning
        // Back-face cull (optional). Keep CCW (area > 0) – adjust sign to your convention
        RealType area2 = edge_func(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen); // 2*area with sign

        if (area2 < RealType(0))
            continue; // enable to cull backfaces

        // Triangle bounding box (float → int, clamp to viewport)
        BoundingBox<RealType> tri_bb(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen);

        if (tri_bb.max_x_ < RealType(tile_viewport.min_x_) ||
            tri_bb.min_x_ > RealType(tile_viewport.max_x_) ||
            tri_bb.max_y_ < RealType(tile_viewport.min_y_) ||
            tri_bb.min_y_ > RealType(tile_viewport.max_y_))
            continue;

        // IntType viewport_min_x = max(viewport.min_x_, static_cast<IntType>(floor(tri_bb.min_x_)));
        // IntType viewport_max_x = min(viewport.max_x_, static_cast<IntType>(ceil(tri_bb.max_x_)));
        // IntType viewport_min_y = max(viewport.min_y_, static_cast<IntType>(floor(tri_bb.min_y_)));
        // IntType viewport_max_y = min(viewport.max_y_, static_cast<IntType>(ceil(tri_bb.max_y_)));

        // IntType viewport_min_x = max(viewport.min_x_, static_cast<IntType>(tri_bb.min_x_));
        // IntType viewport_max_x = min(viewport.max_x_, static_cast<IntType>(tri_bb.max_x_ + 1));
        // IntType viewport_min_y = max(viewport.min_y_, static_cast<IntType>(tri_bb.min_y_));
        // IntType viewport_max_y = min(viewport.max_y_, static_cast<IntType>(tri_bb.max_y_ + 1));

        // if (viewport_min_x >= viewport_max_x || viewport_min_y >= viewport_max_y)
        //     continue;

        triangles[num_triangles] = triangle;
        num_triangles++;
        if (num_triangles >= max_num_triangles)
            break;
    }
}

// Triangle rasterizer (top-left rule, perspective correct)
template <typename Derived>
static void draw_triangle(const Triangle<Derived> &triangle,
                          const BoundingBox<IntType> &tile_bb,
                          const typename Derived::Uniforms &uniforms,
                          const typename Derived::InTextures &intextures,
                          typename Derived::Fragment fragment_buffer[],
                          RealType depth_buffer[])
{
    // #pragma HLS inline

    IntType triangle_id = triangle.id;

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
    // Vec2<RealType> p_tl;
    // p_tl(0) = static_cast<RealType>(triangle_bb.min_x_) + RealType(RenderConstants::PIXEL_CENTER_OFFSET);
    // p_tl(1) = static_cast<RealType>(triangle_bb.min_y_) + RealType(RenderConstants::PIXEL_CENTER_OFFSET);

    // RealType eAB_row = edge_func(triangle.vout[0].screen, triangle.vout[1].screen, p_tl);
    // RealType eBC_row = edge_func(triangle.vout[1].screen, triangle.vout[2].screen, p_tl);
    // RealType eCA_row = edge_func(triangle.vout[2].screen, triangle.vout[0].screen, p_tl);

    // Step increments when moving +1 in X or +1 in Y
    // const MathType eAB_dx = (vout[0].screen(1) - vout[1].screen(1));
    // const MathType eAB_dy = (vout[1].screen(0) - vout[0].screen(0));
    // const MathType eBC_dx = (vout[1].screen(1) - vout[2].screen(1));
    // const MathType eBC_dy = (vout[2].screen(0) - vout[1].screen(0));
    // const MathType eCA_dx = (vout[2].screen(1) - vout[0].screen(1));
    // const MathType eCA_dy = (vout[0].screen(0) - vout[2].screen(0));
    // for y down, the - is needed
    // const RealType eAB_dx = (triangle.vout[1].screen(1) - triangle.vout[0].screen(1));
    // const RealType eAB_dy = (triangle.vout[0].screen(0) - triangle.vout[1].screen(0));
    // const RealType eBC_dx = (triangle.vout[2].screen(1) - triangle.vout[1].screen(1));
    // const RealType eBC_dy = (triangle.vout[1].screen(0) - triangle.vout[2].screen(0));
    // const RealType eCA_dx = (triangle.vout[0].screen(1) - triangle.vout[2].screen(1));
    // const RealType eCA_dy = (triangle.vout[2].screen(0) - triangle.vout[0].screen(0));

// Rasterize
draw_triangle_y_loop:
    for (IntType iy = 0; iy < triangle_bb.height_; ++iy)
    {
#pragma HLS loop_tripcount min = 70 max = 70 avg = 70

        IntType texture_y = iy + triangle_bb.min_y_;
        IntType tile_y = texture_y - tile_bb.min_y_;
        IntType tile_address_base = tile_y * tile_bb.width_;

        // const RealType eAB_row_local = RealType(iy) * eAB_dy + eAB_row;
        // const RealType eBC_row_local = RealType(iy) * eBC_dy + eBC_row;
        // const RealType eCA_row_local = RealType(iy) * eCA_dy + eCA_row;

    draw_triangle_x_loop:
        for (IntType ix = 0; ix < triangle_bb.width_; ++ix)
        {
#pragma HLS loop_tripcount min = 70 max = 70 avg = 70
#pragma HLS loop_flatten
            //    #pragma HLS PIPELINE II = 1

#pragma HLS dependence variable = depth_buffer type = inter false

            // #pragma HLS dependence variable = fragment_buffer type = inter false

            IntType texture_x = ix + triangle_bb.min_x_;
            IntType tile_x = texture_x - tile_bb.min_x_;
            IntType tile_address = tile_address_base + tile_x;

            RealType prev_depth = depth_buffer[tile_address];

            // const RealType eAB = RealType(ix) * eAB_dx + eAB_row_local;
            // const RealType eBC = RealType(ix) * eBC_dx + eBC_row_local;
            // const RealType eCA = RealType(ix) * eCA_dx + eCA_row_local;

            Vec2<RealType> p(RealType(texture_x) + RealType(RenderConstants::PIXEL_CENTER_OFFSET),
                             RealType(texture_y) + RealType(RenderConstants::PIXEL_CENTER_OFFSET));

            RealType eAB = edge_func(triangle.vout[0].screen, triangle.vout[1].screen, p);
            RealType eBC = edge_func(triangle.vout[1].screen, triangle.vout[2].screen, p);
            RealType eCA = edge_func(triangle.vout[2].screen, triangle.vout[0].screen, p);

            // Top-left rule adjustments (include pixels on top/left edges)
            const bool inside =
                (eAB > 0 || (eAB == 0 && tlAB)) &&
                (eBC > 0 || (eBC == 0 && tlBC)) &&
                (eCA > 0 || (eCA == 0 && tlCA));

            if (!inside)
                continue;

            // Baricentric weights normalized
            const RealType b0 = eBC * inv_area2;
            const RealType b1 = eCA * inv_area2;
            const RealType b2 = eAB * inv_area2;
            // Baricentric weights normalized (perpective)
            RealType w0 = eBC * triangle.vout[0].invW;
            RealType w1 = eCA * triangle.vout[1].invW;
            RealType w2 = eAB * triangle.vout[2].invW;

            // Perspective: 1/w at pixel
            const RealType inv_invW_px = RealType(1) / (w0 + w1 + w2);

            w0 *= inv_invW_px;
            w1 *= inv_invW_px;
            w2 *= inv_invW_px;

            // I am not sure if I should use perspective corrected interpolation or not
            // Comparing with ground truth, nonperspective seems to give less error
            typename Derived::Varyings varying_px = Derived::interpolate_varyings(triangle_id, w0, w1, w2,
                                                                                  triangle.vout[0].var,
                                                                                  triangle.vout[1].var,
                                                                                  triangle.vout[2].var);

            // Depth (if needed; same trick)
            RealType depth_px = b0 * triangle.vout[0].depth +
                                b1 * triangle.vout[1].depth +
                                b2 * triangle.vout[2].depth;

            // Depth test
            if (depth_px < RealType(0) || (prev_depth >= RealType(0) && prev_depth < depth_px))
                continue;

            Vec4<RealType> gl_FragCoord;
            gl_FragCoord(0) = p(0);
            gl_FragCoord(1) = p(1);
            gl_FragCoord(2) = depth_px;
            gl_FragCoord(3) = inv_invW_px;

            // Fragment fragment;
            Derived::fragment_shader(gl_FragCoord,
                                     uniforms,
                                     varying_px,
                                     intextures,
                                     fragment_buffer[tile_address]);

            // fragment_buffer[tile_address] = fragment;
            depth_buffer[tile_address] = depth_px;
        }
    }
}

template <typename VertexBufferView, typename EboBufferView, typename Derived>
static void draw_tile(const BoundingBox<IntType> &viewport,
                      const BoundingBox<IntType> &tile_viewport,
                      const VertexBufferView &vertex_buffer,
                      const EboBufferView &ebo_buffer,
                      const typename Derived::Uniforms &uniforms,
                      const typename Derived::InTextures &intextures,
                      typename Derived::Fragment fragment_buffer[],
                      RealType depth_buffer[])
{
    IntType total_num_triangles = ebo_buffer.size() / 3;

// Loop over triangles
draw_tile_triangle_loop:
    for (IntType tri_idx = 0; tri_idx < total_num_triangles; tri_idx++)
    {
#pragma HLS loop_tripcount min = MAX_TRI_PER_TILE max = MAX_TRI_PER_TILE avg = MAX_TRI_PER_TILE

        IntType vertexids[3];
        vertexids[0] = ebo_buffer[tri_idx * 3 + 0];
        vertexids[1] = ebo_buffer[tri_idx * 3 + 1];
        vertexids[2] = ebo_buffer[tri_idx * 3 + 2];

        typename Derived::VertexData vertexdata[3];
        vertexdata[0] = Derived::get_vertex_data(vertex_buffer, vertexids[0]);
        vertexdata[1] = Derived::get_vertex_data(vertex_buffer, vertexids[1]);
        vertexdata[2] = Derived::get_vertex_data(vertex_buffer, vertexids[2]);

        Triangle<Derived> triangle = create_triangle<Derived>(vertexdata, vertexids, tri_idx, viewport, uniforms);
        draw_triangle<Derived>(triangle, tile_viewport, uniforms, intextures, fragment_buffer, depth_buffer);
    }
}

template <typename DeferredFragment, typename Derived>
static void draw_tile_deferred(const BoundingBox<IntType> &viewport,
                               const DeferredFragment deferredfragment_buffer[],
                               const typename Derived::Uniforms &uniforms,
                               const typename Derived::InTextures &intextures,
                               typename Derived::Fragment fragment_buffer[])
{
draw_tile_deferred_loop_y:
    for (IntType iy = 0; iy < viewport.height_; iy++)
    {
#pragma HLS loop_tripcount min = MAX_TILE_HEIGHT max = MAX_TILE_HEIGHT avg = MAX_TILE_HEIGHT

    draw_tile_deferred_loop_x:
        for (IntType ix = 0; ix < viewport.width_; ix++)
        {
#pragma HLS loop_tripcount min = MAX_TILE_WIDTH max = MAX_TILE_WIDTH avg = MAX_TILE_WIDTH

            IntType x = ix + viewport.min_x_;
            IntType y = iy + viewport.min_y_;
            IntType address = iy * viewport.width_ + ix;

            DeferredFragment deferredfragment = deferredfragment_buffer[address];

            if (deferredfragment.fpos(2) <= RealType(0) || deferredfragment.kfpos(2) <= RealType(0))
                continue;

            Vec4<RealType> gl_FragCoord;
            gl_FragCoord(0) = RealType(x) + RealType(RenderConstants::PIXEL_CENTER_OFFSET);
            gl_FragCoord(1) = RealType(y) + RealType(RenderConstants::PIXEL_CENTER_OFFSET);
            typename Derived::Varyings varyings = Derived::varyings_from_deferred(deferredfragment);
            Derived::fragment_shader(gl_FragCoord,
                                     uniforms,
                                     varyings,
                                     intextures,
                                     fragment_buffer[address]);
        }
    }
}

template <typename Derived>
static void sync_outtextures(typename Derived::OutTextures &textures,
                             const BoundingBox<IntType> &tex_bb,
                             const typename Derived::Fragment *fragment_buffer)
{
    // #pragma HLS INLINE

depthrendererbase_sync_outtexture_y_loop:
    for (IntType iy = 0; iy < tex_bb.height_; iy++)
    {
#pragma HLS loop_tripcount min = MAX_TILE_HEIGHT max = MAX_TILE_HEIGHT avg = MAX_TILE_HEIGHT

    depthrendererbase_sync_outtexture_x_loop:
        for (IntType ix = 0; ix < tex_bb.width_; ix++)
        {
#pragma HLS loop_tripcount min = MAX_TILE_WIDTH max = MAX_TILE_WIDTH avg = MAX_TILE_WIDTH

            IntType x = ix + tex_bb.min_x_;
            IntType y = iy + tex_bb.min_y_;
            IntType address = iy * tex_bb.width_ + ix;

            Derived::set_outtexture(fragment_buffer[address], textures, x, y);
        }
    }
}

template <class Base>
class RendererBase
{
public:
    using BaseFragment = typename Base::Fragment;
    using BaseUniforms = typename Base::Uniforms;
    using BaseInTextures = typename Base::InTextures;
    using BaseOutTextures = typename Base::OutTextures;

    template <typename VertexBufferView, typename EboBufferView>
    static void RenderTile(const BoundingBox<IntType> &viewport,
                           const BoundingBox<IntType> &tile_viewport,
                           const VertexBufferView &vertex_buffer,
                           const EboBufferView &ebo_buffer,
                           const BaseUniforms &uniforms,
                           const BaseInTextures &intextures,
                           BaseOutTextures &out_textures,
                           BaseFragment fragment_buffer[],
                           RealType depth_buffer[])
    {
        draw_tile<VertexBufferView, EboBufferView, Base>(viewport, tile_viewport, vertex_buffer, ebo_buffer, uniforms, intextures, fragment_buffer, depth_buffer);
        sync_outtextures<Base>(out_textures, tile_viewport, fragment_buffer);
    }

private:
};
