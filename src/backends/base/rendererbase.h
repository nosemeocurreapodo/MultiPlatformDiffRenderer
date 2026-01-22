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
    // #pragma HLS INLINE
    //   return (v0(1) == v1(1)) ? (v1(0) < v0(0)) : (v0(1) < v1(1));
    //   for y up
    //   return (v0(1) < v1(1)) || (v0(1) == v1(1) && v0(0) > v1(0));
    //   for y down
    return (v0(1) > v1(1)) || (v0(1) == v1(1) && v0(0) > v1(0));
}

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
    /*
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
    */

    template <typename VertexBufferView, typename EboBufferView, typename Uniforms>
    static void get_triangles_(const VertexBufferView &vertex_buffer,
                               const EboBufferView &ebo_buffer,
                               const BoundingBox<IntType> &viewport,
                               const BoundingBox<IntType> &tile_viewport,
                               const Uniforms &uniforms,
                               Triangle triangles[],
                               IntType max_num_triangles,
                               IntType &num_triangles)
    {
        // #pragma HLS INLINE off

        num_triangles = 0;

        // Loop over triangles
    renderbase_render_triangles_loop:
        for (IntType i = 0; i + 2 < ebo_buffer.size(); i += 3)
        {

#pragma HLS loop_tripcount min = 768 max = 768 avg = 768
            // #pragma HLS PIPELINE II = 1

            IntType vertexids[3];

            vertexids[0] = ebo_buffer[i + 0];
            vertexids[1] = ebo_buffer[i + 1];
            vertexids[2] = ebo_buffer[i + 2];

            typename Derived::VertexData vertexdata[3];

            vertexdata[0] = Derived::get_vertex_data(vertex_buffer, vertexids[0]);
            vertexdata[1] = Derived::get_vertex_data(vertex_buffer, vertexids[1]);
            vertexdata[2] = Derived::get_vertex_data(vertex_buffer, vertexids[2]);

            Triangle triangle;

            create_triangle_(vertexdata, vertexids, viewport, uniforms, triangle);

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

    static void create_tile_viewports_(BoundingBox<IntType> viewport_tiles[], const BoundingBox<IntType> &viewport, IntType num_tiles_x, IntType num_tiles_y)
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
    static void create_triangle_(const VertexData vertexdata[], const IntType vertexids[], const BoundingBox<IntType> &viewport, const Uniforms &uniforms, Triangle &triangle)
    {
        // #pragma HLS INLINE off

    create_triangle_loop:
        for (int j = 0; j < 3; ++j)
        {
            // #pragma HLS UNROLL

            Vec4<RealType> gl_Position;
            typename Derived::Varyings outvaryings;
            VertexData vertexdata_ = vertexdata[j];
            IntType vertexid_ = vertexids[j];
            Derived::vertex_shader(vertexdata_, vertexid_, uniforms, gl_Position, outvaryings);

            const RealType invW = RealType(1) / gl_Position(3);
            const RealType ndc_x = gl_Position(0) * invW; // [-1,1]
            const RealType ndc_y = gl_Position(1) * invW;
            const RealType ndc_z = gl_Position(2) * invW; // assumed 0..1 after proj (adjust if -1..1)

            // #ifndef USE_VITIS
            // assert(ndc_x >= -1 && ndc_x <= 1 && ndc_y >= -1 && ndc_y <= 1 && ndc_z >= -1 && ndc_z <= 1);
            // #endif

            VSOut vsout;
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
    }

    // Triangle rasterizer (top-left rule, perspective correct)
    template <typename InTextures, typename Uniforms, typename Fragment>
    static void draw_triangle_(const Triangle &triangle, const BoundingBox<IntType> &tile_bb, RealType depth_buffer[], const Uniforms &uniforms, const InTextures &intextures, Fragment fragment_buffer[])
    {
//#pragma HLS inline

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

            // const RealType eAB_row_local = RealType(iy) * eAB_dy + eAB_row;
            // const RealType eBC_row_local = RealType(iy) * eBC_dy + eBC_row;
            // const RealType eCA_row_local = RealType(iy) * eCA_dy + eCA_row;

        draw_triangle_x_loop:
            for (IntType ix = 0; ix < triangle_bb.width_; ++ix)
            {
#pragma HLS loop_tripcount min = 70 max = 70 avg = 70
#pragma HLS loop_flatten
                //    #pragma HLS PIPELINE II = 1

//#pragma HLS dependence variable = depth_buffer type = inter false
                //  #pragma HLS dependence variable = depth_buffer type = intra false

//#pragma HLS dependence variable = fragment_buffer type = inter false

                IntType texture_x = ix + triangle_bb.min_x_;
                IntType tile_x = texture_x - tile_bb.min_x_;
                IntType tile_address = tile_y * tile_bb.width_ + tile_x;

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
                typename Derived::Varyings varying_px = Derived::interpolate_varyings(w0, w1, w2,
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
        Texture<Vec3<OutType>> &out_texture;
    };

    struct VertexData
    {
        Vec3<MathType> vertex;
        Vec3<MathType> normal;
    };

    struct InVaryings
    {
        Vec3<OutType> vColor;
    };

    struct OutVaryings
    {
        Vec3<OutType> vColor;
    };

    struct Fragment
    {
        Vec3<OutType> color;
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
                       Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        // Transform to world space
        Vec3<MathType> fragPos = Vec3<MathType>(uModel_ * Vec4<MathType>(vertexdata.vertex, MathType(1)));
        Vec3<MathType> N = (uNormalMatrix_ * vertexdata.normal).normalized();

        // Lighting vectors
        Vec3<MathType> L = (uLightPos_ - fragPos).normalized();
        Vec3<MathType> V = (uViewPos_ - fragPos).normalized();
        MathType n_dot_l = N.dot(L);
        Vec3<MathType> R = L - MathType(2) * n_dot_l * N; // opengls reflect

        // Phong reflectance model (computed per-vertex)
        MathType NdotL = max(n_dot_l, MathType(0));
        MathType spec = 0.0;
        if (NdotL > 0.0)
        {
            spec = pow(max(V.dot(R), MathType(0)), uShininess_);
        }

        Vec3<MathType> ambient = uAmbientLight_ * uKa_;
        Vec3<MathType> diffuse = uLightColor_ * uKd_ * NdotL;
        Vec3<MathType> specular = uLightColor_ * uKs_ * spec;

        outVarying.vColor = ambient + diffuse + specular;

        gl_Position = uProjection_ * uView_ * Vec4<MathType>(fragPos, MathType(1));
    }

    void fragment_shader(const Vec4<MathType> &gl_FragCoord,
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

    void fragment_shader(const Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         const InTextures &intextures,
                         OutTextures &outtextures)
    {
        // #pragma HLS INLINE

        outtextures.out_texture.set_texel_(in_varying.vColor, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

    Mat4<MathType> uModel_;        // model to world space
    Mat4<MathType> uView_;         // world space to camera space
    Mat4<MathType> uProjection_;   // camera space to clip space
    Mat3<MathType> uNormalMatrix_; // transpose(inverse(mat3(uModel))) computed on CPU

    Vec3<MathType> uLightPos_; // world space
    Vec3<MathType> uViewPos_;  // camera position in world space

    // Material and light
    Vec3<MathType> uKa_;           // ambient reflectance (rgb)
    Vec3<MathType> uKd_;           // diffuse reflectance (rgb)
    Vec3<MathType> uKs_;           // specular reflectance (rgb)
    MathType uShininess_;                  // specular exponent
    Vec3<MathType> uLightColor_;   // light color/intensity (rgb)
    Vec3<MathType> uAmbientLight_; // ambient light (rgb)

    unsigned int out_lvl_;

    Fragment nodata_;
};
*/
// -----------------------------------------------------------------------------
// DepthRenderer
//   Evout[0].screen(0)mple derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

template <template <class> class TextureView>
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
        TextureView<float> out_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
        // Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        Mat4<RealType> pose_matrix;
        Mat4<RealType> view_matrix;
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

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
#pragma HLS INLINE

        VertexData vertexdata;
        IntType base = vertexid * 3;
        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

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
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
#pragma HLS INLINE

        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));

        //   std::cout << "calling vertex shader " << std::endl;
        gl_Position = uniforms.view_matrix * f_ver;
        outVarying.depth = f_ver(2);
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
#pragma HLS INLINE

        RealType depth = in_varying.depth;
        // RealType depth = gl_FragCoord(2);

        fragment.depth = depth;
    }

    static void sync_outtextures(OutTextures &textures, const BoundingBox<IntType> &tex_bb, const Fragment fragment_buffer[], const Uniforms &uniforms)
    {
        // #pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                RealType depth = fragment_buffer[address].depth;
                textures.out_texture(y, x) = depth;
            }
        }
    }
};

// -----------------------------------------------------------------------------
// ImageRendererBase
//   Another evout[0].screen(0)mple derived class that might output color
// -----------------------------------------------------------------------------

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class ImageRendererBase
{
public:
    ImageRendererBase() = delete;
    //~ImageRendererBase() = default;

    struct InTextures
    {
        TextureViewRead<ImageType> in_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<ImageType> out_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        Mat4<RealType> pose_matrix;
        Mat4<RealType> view_matrix;
        PinholeCamera<RealType> camera;
        Vec2<RealType> exposure;
        IntType in_lvl;
        IntType out_lvl;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
    };

    struct Fragment
    {
        RealType color;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
//#pragma HLS inline

        return Fragment{RealType(textures.out_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const unsigned int vertexid)
    {
//#pragma HLS inline

        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
//#pragma HLS inline

        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
//#pragma HLS inline

        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));

        gl_Position = uniforms.view_matrix * f_ver;
        outVarying.kf_ver = vertexdata.vertex;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
//#pragma HLS inline

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(in_varying.kf_ver);

        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
        {
            return;
        }

        RealType pix = sample<RealType, TextureViewRead<ImageType>>(intextures.in_texture,
                                                                    texcoord(1), texcoord(0),
                                                                    AddressMode::Clamp,
                                                                    FilterMode::Nearest);
        pix = apply_exposure(pix, uniforms.exposure);
        fragment.color = pix;
    }

    static void sync_outtextures(OutTextures &textures, const BoundingBox<IntType> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
    {
//#pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                ImageType color = fragment_buffer[address].color;
                textures.out_texture(y, x) = color;
            }
        }
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class DIDxyRendererBase
{
public:
    DIDxyRendererBase() = delete;
    //~DIDxyRendererBase() = default;

    struct InTextures
    {
        TextureViewRead<ImageType> in_texture;
    };
    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> out_texture;
    };

    struct VertexData
    {
        Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        IntType in_lvl;
        IntType out_lvl;
    };

    struct Varyings
    {
        Vec2<RealType> texcoord;
    };

    struct Fragment
    {
        Vec3<RealType> didxy;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        return Fragment{Vec3<RealType>(textures.out_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 2;

        vertexdata.texcoord(0) = vertex_buffer[base + 0];
        vertexdata.texcoord(1) = vertex_buffer[base + 1];

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
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        // gl_Position = (view_matrix * pose_matrix) * Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = Vec4<RealType>(RealType(2) * vertexdata.texcoord(0) - RealType(1), RealType(2) * vertexdata.texcoord(1) - RealType(1), RealType(0), RealType(1));
        outVarying.texcoord = vertexdata.texcoord;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        if (in_varying.texcoord(0) < RealType(0) || in_varying.texcoord(0) > RealType(1) ||
            in_varying.texcoord(1) < RealType(0) || in_varying.texcoord(1) > RealType(1))
            return;

        IntType height = intextures.in_texture.height();
        IntType width = intextures.in_texture.width();
        ImageType nodata = intextures.in_texture.nodata();

        // if(in_varying.texcoord(0) > 0.5)
        //     return;

        // IntType x = IntType(in_varying.texcoord(0) * RealType(width - 1));
        // IntType y = IntType(in_varying.texcoord(1) * RealType(height - 1));
        IntType x = IntType(gl_FragCoord(0));
        IntType y = IntType(gl_FragCoord(1));
        IntType x_p = x + 1;
        IntType x_pp = x + 2;
        IntType x_m = x - 1;
        IntType x_mm = x - 2;
        IntType y_p = y + 1;
        IntType y_pp = y + 2;
        IntType y_m = y - 1;
        IntType y_mm = y - 2;

        if (x_p >= width || x_m < 0 || y_p >= height || y_m < 0) // ||
                                                                 // x_pp >= width || x_mm < 0 || y_pp >= height || y_mm < 0)
        {
            //  No need to explicitly set to nodata, it is already in the background color
            return;
        }

        ImageType f = intextures.in_texture(y, x);
        ImageType f_y_p = intextures.in_texture(y_p, x);
        ImageType f_y_m = intextures.in_texture(y_m, x);
        ImageType f_x_p = intextures.in_texture(y, x_p);
        ImageType f_x_m = intextures.in_texture(y, x_m);
        // ImageType f_y_pp = intextures.in_texture.texel_(y_pp, x, uniforms.in_lvl);
        // ImageType f_y_mm = intextures.in_texture.texel_(y_mm, x, uniforms.in_lvl);
        // mageType f_x_pp = intextures.in_texture.texel_(y, x_pp, uniforms.in_lvl);
        // ImageType f_x_mm = intextures.in_texture.texel_(y, x_mm, uniforms.in_lvl);

        // if (f_x_p == nodata || f_x_m == nodata ||
        //     f_y_p == nodata || f_y_m == nodata || f == nodata)
        //{
        //   No need to explicitly set to nodata, it is already in the background color
        //    return;
        //}

        Vec3<RealType> out_fragment;
        // out_fragment(0) = (-RealType(f_x_pp) + RealType(8) * RealType(f_x_p) - RealType(8) * RealType(f_x_m) + RealType(f_x_mm)) / RealType(12);
        // out_fragment(1) = (-RealType(f_y_pp) + RealType(8) * RealType(f_y_p) - RealType(8) * RealType(f_y_m) + RealType(f_y_mm)) / RealType(12);
        // out_fragment(2) = RealType(f);
        out_fragment(0) = (RealType(f_x_p) - RealType(f_x_m)) / RealType(2);
        out_fragment(1) = (RealType(f_y_p) - RealType(f_y_m)) / RealType(2);
        out_fragment(2) = RealType(0); // f; // save the projected frame for later processing

        // if(out_fragment.norm() < RealType(50))
        //     return;

        fragment.didxy = out_fragment;
    }

    /*
    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType height = intextures.in_texture.height(uniforms.in_lvl);
        IntType width = intextures.in_texture.width(uniforms.in_lvl);
        ImageType nodata = intextures.in_texture.nodata();

        //IntType x = IntType(in_varying.texcoord(0) * RealType(width - 1));
        //IntType y = IntType(in_varying.texcoord(1) * RealType(height - 1));
        IntType x = IntType(gl_FragCoord(0));
        IntType y = IntType(gl_FragCoord(1));

        if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1)
            return;

        RealType sobel_x[3][3];
        RealType sobel_y[3][3];

        sobel_x[0][0] = RealType(-1);
        sobel_x[0][1] = RealType(0);
        sobel_x[0][2] = RealType(1);
        sobel_x[1][0] = RealType(-2);
        sobel_x[1][1] = RealType(0);
        sobel_x[1][2] = RealType(2);
        sobel_x[2][0] = RealType(-1);
        sobel_x[2][1] = RealType(0);
        sobel_x[2][2] = RealType(1);

        sobel_y[0][0] = RealType(-1);
        sobel_y[1][0] = RealType(0);
        sobel_y[2][0] = RealType(1);
        sobel_y[0][1] = RealType(-2);
        sobel_y[1][1] = RealType(0);
        sobel_y[2][1] = RealType(2);
        sobel_y[0][2] = RealType(-1);
        sobel_y[1][2] = RealType(0);
        sobel_y[2][2] = RealType(1);

        Vec3<RealType> out_fragment(0, 0, 0);
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++)
            {
                ImageType val = intextures.in_texture.texel_(y + j - 1, x + i - 1, uniforms.in_lvl);
                out_fragment(0) += sobel_x[j][i] * RealType(val);
                out_fragment(1) += sobel_y[j][i] * RealType(val);
            }

        fragment.didxy = out_fragment;
    }
    */
    /*
    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        // if (in_varying.texcoord(0) > 0.5)
        //   return;

        IntType height = intextures.in_texture.height(uniforms.in_lvl);
        IntType width = intextures.in_texture.width(uniforms.in_lvl);
        ImageType nodata = intextures.in_texture.nodata();

        //IntType x = IntType(in_varying.texcoord(0) * RealType(width - 1));
        //IntType y = IntType(in_varying.texcoord(1) * RealType(height - 1));
        IntType x = IntType(gl_FragCoord(0));
        IntType y = IntType(gl_FragCoord(1));

        if (x <= 1 || x >= width - 2 || y <= 1 || y >= height - 2)
            return;

        RealType scharr5x5[5][5];

        scharr5x5[0][0] = RealType(-1);
        scharr5x5[0][1] = RealType(-1);
        scharr5x5[0][2] = RealType(0);
        scharr5x5[0][3] = RealType(1);
        scharr5x5[0][4] = RealType(1);

        scharr5x5[1][0] = RealType(-2);
        scharr5x5[1][1] = RealType(-2);
        scharr5x5[1][2] = RealType(0);
        scharr5x5[1][3] = RealType(2);
        scharr5x5[1][4] = RealType(2);

        scharr5x5[2][0] = RealType(-3);
        scharr5x5[2][1] = RealType(-6);
        scharr5x5[2][2] = RealType(0);
        scharr5x5[2][3] = RealType(6);
        scharr5x5[2][4] = RealType(3);

        scharr5x5[3][0] = RealType(-2);
        scharr5x5[3][1] = RealType(-2);
        scharr5x5[3][2] = RealType(0);
        scharr5x5[3][3] = RealType(2);
        scharr5x5[3][4] = RealType(2);

        scharr5x5[4][0] = RealType(-1);
        scharr5x5[4][1] = RealType(-1);
        scharr5x5[4][2] = RealType(0);
        scharr5x5[4][3] = RealType(1);
        scharr5x5[4][4] = RealType(1);

        Vec3<RealType> out_fragment(0, 0, 0);
        for (int j = 0; j < 5; j++)
            for (int i = 0; i < 5; i++)
            {
                ImageType val = intextures.in_texture.texel_(y + j - 2, x + i - 2, uniforms.in_lvl);
                out_fragment(0) += scharr5x5[j][i] * RealType(val);
                out_fragment(1) += scharr5x5[i][j] * RealType(val);
            }

        fragment.didxy = out_fragment;
    }
    */

    static void sync_outtextures(OutTextures &textures, const BoundingBox<IntType> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
    {
        // #pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                Vec3<RealType> didxy = fragment_buffer[address].didxy;

                textures.out_texture(y, x) = didxy;
            }
        }
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class DIDexpRendererBase
{
public:
    DIDexpRendererBase() = delete;
    //~DIDxyRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> in_texture;
    };
    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> out_texture;
    };

    struct VertexData
    {
        Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        IntType in_lvl;
        IntType out_lvl;
        Vec2<RealType> exposure;
    };

    struct Varyings
    {
        Vec2<RealType> texcoord;
    };

    struct Fragment
    {
        Vec3<RealType> didexp;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        return Fragment{Vec3<RealType>(textures.out_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 2;

        vertexdata.texcoord(0) = vertex_buffer[base + 0];
        vertexdata.texcoord(1) = vertex_buffer[base + 1];

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
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        // gl_Position = (view_matrix * pose_matrix) * Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = Vec4<RealType>(RealType(2) * vertexdata.texcoord(0) - RealType(1), RealType(2) * vertexdata.texcoord(1) - RealType(1), RealType(0), RealType(1));
        outVarying.texcoord = vertexdata.texcoord;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        if (in_varying.texcoord(0) < RealType(0) || in_varying.texcoord(0) > RealType(1) ||
            in_varying.texcoord(1) < RealType(0) || in_varying.texcoord(1) > RealType(1))
            return;

        IntType height = intextures.in_texture.height();
        IntType width = intextures.in_texture.width();
        ImageType nodata = intextures.in_texture.nodata();

        IntType x = IntType(in_varying.texcoord(0) * RealType(width - 1));
        IntType y = IntType(in_varying.texcoord(1) * RealType(height - 1));

        ImageType f = intextures.in_texture(y, x);

        if (f == intextures.in_texture.nodata())
            return;

        Vec3<RealType> d = d_f_exp_d_exp(RealType(f), uniforms.exposure);

        Vec3<RealType> out_fragment;
        out_fragment(0) = d(0);        // b = f*exp(param(0)) + param(1)
        out_fragment(1) = d(1);        // dbdexp = f*exp(param(0)), 1.0
        out_fragment(2) = RealType(0); // f; // save the projected frame for later processing

        fragment.didexp = out_fragment;
    }

    static void sync_outtextures(OutTextures &textures, const BoundingBox<IntType> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
    {
        // #pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                Vec3<RealType> didexp = fragment_buffer[address].didexp;

                textures.out_texture(y, x) = didexp;
            }
        }
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JPoseExpRendererBase
{
public:
    JPoseExpRendererBase() = delete;
    //~JPoseRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> kf_texture;
        const TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jtra_texture;
        TextureViewWrite<Vec3<float>> jrot_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        Vec2<RealType> exposure;
        PinholeCamera<RealType> camera;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        Vec3<RealType> f_ver;
        Vec3<RealType> kf_ver;
    };

    struct Fragment
    {
        Vec3<RealType> jtra;
        Vec3<RealType> jrot;
        Vec3<RealType> jexp;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        return Fragment{Vec3<RealType>(textures.jtra_texture.nodata()),
                        Vec3<RealType>(textures.jrot_texture.nodata()),
                        Vec3<RealType>(textures.jexp_texture.nodata()),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ver = vertexdata.vertex;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);

        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture,
                                                                   texcoord(1), texcoord(0));
        // Vec3<RealType> d_f_d_xy = intextures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //     return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_d_f = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_d_f * d_f_d_xy;

        RealType v0 = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        RealType v1 = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        RealType v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        Vec3<RealType> d_f_i_d_tra = Vec3<RealType>(v0, v1, v2);
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        fragment.jtra = d_f_i_d_tra;
        fragment.jrot = d_f_i_d_rot;
        fragment.jexp = d_fexp_d_exp;
        fragment.image = f_exp;
    }

    static void sync_outtextures(OutTextures &textures, const BoundingBox<IntType> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
    {
        // #pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                Vec3<RealType> jtra = fragment_buffer[address].jtra;
                Vec3<RealType> jrot = fragment_buffer[address].jrot;
                Vec3<RealType> jexp = fragment_buffer[address].jexp;
                RealType image = fragment_buffer[address].image;

                textures.jtra_texture(y, x) = jtra;
                textures.jrot_texture(y, x) = jrot;
                textures.jexp_texture(y, x) = jexp;
                textures.image_texture(y, x) = image;
            }
        }
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JPoseVelExpRendererBase
{
public:
    JPoseVelExpRendererBase() = delete;
    //~JPoseRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> kf_texture;
        const TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jtra_texture;
        TextureViewWrite<Vec3<float>> jrot_texture;
        TextureViewWrite<Vec3<float>> jtravel_texture;
        TextureViewWrite<Vec3<float>> jrotvel_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        Vec6<RealType> vel_matrix;
        Vec2<RealType> exposure;
        PinholeCamera<RealType> camera;
        IntType out_width;
        IntType out_height;
        RealType readout_time;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        RealType dt;
    };

    struct Fragment
    {
        Vec3<RealType> jtra;
        Vec3<RealType> jrot;
        Vec3<RealType> jtravel;
        Vec3<RealType> jrotvel;
        Vec3<RealType> jexp;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        return Fragment{Vec3<RealType>(textures.jtra_texture.nodata()),
                        Vec3<RealType>(textures.jrot_texture.nodata()),
                        Vec3<RealType>(textures.jtravel_texture.nodata()),
                        Vec3<RealType>(textures.jrotvel_texture.nodata()),
                        Vec3<RealType>(textures.jexp_texture.nodata()),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.dt =
            (w0 * varying_px0.dt +
             w1 * varying_px1.dt +
             w2 * varying_px2.dt);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        RealType dt; // = (vertexdata.texcoord(1) - RealType(0.5)) * uniforms.readout_time;
        Mat4<RealType> T_rs = (SE3<float>::exp(uniforms.vel_matrix * dt)).matrix() * uniforms.pose_matrix;
        Vec4<RealType> f_ver = T_rs * Vec4<RealType>(vertexdata.vertex(0),
                                                     vertexdata.vertex(1),
                                                     vertexdata.vertex(2),
                                                     RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ver = vertexdata.vertex;
        outVarying.dt = dt;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);

        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture,
                                                                   texcoord(1), texcoord(0));
        // Vec3<RealType> d_f_d_xy = intextures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //     return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_d_f = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_d_f * d_f_d_xy;

        RealType v0 = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        RealType v1 = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        RealType v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        Vec3<RealType> d_f_i_d_tra = Vec3<RealType>(v0, v1, v2);
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        Vec3<RealType> d_f_i_d_travel = in_varying.dt * d_f_i_d_tra;
        Vec3<RealType> d_f_i_d_rotvel = in_varying.dt * d_f_i_d_rot;

        fragment.jtra = d_f_i_d_tra;
        fragment.jrot = d_f_i_d_rot;
        fragment.jtravel = d_f_i_d_travel;
        fragment.jrotvel = d_f_i_d_rotvel;
        fragment.jexp = d_fexp_d_exp;
        fragment.image = f_exp;
    }

    static void sync_outtextures(OutTextures &textures, const BoundingBox<IntType> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
    {
        // #pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                Vec3<RealType> jtra = fragment_buffer[address].jtra;
                Vec3<RealType> jrot = fragment_buffer[address].jrot;
                Vec3<RealType> jtravel = fragment_buffer[address].jtravel;
                Vec3<RealType> jrotvel = fragment_buffer[address].jrotvel;
                Vec3<RealType> jexp = fragment_buffer[address].jexp;
                RealType image = fragment_buffer[address].image;

                textures.jtra_texture(y, x) = jtra;
                textures.jrot_texture(y, x) = jrot;
                textures.jtravel_texture(y, x) = jtravel;
                textures.jrotvel_texture(y, x) = jrotvel;
                textures.jexp_texture(y, x) = jexp;
                textures.image_texture(y, x) = image;
            }
        }
    }
};

template <template <class> class TextureView>
class PidsRendererBase
{
public:
    PidsRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        const int notused;
    };

    struct OutTextures
    {
        TextureView<Vec3<PidType>> pids_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        IntType vertexId;
        Vec3<IntType> pids;
    };

    struct Fragment
    {
        Vec3<IntType> pids;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2))};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        outVarying.vertexId = vertexid;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        Vec3<IntType> vertexid = in_varying.pids;
        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));
        fragment.pids = ids;
    }

    static void sync_outtextures(OutTextures &textures, const BoundingBox<IntType> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
    {
        // #pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                Vec3<IntType> pids = fragment_buffer[address].pids;
                Vec3<PidType> pids_out(pids(0), pids(1), pids(2));
                textures.pids_texture(y, x) = pids_out;
            }
        }
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JMapExpRendererBase
{
public:
    JMapExpRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> kf_texture;
        const TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jmap_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<Vec3<PidType>> pids_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        PinholeCamera<RealType> camera;
        IntType out_width;
        IntType out_height;
        Vec2<RealType> exposure;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        Vec3<RealType> kf_ray;
        Vec3<RealType> kf_ray_0;
        Vec3<RealType> kf_ray_1;
        Vec3<RealType> kf_ray_2;
        RealType depth;
        Vec3<RealType> baricentric;
        IntType vertexId;
        Vec3<IntType> pids;
    };

    struct Fragment
    {
        Vec3<RealType> jmap;
        Vec3<RealType> jexp;
        Vec3<IntType> pids;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<float> jmap_nodata(textures.jmap_texture.nodata());
        Vec3<float> jexp_nodata(textures.jexp_texture.nodata());
        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<RealType>(jmap_nodata(0), jmap_nodata(1), jmap_nodata(2)),
                        Vec3<RealType>(jexp_nodata(0), jexp_nodata(1), jexp_nodata(2)),
                        Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2)),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray_0 = varying_px0.kf_ray;
        var_over_w_px.kf_ray_1 = varying_px1.kf_ray;
        var_over_w_px.kf_ray_2 = varying_px2.kf_ray;
        // var_over_w_px.barvout[2].screen(1)entric = Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = Vec3<RealType>(w0,
                                                   w1,
                                                   w2);
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        Vec3<RealType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), RealType(1));
        Vec4<RealType> d_f_ver_d_kf_depth_ = uniforms.pose_matrix * Vec4<RealType>(kf_ray(0),
                                                                                   kf_ray(1),
                                                                                   kf_ray(2),
                                                                                   RealType(0));
        Vec3<RealType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.kf_ver = vertexdata.vertex;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> kf_ray_0 = in_varying.kf_ray_0;
        Vec3<RealType> kf_ray_1 = in_varying.kf_ray_1;
        Vec3<RealType> kf_ray_2 = in_varying.kf_ray_2;
        Vec3<RealType> baricentric = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);

        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture, texcoord(1), texcoord(0));
        // Vec3<RealType> d_f_d_xy = intextures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //     return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_df = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_df * d_f_d_xy;

        Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // Vec3<MathType>d_f_i_d_tra = Vec3<MathType>(v0, v1, v2);
        // Vec3<MathType>d_f_i_d_rot = Vec3<MathType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        RealType d_f_i_d_kf_depth_0 = (d_f_i_d_f_ver.transpose() * kf_ray_0)(0, 0);
        RealType d_f_i_d_kf_depth_1 = (d_f_i_d_f_ver.transpose() * kf_ray_1)(0, 0);
        RealType d_f_i_d_kf_depth_2 = (d_f_i_d_f_ver.transpose() * kf_ray_2)(0, 0);

        Vec3<RealType> d_depth_d_vert_depth = baricentric;

        Vec3<RealType> jac;
        jac(0) = d_f_i_d_kf_depth_0 * d_depth_d_vert_depth(0);
        jac(1) = d_f_i_d_kf_depth_1 * d_depth_d_vert_depth(1);
        jac(2) = d_f_i_d_kf_depth_2 * d_depth_d_vert_depth(2);

        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.jmap = jac;
        fragment.jexp = d_fexp_d_exp;
        fragment.pids = ids;
        fragment.image = f_exp;
    }

    static void sync_outtextures(OutTextures &textures, const BoundingBox<IntType> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
    {
        // #pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                Vec3<RealType> jmap = fragment_buffer[address].jmap;
                Vec3<RealType> jexp = fragment_buffer[address].jexp;
                Vec3<IntType> pids = fragment_buffer[address].pids;
                RealType image = fragment_buffer[address].image;

                Vec3<float> jmap_out(jmap(0), jmap(1), jmap(2));
                Vec3<float> jexp_out(jexp(0), jexp(1), jexp(2));
                Vec3<PidType> pids_out(pids(0), pids(1), pids(2));

                textures.jmap_texture(y, x) = jmap_out;
                textures.jexp_texture(y, x) = jexp_out;
                textures.pids_texture(y, x) = pids_out;
                textures.image_texture(y, x) = image;
            }
        }
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JPoseExpMapRendererBase
{
public:
    JPoseExpMapRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        TextureViewRead<ImageType> kf_texture;
        TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jtra_texture;
        TextureViewWrite<Vec3<float>> jrot_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<Vec3<float>> jmap_texture;
        TextureViewWrite<Vec3<PidType>> pids_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        Vec2<RealType> exposure;
        PinholeCamera<RealType> camera;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        Vec3<RealType> kf_ray;
        Vec3<RealType> kf_ray_0;
        Vec3<RealType> kf_ray_1;
        Vec3<RealType> kf_ray_2;
        RealType depth;
        Vec3<RealType> baricentric;
        IntType vertexId;
        Vec3<IntType> pids;
    };

    struct Fragment
    {
        Vec3<RealType> jtra;
        Vec3<RealType> jrot;
        Vec3<RealType> jexp;
        Vec3<RealType> jmap;
        Vec3<IntType> pids;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<float> jtra_nodata(textures.jtra_texture.nodata());
        Vec3<float> jrot_nodata(textures.jrot_texture.nodata());
        Vec3<float> jexp_nodata(textures.jexp_texture.nodata());
        Vec3<float> jmap_nodata(textures.jmap_texture.nodata());
        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<RealType>(jtra_nodata(0), jtra_nodata(1), jtra_nodata(2)),
                        Vec3<RealType>(jrot_nodata(0), jrot_nodata(1), jrot_nodata(2)),
                        Vec3<RealType>(jexp_nodata(0), jexp_nodata(1), jexp_nodata(2)),
                        Vec3<RealType>(jmap_nodata(0), jmap_nodata(1), jmap_nodata(2)),
                        Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2)),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
#pragma HLS INLINE

        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
#pragma HLS INLINE

        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray_0 = varying_px0.kf_ray;
        var_over_w_px.kf_ray_1 = varying_px1.kf_ray;
        var_over_w_px.kf_ray_2 = varying_px2.kf_ray;
        // var_over_w_px.barvout[2].screen(1)entric = Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = Vec3<RealType>(w0,
                                                   w1,
                                                   w2);
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
#pragma HLS INLINE

        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));

        gl_Position = uniforms.view_matrix * f_ver;

        Vec3<RealType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), RealType(1));
        Vec4<RealType> d_f_ver_d_kf_depth_ = uniforms.pose_matrix * Vec4<RealType>(kf_ray(0),
                                                                                   kf_ray(1),
                                                                                   kf_ray(2),
                                                                                   RealType(0));
        Vec3<RealType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.kf_ver = vertexdata.vertex;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
#pragma HLS INLINE

        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> kf_ray_0 = in_varying.kf_ray_0;
        Vec3<RealType> kf_ray_1 = in_varying.kf_ray_1;
        Vec3<RealType> kf_ray_2 = in_varying.kf_ray_2;
        Vec3<RealType> baricentric = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);
        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture,
                                                                   texcoord(1), texcoord(0));
        // Vec3<RealType> d_f_d_xy = intextures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //    return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_df = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_df * d_f_d_xy;

        Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        Vec3<RealType> d_f_i_d_tra = d_f_i_d_f_ver;
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2),
                                                    f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2),
                                                    -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        RealType d_f_i_d_kf_depth_0 = (d_f_i_d_f_ver.transpose() * kf_ray_0)(0, 0);
        RealType d_f_i_d_kf_depth_1 = (d_f_i_d_f_ver.transpose() * kf_ray_1)(0, 0);
        RealType d_f_i_d_kf_depth_2 = (d_f_i_d_f_ver.transpose() * kf_ray_2)(0, 0);

        Vec3<RealType> d_depth_d_vert_depth = baricentric;

        Vec3<RealType> jac;
        jac(0) = d_f_i_d_kf_depth_0 * d_depth_d_vert_depth(0);
        jac(1) = d_f_i_d_kf_depth_1 * d_depth_d_vert_depth(1);
        jac(2) = d_f_i_d_kf_depth_2 * d_depth_d_vert_depth(2);

        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.jtra = d_f_i_d_tra;
        fragment.jrot = d_f_i_d_rot;
        fragment.jexp = d_fexp_d_exp;
        fragment.jmap = jac;
        fragment.pids = ids;
        fragment.image = f_exp;
    }

    static void sync_outtextures(OutTextures &textures, const BoundingBox<IntType> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
    {
#pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                Vec3<RealType> jtra = fragment_buffer[address].jtra;
                Vec3<RealType> jrot = fragment_buffer[address].jrot;
                Vec3<RealType> jexp = fragment_buffer[address].jexp;
                Vec3<RealType> jmap = fragment_buffer[address].jmap;
                Vec3<IntType> pids = fragment_buffer[address].pids;
                RealType image = fragment_buffer[address].image;

                Vec3<float> jtra_out(jtra(0), jtra(1), jtra(2));
                Vec3<float> jrot_out(jrot(0), jrot(1), jrot(2));
                Vec3<float> jexp_out(jexp(0), jexp(1), jexp(2));
                Vec3<float> jmap_out(jmap(0), jmap(1), jmap(2));
                Vec3<PidType> pids_out(pids(0), pids(1), pids(2));

                textures.jtra_texture(y, x) = jtra_out;
                textures.jrot_texture(y, x) = jrot_out;
                textures.jexp_texture(y, x) = jexp_out;
                textures.jmap_texture(y, x) = jmap_out;
                textures.pids_texture(y, x) = pids_out;
                textures.image_texture(y, x) = image;
            }
        }
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JPoseVelExpMapRendererBase
{
public:
    JPoseVelExpMapRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> kf_texture;
        const TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jtra_texture;
        TextureViewWrite<Vec3<float>> jrot_texture;
        TextureViewWrite<Vec3<float>> jtravel_texture;
        TextureViewWrite<Vec3<float>> jrotvel_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<Vec3<float>> jmap_texture;
        TextureViewWrite<Vec3<PidType>> pids_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        Vec6<RealType> vel_matrix;
        Vec2<RealType> exposure;
        PinholeCamera<RealType> camera;
        RealType readout_time;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        Vec3<RealType> kf_ray;
        Vec3<RealType> kf_ray_0;
        Vec3<RealType> kf_ray_1;
        Vec3<RealType> kf_ray_2;
        RealType depth;
        Vec3<RealType> baricentric;
        IntType vertexId;
        Vec3<IntType> pids;
        RealType dt;
    };

    struct Fragment
    {
        Vec3<RealType> jtra;
        Vec3<RealType> jrot;
        Vec3<RealType> jtravel;
        Vec3<RealType> jrotvel;
        Vec3<RealType> jexp;
        Vec3<RealType> jmap;
        Vec3<IntType> pids;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<float> jtra_nodata(textures.jtra_texture.nodata());
        Vec3<float> jrot_nodata(textures.jrot_texture.nodata());
        Vec3<float> jtravel_nodata(textures.jtravel_texture.nodata());
        Vec3<float> jrotvel_nodata(textures.jrotvel_texture.nodata());
        Vec3<float> jexp_nodata(textures.jexp_texture.nodata());
        Vec3<float> jmap_nodata(textures.jmap_texture.nodata());
        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<RealType>(jtra_nodata(0), jtra_nodata(1), jtra_nodata(2)),
                        Vec3<RealType>(jrot_nodata(0), jrot_nodata(1), jrot_nodata(2)),
                        Vec3<RealType>(jtravel_nodata(0), jtravel_nodata(1), jtravel_nodata(2)),
                        Vec3<RealType>(jrotvel_nodata(0), jrotvel_nodata(1), jrotvel_nodata(2)),
                        Vec3<RealType>(jexp_nodata(0), jexp_nodata(1), jexp_nodata(2)),
                        Vec3<RealType>(jmap_nodata(0), jmap_nodata(1), jmap_nodata(2)),
                        Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2)),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &verte_buffer, const IntType vertexid)
    {
#pragma HLS inline

        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = verte_buffer[base + 0];
        vertexdata.vertex(1) = verte_buffer[base + 1];
        vertexdata.vertex(2) = verte_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray_0 = varying_px0.kf_ray;
        var_over_w_px.kf_ray_1 = varying_px1.kf_ray;
        var_over_w_px.kf_ray_2 = varying_px2.kf_ray;
        // var_over_w_px.barvout[2].screen(1)entric = Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = Vec3<RealType>(w0,
                                                   w1,
                                                   w2);
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);
        var_over_w_px.dt =
            (w0 * varying_px0.dt +
             w1 * varying_px1.dt +
             w2 * varying_px2.dt);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        RealType dt; // = (vertexdata.texcoord(1) - RealType(0.5)) * uniforms.readout_time;
        Mat4<RealType> T_rs = (SE3<float>::exp(uniforms.vel_matrix * dt)).matrix() * uniforms.pose_matrix;
        Vec4<RealType> f_ver = T_rs * Vec4<RealType>(vertexdata.vertex(0),
                                                     vertexdata.vertex(1),
                                                     vertexdata.vertex(2),
                                                     RealType(1));

        gl_Position = uniforms.view_matrix * f_ver;

        Vec3<RealType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), RealType(1));
        Vec4<RealType> d_f_ver_d_kf_depth_ = T_rs * Vec4<RealType>(kf_ray(0),
                                                                   kf_ray(1),
                                                                   kf_ray(2),
                                                                   RealType(0));
        Vec3<RealType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.kf_ver = vertexdata.vertex;
        outVarying.dt = dt;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> kf_ray_0 = in_varying.kf_ray_0;
        Vec3<RealType> kf_ray_1 = in_varying.kf_ray_1;
        Vec3<RealType> kf_ray_2 = in_varying.kf_ray_2;
        Vec3<RealType> baricentric = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);
        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture,
                                                                   texcoord(1), texcoord(0));

        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //     return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_df = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_df * d_f_d_xy;

        Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        Vec3<RealType> d_f_i_d_tra = d_f_i_d_f_ver;
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2),
                                                    f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2),
                                                    -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        Vec3<RealType> d_f_i_d_travel = in_varying.dt * d_f_i_d_tra;
        Vec3<RealType> d_f_i_d_rotvel = in_varying.dt * d_f_i_d_rot;

        RealType d_f_i_d_kf_depth_0 = (d_f_i_d_f_ver.transpose() * kf_ray_0)(0, 0);
        RealType d_f_i_d_kf_depth_1 = (d_f_i_d_f_ver.transpose() * kf_ray_1)(0, 0);
        RealType d_f_i_d_kf_depth_2 = (d_f_i_d_f_ver.transpose() * kf_ray_2)(0, 0);

        Vec3<RealType> d_depth_d_vert_depth = baricentric;

        Vec3<RealType> jac;
        jac(0) = d_f_i_d_kf_depth_0 * d_depth_d_vert_depth(0);
        jac(1) = d_f_i_d_kf_depth_1 * d_depth_d_vert_depth(1);
        jac(2) = d_f_i_d_kf_depth_2 * d_depth_d_vert_depth(2);

        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.jtra = d_f_i_d_tra;
        fragment.jrot = d_f_i_d_rot;
        fragment.jtravel = d_f_i_d_travel;
        fragment.jrotvel = d_f_i_d_rotvel;
        fragment.jexp = d_fexp_d_exp;
        fragment.jmap = jac;
        fragment.pids = ids;
        fragment.image = f_exp;
    }

    static void sync_outtextures(OutTextures &textures, const BoundingBox<IntType> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
    {
        // #pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                Vec3<RealType> jtra = fragment_buffer[address].jtra;
                Vec3<RealType> jrot = fragment_buffer[address].jrot;
                Vec3<RealType> jtravel = fragment_buffer[address].jtravel;
                Vec3<RealType> jrotvel = fragment_buffer[address].jrotvel;
                Vec3<RealType> jexp = fragment_buffer[address].jexp;
                Vec3<RealType> jmap = fragment_buffer[address].jmap;
                Vec3<IntType> pids = fragment_buffer[address].pids;
                RealType image = fragment_buffer[address].image;

                Vec3<float> jtra_out(jtra(0), jtra(1), jtra(2));
                Vec3<float> jrot_out(jrot(0), jrot(1), jrot(2));
                Vec3<float> jtravel_out(jtravel(0), jtravel(1), jtravel(2));
                Vec3<float> jrotvel_out(jrotvel(0), jrotvel(1), jrotvel(2));
                Vec3<float> jexp_out(jexp(0), jexp(1), jexp(2));
                Vec3<float> jmap_out(jmap(0), jmap(1), jmap(2));
                Vec3<PidType> pids_out(pids(0), pids(1), pids(2));

                textures.jtra_texture(y, x) = jtra_out;
                textures.jrot_texture(y, x) = jrot_out;
                textures.jtravel_texture(y, x) = jtravel_out;
                textures.jrotvel_texture(y, x) = jrotvel_out;
                textures.jexp_texture(y, x) = jexp_out;
                textures.jmap_texture(y, x) = jmap_out;
                textures.pids_texture(y, x) = pids_out;
                textures.image_texture(y, x) = image;
            }
        }
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class DiffRendererBase
{
public:
    DiffRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        TextureViewRead<ImageType> diffuse_texture;
        TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jtra_texture;
        TextureViewWrite<Vec3<float>> jrot_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<Vec3<float>> jmap_texture;
        TextureViewWrite<Vec3<PidType>> pids_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
        Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        Vec2<RealType> exposure;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        Vec2<RealType> texcoord;
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        Vec3<RealType> kf_ray;
        Vec3<RealType> kf_ray_0;
        Vec3<RealType> kf_ray_1;
        Vec3<RealType> kf_ray_2;
        RealType depth;
        Vec3<RealType> baricentric;
        IntType vertexId;
        Vec3<IntType> pids;
    };

    struct Fragment
    {
        Vec3<RealType> jtra;
        Vec3<RealType> jrot;
        Vec3<RealType> jexp;
        Vec3<RealType> jmap;
        Vec3<IntType> pids;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<float> jtra_nodata(textures.jtra_texture.nodata());
        Vec3<float> jrot_nodata(textures.jrot_texture.nodata());
        Vec3<float> jexp_nodata(textures.jexp_texture.nodata());
        Vec3<float> jmap_nodata(textures.jmap_texture.nodata());
        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<RealType>(jtra_nodata(0), jtra_nodata(1), jtra_nodata(2)),
                        Vec3<RealType>(jrot_nodata(0), jrot_nodata(1), jrot_nodata(2)),
                        Vec3<RealType>(jexp_nodata(0), jexp_nodata(1), jexp_nodata(2)),
                        Vec3<RealType>(jmap_nodata(0), jmap_nodata(1), jmap_nodata(2)),
                        Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2)),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
#pragma HLS INLINE

        VertexData vertexdata;

        IntType base = vertexid * 5;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        vertexdata.texcoord(0) = vertex_buffer[base + 3];
        vertexdata.texcoord(1) = vertex_buffer[base + 4];

        return vertexdata;
    }

    static Varyings interpolate_varyings(const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
#pragma HLS INLINE

        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray_0 = varying_px0.kf_ray;
        var_over_w_px.kf_ray_1 = varying_px1.kf_ray;
        var_over_w_px.kf_ray_2 = varying_px2.kf_ray;
        // var_over_w_px.barvout[2].screen(1)entric = Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = Vec3<RealType>(w0,
                                                   w1,
                                                   w2);
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
#pragma HLS INLINE

        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));

        gl_Position = uniforms.view_matrix * f_ver;

        Vec3<RealType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), RealType(1));
        Vec4<RealType> d_f_ver_d_kf_depth_ = uniforms.pose_matrix * Vec4<RealType>(kf_ray(0),
                                                                                   kf_ray(1),
                                                                                   kf_ray(2),
                                                                                   RealType(0));
        Vec3<RealType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.texcoord = vertexdata.texcoord;
        outVarying.kf_ver = vertexdata.vertex;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
#pragma HLS INLINE

        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> kf_ray_0 = in_varying.kf_ray_0;
        Vec3<RealType> kf_ray_1 = in_varying.kf_ray_1;
        Vec3<RealType> kf_ray_2 = in_varying.kf_ray_2;
        Vec2<RealType> texcoord = in_varying.texcoord;
        Vec3<RealType> baricentric = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.diffuse_texture,
                                                                   texcoord(1), texcoord(0));

        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //    return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_df = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_df * d_f_d_xy;

        Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        Vec3<RealType> d_f_i_d_tra = d_f_i_d_f_ver;
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2),
                                                    f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2),
                                                    -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        RealType d_f_i_d_kf_depth_0 = (d_f_i_d_f_ver.transpose() * kf_ray_0)(0, 0);
        RealType d_f_i_d_kf_depth_1 = (d_f_i_d_f_ver.transpose() * kf_ray_1)(0, 0);
        RealType d_f_i_d_kf_depth_2 = (d_f_i_d_f_ver.transpose() * kf_ray_2)(0, 0);

        Vec3<RealType> d_depth_d_vert_depth = baricentric;

        Vec3<RealType> jac;
        jac(0) = d_f_i_d_kf_depth_0 * d_depth_d_vert_depth(0);
        jac(1) = d_f_i_d_kf_depth_1 * d_depth_d_vert_depth(1);
        jac(2) = d_f_i_d_kf_depth_2 * d_depth_d_vert_depth(2);

        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.jtra = d_f_i_d_tra;
        fragment.jrot = d_f_i_d_rot;
        fragment.jexp = d_fexp_d_exp;
        fragment.jmap = jac;
        fragment.pids = ids;
        fragment.image = f_exp;
    }

    static void sync_outtextures(OutTextures &textures, const BoundingBox<IntType> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
    {
#pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                Vec3<RealType> jtra = fragment_buffer[address].jtra;
                Vec3<RealType> jrot = fragment_buffer[address].jrot;
                Vec3<RealType> jexp = fragment_buffer[address].jexp;
                Vec3<RealType> jmap = fragment_buffer[address].jmap;
                Vec3<IntType> pids = fragment_buffer[address].pids;
                RealType image = fragment_buffer[address].image;

                Vec3<float> jtra_out(jtra(0), jtra(1), jtra(2));
                Vec3<float> jrot_out(jrot(0), jrot(1), jrot(2));
                Vec3<float> jexp_out(jexp(0), jexp(1), jexp(2));
                Vec3<float> jmap_out(jmap(0), jmap(1), jmap(2));
                Vec3<PidType> pids_out(pids(0), pids(1), pids(2));

                textures.jtra_texture(y, x) = jtra_out;
                textures.jrot_texture(y, x) = jrot_out;
                textures.jexp_texture(y, x) = jexp_out;
                textures.jmap_texture(y, x) = jmap_out;
                textures.pids_texture(y, x) = pids_out;
                textures.image_texture(y, x) = image;
            }
        }
    }
};
