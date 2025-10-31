#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
#include "linalg/linalg.h"
#include "backends/base/texturebase.h"
#include "core/render_constants.h"
// #include "core/error_handling.h"

// template <typename T>
// inline T cross(const linalg::Vec2<T> &a, const linalg::Vec2<T> &b) { return a(0) * b(1) - a(1) * b(0); }

// template <typename T>
// inline T triangle_area(const linalg::Vec2<T> &p0, const linalg::Vec2<T> &p1, const linalg::Vec2<T> &p2) { return cross<T>(p1 - p0, p2 - p0); }

// Edge function E_ab(p) = (vout[1].screen(1)-vout[0].screen(1))*px + (vout[0].screen(0)-vout[1].screen(0))*py + (vout[1].screen(0)*vout[0].screen(1) - vout[0].screen(0)*vout[1].screen(1))
template <typename T>
T edge_func(const linalg::Vec2<T> &v0, const linalg::Vec2<T> &v1, const linalg::Vec2<T> &v2)
{
#pragma HLS INLINE
    // return (y1 - y0) * (px - x0) + (x0 - x1) * (py - y0);
    //  return (by - ay) * px + (ax - bx) * py + (bx * ay - ax * by);
    linalg::Vec2<T> v10 = v1 - v0;
    linalg::Vec2<T> v20 = v2 - v0;
    // for y up
    // return v10.cross(v20);
    // for y down
    return v20.cross(v10);
    // return (v2(1) - v0(1)) * (v1(0) - v0(0)) - (v2(0) - v0(0)) * (v1(1) - v0(1));
}

// Top-left test: returns true if edge is a "top" or "left" edge
template <typename T>
bool is_top_left(const linalg::Vec2<T> &v0, const linalg::Vec2<T> &v1)
{
#pragma HLS INLINE
    // return (v0(1) == v1(1)) ? (v1(0) < v0(0)) : (v0(1) < v1(1));
    // for y up
    // return (v0(1) < v1(1)) || (v0(1) == v1(1) && v0(0) > v1(0));
    // for y down
    return (v0(1) > v1(1)) || (v0(1) == v1(1) && v0(0) > v1(0));
}

// -----------------------------------------------------------------------------
// RendererBase
// -----------------------------------------------------------------------------
template <typename MathType, class Derived>
class RendererBase
{
public:
    static constexpr int tile_width = 80;
    static constexpr int tile_height = 60;

    // static constexpr int max_width = 640;
    // static constexpr int max_height = 480;

    static constexpr int max_num_tri = 2048;

    static constexpr int max_num_tiles_x = 8;
    static constexpr int max_num_tiles_y = 8;
    static constexpr int max_num_tiles = max_num_tiles_x * max_num_tiles_y;

    static constexpr int max_tri_per_tile = max_num_tri;

    // only for performance metrics
    static constexpr int max_tri_width = 20;
    static constexpr int max_tri_height = 15;

    // Vertex shading & clip → NDC → screen
    struct VSOut
    {
        linalg::Vec2<MathType> screen; // x,y in pixel space (float)
        MathType depth;                // z in [0,1] if your projection is like GL_ZERO_TO_ONE
        MathType invW;                 // 1 / clip.w
        // std::tuple<Varyings...> var_over_w; // varyings multiplied by invW
        typename Derived::Varyings var; // original varyings (for convenience)
    };

    struct Triangle
    {
        VSOut vout[3];
    };

    RendererBase()
    {
        opencv2opengl_ = linalg::Mat4<MathType>::Identity();
        opencv2opengl_(1, 1) = -1.0; // flip Z like your original intent
        opencv2opengl_(2, 2) = -1.0; // flip Z like your original intent
    };
    // virtual ~RendererBase() = default;
    ~RendererBase() = default;

    template <typename Mesh, typename InTextures, typename OutTextures>
    void Render(const BoundingBox<int> &viewport,
                Mesh &mesh,
                InTextures &intextures,
                OutTextures &outtextures)
    {
        BoundingBox<int> viewport_tiles[max_num_tiles];
        BoundingBox<int> texcoord_bound[max_num_tiles];
        Triangle triangles[max_num_tri];
        int num_triangles;
        int num_triangles_tile[max_num_tiles];
        int sum_triangles_tile;
        int id_triangles_tile[max_num_tiles][max_tri_per_tile];

// #pragma HLS BIND_STORAGE variable = viewport_tiles type = ram_t2p impl = uram
// #pragma HLS BIND_STORAGE variable = texcoord_bound type = ram_t2p impl = uram
#pragma HLS BIND_STORAGE variable = triangles type = ram_t2p impl = uram
#pragma HLS BIND_STORAGE variable = id_triangles_tile type = ram_t2p impl = uram

        // #pragma HLS ARRAY_PARTITION variable = viewport_tiles complete dim = 1
        // #pragma HLS ARRAY_PARTITION variable = triangles complete dim = 1
        // #pragma HLS ARRAY_PARTITION variable = triangle_count complete dim = 1

        int num_tiles_x = int(ceil(MathType(viewport.width_) / tile_width));
        int num_tiles_y = int(ceil(MathType(viewport.height_) / tile_height));
        int num_tiles = num_tiles_x * num_tiles_y;

        create_tile_viewports_(viewport_tiles, viewport, num_tiles_x, num_tiles_y);

        num_triangles = 0;
        sum_triangles_tile = 0;

    renderbase_init_loop:
        for (int i = 0; i < num_tiles; i++)
        {
#pragma HLS loop_tripcount min = max_num_tiles max = max_num_tiles avg = max_num_tiles

            num_triangles_tile[i] = 0;
        }

    // Loop over triangles
    renderbase_render_triangles_loop:
        for (unsigned int i = 0; i + 2 < mesh.ebo_buffer_.size(); i += 3)
        {
            // the amount of in a 32x32 mesh (31x31*2)
#pragma HLS loop_tripcount min = max_num_tri max = max_num_tri avg = max_num_tri
            // #pragma HLS PIPELINE II = 1

            unsigned int vertexids[3];

            vertexids[0] = mesh.ebo_buffer_[i + 0];
            vertexids[1] = mesh.ebo_buffer_[i + 1];
            vertexids[2] = mesh.ebo_buffer_[i + 2];

            typename Derived::VertexData vertexdata[3];

            vertexdata[0] = derived_().get_vertex_data(mesh, vertexids[0]);
            vertexdata[1] = derived_().get_vertex_data(mesh, vertexids[1]);
            vertexdata[2] = derived_().get_vertex_data(mesh, vertexids[2]);

            Triangle triangle;

            create_triangle_(vertexdata, vertexids, viewport, triangle);

            // directly write to dram
            // draw_triangle_(triangle, viewport, intextures, outtextures);

            // use tile binning
            // Back-face cull (optional). Keep CCW (area > 0) – adjust sign to your convention
            MathType area2 = edge_func(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen); // 2*area with sign

            if (area2 < MathType(0))
                continue; // enable to cull backfaces

            // Triangle bounding box (float → int, clamp to viewport)
            BoundingBox<MathType> tri_bb(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen);

            int viewport_min_x = max(viewport.min_x_, static_cast<int>(floor(tri_bb.min_x_)));
            int viewport_max_x = min(viewport.max_x_, static_cast<int>(ceil(tri_bb.max_x_)));
            int viewport_min_y = max(viewport.min_y_, static_cast<int>(floor(tri_bb.min_y_)));
            int viewport_max_y = min(viewport.max_y_, static_cast<int>(ceil(tri_bb.max_y_)));

            if (viewport_min_x >= viewport_max_x || viewport_min_y >= viewport_max_y)
                continue;

            triangles[num_triangles] = triangle;
            num_triangles++;

        renderbase_render_vertex_tile_loop:
            for (int tile = 0; tile < num_tiles; tile++)
            {
#pragma HLS loop_tripcount min = max_num_tiles max = max_num_tiles avg = max_num_tiles

                int min_x = max(viewport_tiles[tile].min_x_, static_cast<int>(floor(tri_bb.min_x_)));
                int max_x = min(viewport_tiles[tile].max_x_, static_cast<int>(ceil(tri_bb.max_x_)));
                int min_y = max(viewport_tiles[tile].min_y_, static_cast<int>(floor(tri_bb.min_y_)));
                int max_y = min(viewport_tiles[tile].max_y_, static_cast<int>(ceil(tri_bb.max_y_)));

                if (min_x >= max_x || min_y >= max_y)
                    continue;

                // = viewport_tiles[i].Intersection(tri_bb);

                id_triangles_tile[tile][num_triangles_tile[tile]] = num_triangles - 1;
                num_triangles_tile[tile]++;
                sum_triangles_tile++;

                BoundingBox<int> tex_bb = derived_().texcoord_bound(triangle.vout[0].var,
                                                                    triangle.vout[1].var,
                                                                    triangle.vout[2].var);
                if (num_triangles_tile[tile] == 1)
                    texcoord_bound[tile] = tex_bb;
                else
                    texcoord_bound[tile] = texcoord_bound[tile].Union(tex_bb);
            }
        }

        typename Derived::Fragment frags[tile_width * tile_height];
        // #pragma HLS BIND_STORAGE variable = frags type = ram_t2p impl = uram
        //  #pragma HLS ARRAY_PARTITION variable = frags complete dim = 1

    renderbase_render_tiles_loop:
        for (int tile = 0; tile < num_tiles; tile++)
        {
#pragma HLS loop_tripcount min = max_num_tiles max = max_num_tiles avg = max_num_tiles

            clear_tile_(frags, viewport_tiles[tile]);
            derived_().cache_intextures(intextures, texcoord_bound[tile]);
            render_tile_(frags, triangles, id_triangles_tile[tile], num_triangles_tile[tile], viewport_tiles[tile], intextures);
            write_tile_(frags, viewport_tiles[tile], viewport, outtextures);
        }
    }

protected:
    void create_tile_viewports_(BoundingBox<int> *viewport_tiles, const BoundingBox<int> &viewport, int num_tiles_x, int num_tiles_y)
    {
#pragma HLS INLINE off

    create_tile_viewport_y_loop:
        for (int y = 0; y < num_tiles_y; y++)
        {
#pragma HLS loop_tripcount min = max_num_tiles_y max = max_num_tiles_y avg = max_num_tiles_y

        create_tile_viewport_x_loop:
            for (int x = 0; x < num_tiles_x; x++)
            {
#pragma HLS loop_tripcount min = max_num_tiles_x max = max_num_tiles_x avg = max_num_tiles_x

                int min_x_ = int(MathType(viewport.width_ * x) / MathType(num_tiles_x)) + viewport.min_x_;
                int max_x_ = int(MathType(viewport.width_ * (x + 1)) / MathType(num_tiles_x)) + viewport.min_x_;
                int min_y_ = int(MathType(viewport.height_ * y) / MathType(num_tiles_y)) + viewport.min_y_;
                int max_y_ = int(MathType(viewport.height_ * (y + 1)) / MathType(num_tiles_y)) + viewport.min_y_;

                viewport_tiles[y * num_tiles_x + x] = BoundingBox<int>(min_x_, max_x_, min_y_, max_y_);
            }
        }
    }

    template <typename VertexData>
    void create_triangle_(const VertexData *vertexdata, const unsigned int *vertexids, const BoundingBox<int> &viewport, Triangle &triangle)
    {
#pragma HLS INLINE off

    create_triangle_loop:
        for (int j = 0; j < 3; ++j)
        {
            // #pragma HLS UNROLL

            linalg::Vec4<MathType> gl_Position;
            typename Derived::Varyings varyings;
            derived_().vertex_shader(vertexdata[j], vertexids[j], gl_Position, varyings);

            const MathType invW = MathType(1) / gl_Position(3);
            const MathType ndc_x = gl_Position(0) * invW; // [-1,1]
            const MathType ndc_y = gl_Position(1) * invW;
            const MathType ndc_z = gl_Position(2) * invW; // assumed 0..1 after proj (adjust if -1..1)

            // pixel-space (don’t clamp here) — match GL rasterization (remove +1/-0.5 adjustment)
            triangle.vout[j].screen(0) = MathType(0.5) * (ndc_x + MathType(1)) * viewport.width_ + viewport.min_x_;
            triangle.vout[j].screen(1) = MathType(0.5) * (ndc_y + MathType(1)) * viewport.height_ + viewport.min_y_;
            // triangle.vout[i].screen(0) = MathType(0.5) * (ndc_x + MathType(1));
            // triangle.vout[i].screen(1) = MathType(0.5) * (ndc_y + MathType(1));
            triangle.vout[j].depth = MathType(0.5) * (ndc_z + MathType(1));
            triangle.vout[j].invW = invW;
            triangle.vout[j].var = varyings;
            // vout[i].var_over_w = varyings * invW; // requires T*VaryingType
        }
    }

    template <typename Fragment>
    void clear_tile_(Fragment *frags, const BoundingBox<int> &viewport_tile)
    {
#pragma HLS INLINE off

    clear_tile_y_loop:
        for (int y = 0; y < viewport_tile.height_; y++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        clear_tile_x_loop:
            for (int x = 0; x < viewport_tile.width_; x++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                frags[y * viewport_tile.width_ + x] = derived_().nodata_;
            }
        }
    }

    template <typename Fragment, typename InTextures>
    void render_tile_(Fragment *frags, const Triangle *triangles, const int *id_triangles, int num_triangles, const BoundingBox<int> &viewport_tile, const InTextures &intextures)
    {
#pragma HLS INLINE off

    render_tile_loop:
        for (int tri = 0; tri < num_triangles; tri++)
        {
#pragma HLS pipeline off
#pragma HLS loop_tripcount min = max_tri_per_tile max = max_tri_per_tile avg = max_tri_per_tile

            int tri_id = id_triangles[tri];
            Triangle triangle = triangles[tri_id];
            draw_triangle_(triangle, viewport_tile, intextures, frags);
        }
    }

    template <typename Fragment, typename OutTextures>
    void write_tile_(const Fragment *frags, const BoundingBox<int> &viewport_tile, const BoundingBox<int> &viewport, OutTextures &outtextures)
    {
#pragma HLS INLINE off

    write_tile_y_loop:
        for (int iy = 0; iy < viewport_tile.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

            int y = iy + viewport_tile.min_y_;

            int ram_base = y * viewport.width_ + viewport_tile.min_x_;
            int bram_base = iy * viewport_tile.width_;

        write_tile_x_loop:
            for (int ix = 0; ix < viewport_tile.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width
#pragma HLS loop_flatten off
                // #pragma HLS PIPELINE II = 1

                Fragment frag = frags[bram_base + ix];
                derived_().write_fragment(frag,
                                          ram_base + ix,
                                          outtextures);
            }
        }
    }

    // Triangle rasterizer (top-left rule, perspective correct)
    template <typename InTextures, typename Fragment>
    void draw_triangle_(const Triangle &triangle, const BoundingBox<int> &tile_bb, const InTextures &intextures, Fragment *tile_frags)
    // template <typename InTextures, typename OutTextures>
    // void draw_triangle_(const Triangle &triangle, const BoundingBox<int> &tile_bb, const InTextures &intextures, OutTextures &outtextures)
    {
#pragma HLS INLINE

        BoundingBox<MathType> tri_bb(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen);

        int min_x = max(tile_bb.min_x_, static_cast<int>(floor(tri_bb.min_x_)));
        int max_x = min(tile_bb.max_x_, static_cast<int>(ceil(tri_bb.max_x_)));
        int min_y = max(tile_bb.min_y_, static_cast<int>(floor(tri_bb.min_y_)));
        int max_y = min(tile_bb.max_y_, static_cast<int>(ceil(tri_bb.max_y_)));

        BoundingBox<int> triangle_bb(min_x, max_x, min_y, max_y);

        // Back-face cull (optional). Keep CCW (area > 0) – adjust sign to your convention
        MathType area2 = edge_func(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen); // 2*area with sign

        if (area2 < MathType(0))
            return; // enable to cull backfaces

        const MathType inv_area2 = MathType(1) / area2;

        const bool tlAB = is_top_left(triangle.vout[0].screen, triangle.vout[1].screen);
        const bool tlBC = is_top_left(triangle.vout[1].screen, triangle.vout[2].screen);
        const bool tlCA = is_top_left(triangle.vout[2].screen, triangle.vout[0].screen);

        // Evaluate edge functions at top-left corner of each pixel (add +0.5)
        linalg::Vec2<MathType> p;
        p(0) = static_cast<MathType>(triangle_bb.min_x_) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
        p(1) = static_cast<MathType>(triangle_bb.min_y_) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);

        MathType eAB_row = edge_func(triangle.vout[0].screen, triangle.vout[1].screen, p);
        MathType eBC_row = edge_func(triangle.vout[1].screen, triangle.vout[2].screen, p);
        MathType eCA_row = edge_func(triangle.vout[2].screen, triangle.vout[0].screen, p);

        // Step increments when moving +1 in X or +1 in Y
        // const MathType eAB_dx = (vout[0].screen(1) - vout[1].screen(1));
        // const MathType eAB_dy = (vout[1].screen(0) - vout[0].screen(0));
        // const MathType eBC_dx = (vout[1].screen(1) - vout[2].screen(1));
        // const MathType eBC_dy = (vout[2].screen(0) - vout[1].screen(0));
        // const MathType eCA_dx = (vout[2].screen(1) - vout[0].screen(1));
        // const MathType eCA_dy = (vout[0].screen(0) - vout[2].screen(0));
        // for y down, the - is needed
        const MathType eAB_dx = (triangle.vout[1].screen(1) - triangle.vout[0].screen(1));
        const MathType eAB_dy = (triangle.vout[0].screen(0) - triangle.vout[1].screen(0));
        const MathType eBC_dx = (triangle.vout[2].screen(1) - triangle.vout[1].screen(1));
        const MathType eBC_dy = (triangle.vout[1].screen(0) - triangle.vout[2].screen(0));
        const MathType eCA_dx = (triangle.vout[0].screen(1) - triangle.vout[2].screen(1));
        const MathType eCA_dy = (triangle.vout[2].screen(0) - triangle.vout[0].screen(0));

    // Rasterize
    draw_triangle_y_loop:
        // for (int y = bb.min_y_, iy = 0; y < bb.max_y_; ++y, ++iy)
        for (int iy = 0; iy < triangle_bb.height_; ++iy)
        {
            // for 32x32 meshes and 640x480 images
#pragma HLS loop_tripcount min = max_tri_height max = max_tri_height avg = max_tri_height

            int y = iy + triangle_bb.min_y_;

            const MathType eAB_row_local = MathType(iy) * eAB_dy + eAB_row;
            const MathType eBC_row_local = MathType(iy) * eBC_dy + eBC_row;
            const MathType eCA_row_local = MathType(iy) * eCA_dy + eCA_row;

        draw_triangle_x_loop:
            // for (int x = bb.min_x_, ix = 0; x < bb.max_x_; ++x, ++ix)
            for (int ix = 0; ix < triangle_bb.width_; ++ix)
            {
                // for 32x32 meshes and 640x480 images
#pragma HLS loop_tripcount min = max_tri_width max = max_tri_width avg = max_tri_width
#pragma HLS loop_flatten
                //   #pragma HLS PIPELINE II = 1

                int x = ix + triangle_bb.min_x_;

                const MathType eAB = MathType(ix) * eAB_dx + eAB_row_local;
                const MathType eBC = MathType(ix) * eBC_dx + eBC_row_local;
                const MathType eCA = MathType(ix) * eCA_dx + eCA_row_local;

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
                MathType w0 = eBC * inv_area2 * triangle.vout[0].invW;
                MathType w1 = eCA * inv_area2 * triangle.vout[1].invW;
                MathType w2 = eAB * inv_area2 * triangle.vout[2].invW;

                // Perspective: 1/w at pixel
                const MathType inv_invW_px = MathType(1) / (w0 + w1 + w2);

                w0 *= inv_invW_px;
                w1 *= inv_invW_px;
                w2 *= inv_invW_px;

                typename Derived::Varyings varying_px = derived_().interpolate_varyings(w0, w1, w2,
                                                                                        triangle.vout[0].var,
                                                                                        triangle.vout[1].var,
                                                                                        triangle.vout[2].var);

                // Depth (if needed; same trick)
                MathType depth_px = w0 * triangle.vout[0].depth +
                                    w1 * triangle.vout[1].depth +
                                    w2 * triangle.vout[2].depth;

                // Depth test could go here

                linalg::Vec4<MathType> gl_FragCoord;
                gl_FragCoord(0) = static_cast<MathType>(x) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
                gl_FragCoord(1) = static_cast<MathType>(y) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
                gl_FragCoord(2) = depth_px;
                gl_FragCoord(3) = inv_invW_px;

                int frag_address = (y - tile_bb.min_y_) * tile_bb.width_ + x - tile_bb.min_x_;

                derived_().fragment_shader(gl_FragCoord,
                                           varying_px,
                                           intextures,
                                           tile_frags[frag_address]);

                /*
                typename Derived::Fragment frag;
                if (derived_().fragment_shader(gl_FragCoord,
                                               varying_px,
                                               intextures,
                                               frag))
                {
                    derived_().write_fragment(frag,
                                              y * tile_bb.width_ + x,
                                              outtextures);
                }
                */
            }
        }
    }

    Derived &derived_() { return *static_cast<Derived *>(this); }
    const Derived &derived_() const { return *static_cast<const Derived *>(this); }

    linalg::Mat4<MathType> opencv2opengl_;
};

template <typename MathType, typename OutType, class Mesh, template <class> class Texture>
class GouraudRendererBase
    : public RendererBase<MathType, GouraudRendererBase<MathType, OutType, Mesh, Texture>>
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

    struct Varyings
    {
        linalg::Vec3<OutType> vColor;
    };

    struct Fragment
    {
        linalg::Vec3<OutType> vColor;
    };

    GouraudRendererBase() = default;
    ~GouraudRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                const linalg::Vec3<MathType> &light_pos,
                const linalg::Vec3<MathType> &light_color,
                const linalg::Vec3<MathType> &ambient_reflectance,
                const linalg::Vec3<MathType> &diffuse_reflectance,
                const linalg::Vec3<MathType> &specular_reflectance,
                const MathType shininess,
                const linalg::Vec3<MathType> &ambient_light,
                unsigned int out_lvl,
                Texture<linalg::Vec3<OutType>> &out_texture)
    {
        nodata_.vColor = out_texture.nodata();

        linalg::SE3<MathType> cam2world = pose.inverse();

        // model already in world space
        uModel_ = linalg::Mat4<MathType>::Identity();
        uView_ = this->opencv2opengl_ * pose.matrix();
        uProjection_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uNormalMatrix_ = linalg::Mat3<MathType>::Identity(); // linalg::Mat3<MathType>(uModel_).inverse().transpose();

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

        RendererBase<MathType, GouraudRendererBase>::Render(viewport, mesh, intextures, outtextures);
    }

    BoundingBox<int> texcoord_bound(const Varyings &var0, const Varyings &avar1, const Varyings &avar2)
    {
        BoundingBox<int> bb(0, 0, 0, 0);
        return bb;
    }

    void cache_intextures(InTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void cache_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void sync_outtextures(OutTextures &textures)
    {

    }

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

    bool fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         const InTextures &textures,
                         Fragment &frag)
    {
        // std::cout << "calling fragment shader " << std::endl;
        // if (!inside)
        //    return;

        // textures.out_texture.set_texel_(in_varying.vColor, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        frag.vColor = in_varying.vColor;
        return true;
    }

    void write_fragment(const Fragment &frag, int address, OutTextures &textures)
    {
        textures.out_texture.set_texel_(frag.vColor, address, out_lvl_);
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

/*
template <typename MathType, typename DepthType, class Mesh, template <class> class Texture>
class PhongRendererBase
    : public RendererBase<MathType, DepthRendererBase<MathType, DepthType, Mesh, Texture>>
{
public:
    // struct Buffers
    //{
    //     const Buffer<MathType> &pos_buffer;
    //     const Buffer<unsigned int> &ebo_buffer;
    // };

    struct Textures
    {
        Texture<DepthType> &out_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec3<MathType> normal;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        float depth;
    };

    PhongRendererBase() = default;
    ~PhongRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                unsigned int out_lvl,
                Texture<DepthType> &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_ * pose.matrix();
        out_lvl_ = out_lvl;

        // out_texture_ = &out_texture;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Buffers buffers{mesh.pos_buffer_, mesh.ebo_buffer_};
        Textures textures{out_texture};

        RendererBase<MathType, DepthRendererBase>::Render(viewport, mesh, textures);
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.pos_buffer_[vertexid * 3 + 0];
        vertexdata.vertex(1) = mesh.pos_buffer_[vertexid * 3 + 1];
        vertexdata.vertex(2) = mesh.pos_buffer_[vertexid * 3 + 2];

        vertexdata.normal(0) = mesh.nor_buffer_[vertexid * 3 + 0];
        vertexdata.normal(1) = mesh.nor_buffer_[vertexid * 3 + 1];
        vertexdata.normal(2) = mesh.nor_buffer_[vertexid * 3 + 2];

        vertexdata.texcoord(0) = mesh.tex_buffer_[vertexid * 2 + 0];
        vertexdata.texcoord(1) = mesh.tex_buffer_[vertexid * 2 + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
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
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        // #pragma HLS INLINE
        //  std::cout << "calling vertex shader " << std::endl;
        gl_Position = t_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        outVarying.depth = vertexdata.vertex(2);
    }

    void fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        // std::cout << "calling fragment shader " << std::endl;
        textures.out_texture.set_texel_(in_varying.depth, int(gl_FragCoord(1)), int(gl_FragCoord(0)), out_lvl_);
    }

    linalg::Mat4<MathType> t_matrix_;
    unsigned int out_lvl_;
};
*/
// -----------------------------------------------------------------------------
// DepthRenderer
//   Evout[0].screen(0)mple derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

template <typename MathType, typename DepthType, class Mesh, template <class> class Texture>
class DepthRendererBase
    : public RendererBase<MathType, DepthRendererBase<MathType, DepthType, Mesh, Texture>>
{
public:
    struct InTextures
    {
        const MathType unused;
    };

    struct OutTextures
    {
        // Texture<DepthType> &out_texture;
        DepthType *out_data;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
    };

    struct Varyings
    {
        MathType depth;
    };

    struct Fragment
    {
        DepthType depth;
    };

    DepthRendererBase() = default;
    ~DepthRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                unsigned int out_lvl,
                Texture<DepthType> &out_texture)
    {
        nodata_.depth = out_texture.nodata();

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_ * pose.matrix();
        out_lvl_ = out_lvl;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        InTextures intextures{0};
        OutTextures outtextures{out_texture.data(out_lvl_)};

        RendererBase<MathType, DepthRendererBase>::Render(viewport, mesh, intextures, outtextures);
    }

    BoundingBox<int> texcoord_bound(const Varyings &var0, const Varyings &avar1, const Varyings &avar2)
    {
        BoundingBox<int> bb(0, 0, 0, 0);
        return bb;
    }

    void cache_intextures(InTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void cache_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void sync_outtextures(OutTextures &textures)
    {

    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
#pragma HLS INLINE

        VertexData vertexdata;
        unsigned int base = vertexid * mesh.stride_ + mesh.pos_offset_;
        vertexdata.vertex(0) = mesh.vertex_buffer_[base + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[base + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[base + 2];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
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
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        // #pragma HLS INLINE
        //   std::cout << "calling vertex shader " << std::endl;
        gl_Position = t_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        outVarying.depth = gl_Position(2);
    }

    bool fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         const InTextures &textures,
                         Fragment &frag)
    {
#pragma HLS INLINE

        MathType depth = in_varying.depth;

        // if (frag.depth != nodata_.depth && depth > frag.depth)
        //     return;

        frag.depth = depth;
        return true;
    }

    void write_fragment(const Fragment &frag, int address, OutTextures &textures)
    {
#pragma HLS INLINE
        // DepthType* __restrict data = textures.out_data;
        // data[address] = frag.depth;
        textures.out_data[address] = frag.depth;
    }

    linalg::Mat4<MathType> t_matrix_;
    unsigned int out_lvl_;
    Fragment nodata_;
};

// -----------------------------------------------------------------------------
// ImageRendererBase
//   Another evout[0].screen(0)mple derived class that might output color
// -----------------------------------------------------------------------------

template <typename MathType, typename ImageType, class Mesh, template <class> class DiffuseTexture, template <class> class OutTexture>
class ImageRendererBase
    : public RendererBase<MathType, ImageRendererBase<MathType, ImageType, Mesh, DiffuseTexture, OutTexture>>
{
public:
    // struct Buffers
    //{
    //     const Buffer<MathType> &pos_buffer;
    //     const Buffer<MathType> &tex_buffer;
    //     const Buffer<unsigned int> &ebo_buffer;
    // };

    struct InTextures
    {
        DiffuseTexture<ImageType> &in_texture;
        // const ImageType *in_data;
    };

    struct OutTextures
    {
        // OutTexture<ImageType> &out_texture;
        ImageType *out_data;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
    };

    struct Fragment
    {
        ImageType color;
        MathType depth;
    };

    ImageRendererBase() = default;
    ~ImageRendererBase() = default;

    void Render(Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                OutTexture<ImageType> &out_texture)
    {
        // #pragma HLS inline

        nodata_.color = out_texture.nodata();
        nodata_.depth = MathType(-1);

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                    this->opencv2opengl_ *
                    pose.matrix();

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        InTextures intextures{mesh.diffuse_};
        // InTextures intextures{mesh.diffuse_.data(in_lvl_)};
        OutTextures outtextures{out_texture.data(out_lvl_)};

        in_textues_size_ = linalg::Vec2<unsigned int>(mesh.diffuse_.width(in_lvl_), mesh.diffuse_.height(in_lvl_));

        RendererBase<MathType, ImageRendererBase>::Render(viewport, mesh, intextures, outtextures);
    }

    BoundingBox<int> texcoord_bound(const Varyings &var0, const Varyings &var1, const Varyings &var2)
    {
        linalg::Vec2<int> texcoord0(var0.texcoord(0) * in_textues_size_(0), var0.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord1(var1.texcoord(0) * in_textues_size_(0), var1.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord2(var2.texcoord(0) * in_textues_size_(0), var2.texcoord(1) * in_textues_size_(1));

        BoundingBox<int> bb(texcoord0, texcoord1, texcoord2);
        return bb;
    }

    void cache_intextures(InTextures &textures, const BoundingBox<int> &tex_bb)
    {
        textures.in_texture.set_cache_bb_(tex_bb, in_lvl_);
        textures.in_texture.cache_read_();
    }

    void cache_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb)
    {
        textures.out_texture.set_cache_bb_(tex_bb, out_lvl_);
    }

    void sync_outtextures(OutTextures &textures)
    {
        textures.out_texture.cache_write_();
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
#pragma HLS inline

        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
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
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
#pragma HLS inline

        gl_Position = t_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1.0f));
        outVarying.texcoord = vertexdata.texcoord;
    }

    bool fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         const InTextures &textures,
                         Fragment &frag)
    {
#pragma HLS inline

        MathType depth = gl_FragCoord(2);

        if (frag.depth != nodata_.depth && depth > frag.depth)
            return false;

        MathType pix = sample<MathType, DiffuseTexture<ImageType>>(textures.in_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        // linalg::Vec2<MathType> screen_texcoord(in_varying.texcoord(0) * in_textues_size_(0), in_varying.texcoord(1) * in_textues_size_(1));
        // int address = screen_texcoord(1) * in_textues_size_(0) + screen_texcoord(0);
        // MathType pix = textures.in_data[address];

        // linalg::Vec2<MathType> screen_texcoord(in_varying.texcoord(0) * textures.in_texture.width(in_lvl_), in_varying.texcoord(1) * textures.in_texture.height(in_lvl_));
        // MathType pix = textures.in_texture.texel_(screen_texcoord(1), screen_texcoord(0), in_lvl_);

        frag.color = pix;
        frag.depth = depth;
        return true;
    }

    void write_fragment(const Fragment &frag, int address, OutTextures &textures)
    {
#pragma HLS INLINE
        textures.out_data[address] = frag.color;
    }

    linalg::Vec2<unsigned int> in_textues_size_;
    linalg::Mat4<MathType> t_matrix_;
    unsigned int in_lvl_;
    unsigned int out_lvl_;
    Fragment nodata_;
};

template <typename MathType, typename ImageType, typename ErrorType, class Mesh, template <class> class Texture>
class ResidualRendererBase
    : public RendererBase<MathType, ResidualRendererBase<MathType, ImageType, ErrorType, Mesh, Texture>>
{
public:
    struct InTextures
    {
        Texture<ImageType> &kf_texture;
        Texture<ImageType> &f_texture;
    };

    struct OutTextures
    {
        Texture<ErrorType> &r_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
    };

    struct Fragment
    {
        ErrorType r;
    };

    ResidualRendererBase() = default;
    ~ResidualRendererBase() = default;

    void Render(Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                Texture<ImageType> &kf_texture,
                Texture<ImageType> &f_texture,
                Texture<ErrorType> &r_texture)
    {
        // r_texture.fill(out_lvl, r_texture.nodata());
        nodata_.r = r_texture.nodata();

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                    this->opencv2opengl_ * pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        InTextures intextures{kf_texture, f_texture};
        OutTextures outtextures{r_texture};

        RendererBase<MathType, ResidualRendererBase>::Render(viewport, mesh, intextures, outtextures);
    }

    BoundingBox<int> texcoord_bound(const Varyings &var0, const Varyings &var1, const Varyings &var2)
    {
        /*
        linalg::Vec2<int> texcoord0(var0.texcoord(0) * in_textues_size_(0), var0.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord1(var1.texcoord(0) * in_textues_size_(0), var1.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord2(var2.texcoord(0) * in_textues_size_(0), var2.texcoord(1) * in_textues_size_(1));

        BoundingBox<int> bb(texcoord0, texcoord1, texcoord2);
        return bb;
        */
        return BoundingBox<int>(0, 0, 0, 0);
    }

    void cache_intextures(InTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void cache_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void sync_outtextures(OutTextures &textures)
    {

    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
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
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        gl_Position = t_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        outVarying.texcoord = vertexdata.texcoord;
    }

    bool fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         const InTextures &textures,
                         Fragment &frag)
    {
        int width = textures.kf_texture.width(out_lvl_);
        int height = textures.kf_texture.height(out_lvl_);

        linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        ImageType kf = sample<ImageType, Texture<ImageType>>(textures.kf_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        ImageType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = f_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        // if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata())
        //     return;

        ErrorType e = ErrorType(f) - ErrorType(kf);
        // textures.r_texture.set_texel_(e, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        frag.r = e;
        return true;
    }

    void write_fragment(const Fragment &frag, int address, OutTextures &textures)
    {
        textures.r_texture.set_texel_(frag.r, address, out_lvl_);
    }

    Fragment nodata_;

private:
    linalg::Mat4<MathType> t_matrix_;
    int in_lvl_;
    int out_lvl_;
};

template <typename MathType, typename ImageType, typename DType, class Mesh, template <class> class Texture>
class DIDxyRendererBase
    : public RendererBase<MathType, DIDxyRendererBase<MathType, ImageType, DType, Mesh, Texture>>
{
public:
    struct InTextures
    {
        Texture<ImageType> &in_texture;
    };
    struct OutTextures
    {
        Texture<linalg::Vec3<DType>> &out_texture;
    };

    struct VertexData
    {
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
    };

    struct Fragment
    {
        linalg::Vec3<DType> color;
    };

    DIDxyRendererBase() = default;
    ~DIDxyRendererBase() = default;

    void Render(Mesh &mesh,
                int in_lvl,
                int out_lvl,
                Texture<ImageType> &in_texture,
                Texture<linalg::Vec3<DType>> &out_texture)
    {
        nodata_.color = out_texture.nodata();

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        InTextures intextures{in_texture};
        OutTextures outtextures{out_texture};

        RendererBase<MathType, DIDxyRendererBase>::Render(viewport, mesh, intextures, outtextures);
    }

    BoundingBox<int> texcoord_bound(const Varyings &var0, const Varyings &var1, const Varyings &var2)
    {
        /*
        linalg::Vec2<int> texcoord0(var0.texcoord(0) * in_textues_size_(0), var0.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord1(var1.texcoord(0) * in_textues_size_(0), var1.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord2(var2.texcoord(0) * in_textues_size_(0), var2.texcoord(1) * in_textues_size_(1));

        BoundingBox<int> bb(texcoord0, texcoord1, texcoord2);
        return bb;
        */
        return BoundingBox<int>(0, 0, 0, 0);
    }

    void cache_intextures(InTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void cache_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void sync_outtextures(OutTextures &textures)
    {

    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
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
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        // gl_Position = (view_matrix * pose_matrix) * linalg::Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = linalg::Vec4<MathType>(2.0 * vertexdata.texcoord(0) - 1.0, 2.0 * vertexdata.texcoord(1) - 1.0, 0.0, 1.0);
        outVarying.texcoord = vertexdata.texcoord;
    }

    bool fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         const InTextures &textures,
                         Fragment &frag)
    {
        unsigned int height = textures.in_texture.height(out_lvl_);
        unsigned int width = textures.in_texture.width(out_lvl_);
        ImageType nodata = textures.in_texture.nodata();

        int x = int(in_varying.texcoord(0) * (width - 1));
        int y = int(in_varying.texcoord(1) * (height - 1));
        int x_p = x + 1;
        int x_m = x - 1;
        int y_p = y + 1;
        int y_m = y - 1;

        if (x_p >= width || x_m < 0 || y_p >= height || y_m < 0)
        {
            //  No need to explicitly set to nodata, it is already in the background color
            //  outFragment(0) = 0.0f;
            //  outFragment(1) = 0.0f;
            //  outFragment(2) = 0.0f;
            return false;
        }

        ImageType f = textures.in_texture.texel_(y, x, out_lvl_);
        ImageType f_y_p = textures.in_texture.texel_(y_p, x, out_lvl_);
        ImageType f_y_m = textures.in_texture.texel_(y_m, x, out_lvl_);
        ImageType f_x_p = textures.in_texture.texel_(y, x_p, out_lvl_);
        ImageType f_x_m = textures.in_texture.texel_(y, x_m, out_lvl_);

        if (f_x_p == nodata || f_x_m == nodata ||
            f_y_p == nodata || f_y_m == nodata || f == nodata)
        {
            //  No need to explicitly set to nodata, it is already in the background color
            //  outFragment(0) = 0.0f;
            //  outFragment(1) = 0.0f;
            //  outFragment(2) = 0.0f;
            return false;
        }

        linalg::Vec3<MathType> out_fragment;
        out_fragment(0) = (f_x_p - f_x_m) / 2.0f;
        out_fragment(1) = (f_y_p - f_y_m) / 2.0f;
        out_fragment(2) = 0.0; // f; // save the projected frame for later processing

        // textures.out_texture.set_texel_(out_fragment, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        frag.color = out_fragment;
        return true;
    }

    void write_fragment(const Fragment &frag, int address, OutTextures &textures)
    {
        textures.out_texture.set_texel_(frag.color, address, out_lvl_);
    }

    Fragment nodata_;

private:
    int in_lvl_;
    int out_lvl_;
};

template <typename MathType, typename ImageType, typename DType, typename ErrorType, class Mesh, template <class> class Texture>
class JPoseRendererBase
    : public RendererBase<MathType, JPoseRendererBase<MathType, ImageType, DType, ErrorType, Mesh, Texture>>
{
public:
    struct InTextures
    {
        Texture<ImageType> &kf_texture;
        Texture<ImageType> &f_texture;
        Texture<linalg::Vec3<DType>> &dfdxy_texture;
    };

    struct OutTextures
    {
        Texture<linalg::Vec3<DType>> &jtra_texture;
        Texture<linalg::Vec3<DType>> &jrot_texture;
        Texture<ErrorType> &r_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
        linalg::Vec3<MathType> f_ver;
    };

    struct Fragment
    {
        linalg::Vec3<DType> jtra;
        linalg::Vec3<DType> jrot;
        ErrorType r;
    };

    JPoseRendererBase() = default;
    ~JPoseRendererBase() = default;

    void Render(Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                Texture<ImageType> &f_texture,
                Texture<linalg::Vec3<DType>> &dfdxy_texture,
                Texture<linalg::Vec3<DType>> &jtra_texture,
                Texture<linalg::Vec3<DType>> &jrot_texture,
                Texture<ErrorType> &r_texture)
    {
        nodata_.jtra = jtra_texture.nodata();
        nodata_.jrot = jrot_texture.nodata();
        nodata_.r = r_texture.nodata();

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

        // Buffers buffers{mesh.pos_buffer_, mesh.tex_buffer_, mesh.ebo_buffer_};
        InTextures intextures{mesh.diffuse_, f_texture, dfdxy_texture};
        OutTextures outtextures{jtra_texture, jrot_texture, r_texture};

        RendererBase<MathType, JPoseRendererBase>::Render(viewport, mesh, intextures, outtextures);
    }

    BoundingBox<int> texcoord_bound(const Varyings &var0, const Varyings &var1, const Varyings &var2)
    {
        /*
        linalg::Vec2<int> texcoord0(var0.texcoord(0) * in_textues_size_(0), var0.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord1(var1.texcoord(0) * in_textues_size_(0), var1.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord2(var2.texcoord(0) * in_textues_size_(0), var2.texcoord(1) * in_textues_size_(1));

        BoundingBox<int> bb(texcoord0, texcoord1, texcoord2);
        return bb;
        */
        return BoundingBox<int>(0, 0, 0, 0);
    }

    void cache_intextures(InTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void cache_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void sync_outtextures(OutTextures &textures)
    {

    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
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
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        linalg::Vec4<MathType> f_ver = pose_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        gl_Position = view_matrix_ * f_ver;

        outVarying.f_ver = linalg::Vec3<MathType>(f_ver);
        outVarying.texcoord = vertexdata.texcoord;
    }

    bool fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         const InTextures &textures,
                         Fragment &frag)
    {
        unsigned int width = textures.kf_texture.width(out_lvl_);
        unsigned int height = textures.kf_texture.height(out_lvl_);

        linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        linalg::Vec3<MathType> f_ver = in_varying.f_ver;
        linalg::Vec2<MathType> texcoord = in_varying.texcoord;

        ImageType kf = sample<ImageType, Texture<ImageType>>(textures.kf_texture, texcoord(1), texcoord(0), in_lvl_);
        ImageType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        linalg::Vec3<DType> f_der = textures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = sample<T, Texture<MathType>>(textures.f_texture, screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);
        // linalg::Vec3<MathType>f_der = sample<Vec3, Texture<Vec3>>(textures.dfdxy_texture, screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        // if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata() || f_der == textures.dfdxy_texture.nodata())
        //     return;

        ErrorType r = ErrorType(f) - ErrorType(kf);

        MathType v0 = MathType(f_der(0)) * fx_ * width / f_ver(2);
        MathType v1 = MathType(f_der(1)) * fy_ * height / f_ver(2);
        MathType v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        linalg::Vec3<MathType> d_f_i_d_tra = linalg::Vec3<MathType>(v0, v1, v2);
        linalg::Vec3<MathType> d_f_i_d_rot = linalg::Vec3<MathType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        // textures.jtra_texture.set_texel_(d_f_i_d_tra, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // textures.jrot_texture.set_texel_(d_f_i_d_rot, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // textures.r_texture.set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        frag.jtra = d_f_i_d_tra;
        frag.jrot = d_f_i_d_rot;
        frag.r = r;
        return true;
    }

    void write_fragment(const Fragment &frag, int address, OutTextures &textures)
    {
        textures.jtra_texture.set_texel_(frag.jtra, address, out_lvl_);
        textures.jrot_texture.set_texel_(frag.jrot, address, out_lvl_);
        textures.r_texture.set_texel_(frag.r, address, out_lvl_);
    }

    Fragment nodata_;

private:
    int in_lvl_;
    int out_lvl_;
    MathType fx_;
    MathType fy_;
    linalg::Mat4<MathType> view_matrix_;
    linalg::Mat4<MathType> pose_matrix_;
};

template <typename MathType, typename ImageType, typename DType, typename IdType, typename ErrorType, class Mesh, template <class> class Texture>
class JMapRendererBase
    : public RendererBase<MathType, JMapRendererBase<MathType, ImageType, DType, IdType, ErrorType, Mesh, Texture>>
{
public:
    struct InTextures
    {
        Texture<ImageType> &kf_texture;
        Texture<ImageType> &f_texture;
        Texture<linalg::Vec3<DType>> &dfdxy_texture;
    };

    struct OutTextures
    {
        Texture<linalg::Vec3<DType>> &jmap_texture;
        Texture<linalg::Vec3<IdType>> &pids_texture;
        Texture<ErrorType> &r_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
        linalg::Vec3<MathType> f_ver;
        linalg::Vec3<MathType> kf_ray;
        MathType depth;
        linalg::Vec3<MathType> baricentric;
        unsigned int vertexId;
        linalg::Vec3<int> pids;
    };

    struct Fragment
    {
        linalg::Vec3<DType> jmap;
        linalg::Vec3<DType> pids;
        ErrorType r;
    };

    JMapRendererBase() = default;
    ~JMapRendererBase() = default;

    void Render(Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                Texture<ImageType> &f_texture,
                Texture<linalg::Vec3<DType>> &dfdxy_texture,
                Texture<linalg::Vec3<DType>> &jmap_texture,
                Texture<linalg::Vec3<IdType>> &pids_texture,
                Texture<ErrorType> &r_texture)
    {

        nodata_.jmap = jmap_texture.nodata();
        nodata_.pids = pids_texture.nodata();
        nodata_.r = r_texture.nodata();

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        fx_ = cam.GetParams()(0);
        fy_ = cam.GetParams()(1);
        view_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_;
        pose_matrix_ = pose.matrix();

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        InTextures intextures{mesh.diffuse_, f_texture, dfdxy_texture};
        OutTextures outtextures{jmap_texture, pids_texture, r_texture};

        RendererBase<MathType, JMapRendererBase>::Render(viewport, mesh, intextures, outtextures);
    }

    BoundingBox<int> texcoord_bound(const Varyings &var0, const Varyings &var1, const Varyings &var2)
    {
        /*
        linalg::Vec2<int> texcoord0(var0.texcoord(0) * in_textues_size_(0), var0.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord1(var1.texcoord(0) * in_textues_size_(0), var1.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord2(var2.texcoord(0) * in_textues_size_(0), var2.texcoord(1) * in_textues_size_(1));

        BoundingBox<int> bb(texcoord0, texcoord1, texcoord2);
        return bb;
        */
        return BoundingBox<int>(0, 0, 0, 0);
    }

    void cache_intextures(InTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void cache_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void sync_outtextures(OutTextures &textures)
    {

    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
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
        var_over_w_px.baricentric = linalg::Vec3<MathType>(w0,
                                                           w1,
                                                           w2);
        var_over_w_px.pids = linalg::Vec3<int>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

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
        linalg::Vec4<MathType> f_ver = pose_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        gl_Position = view_matrix_ * f_ver;

        linalg::Vec3<MathType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), MathType(1));
        linalg::Vec4<MathType> d_f_ver_d_kf_depth_ = pose_matrix_ * linalg::Vec4<MathType>(kf_ray, MathType(0));
        linalg::Vec3<MathType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = linalg::Vec3<MathType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.texcoord = vertexdata.texcoord;
    }

    bool fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         const InTextures &textures,
                         Fragment &frag)
    {
        unsigned int width = textures.kf_texture.width(out_lvl_);
        unsigned int height = textures.kf_texture.height(out_lvl_);

        // linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        linalg::Vec3<MathType> f_ver = in_varying.f_ver;
        linalg::Vec3<MathType> kf_ray = in_varying.kf_ray;
        linalg::Vec2<MathType> texcoord = in_varying.texcoord;
        linalg::Vec3<MathType> baricentric = in_varying.baricentric;
        linalg::Vec3<int> vertexid = in_varying.pids;

        ImageType kf = sample<ImageType, Texture<ImageType>>(textures.kf_texture, texcoord(1), texcoord(0), in_lvl_);
        ImageType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        linalg::Vec3<DType> f_der = textures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = f_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);
        // linalg::Vec3<MathType>f_der = dfdxy_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        // if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata() || f_der == textures.dfdxy_texture.nodata())
        //     return;

        ErrorType r = ErrorType(f) - ErrorType(kf);

        linalg::Vec3<MathType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = MathType(f_der(0)) * fx_ * width / f_ver(2);
        d_f_i_d_f_ver(1) = MathType(f_der(1)) * fy_ * height / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // linalg::Vec3<MathType>d_f_i_d_tra = linalg::Vec3<MathType>(v0, v1, v2);
        // linalg::Vec3<MathType>d_f_i_d_rot = linalg::Vec3<MathType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        linalg::Vec3<MathType> d_f_ver_d_kf_depth = kf_ray; // kfTofPose.rotationMatrix() * kf_ray;
        MathType d_f_i_d_kf_depth = (d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth)(0, 0);

        linalg::Vec3<MathType> d_depth_d_vert_depth = baricentric;

        linalg::Vec3<MathType> jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        linalg::Vec3<IdType> ids = linalg::Vec3<IdType>(vertexid(0), vertexid(1), vertexid(2));

        // textures.jmap_texture.set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // textures.pids_texture.set_texel_(ids, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // textures.r_texture.set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);

        frag.jmap = jac;
        frag.pids = ids;
        frag.r = r;
        return true;
    }

    void write_fragment(const Fragment &frag, int address, OutTextures &textures)
    {
        textures.jmap_texture.set_texel_(frag.jmap, address, out_lvl_);
        textures.pids_texture.set_texel_(frag.pids, address, out_lvl_);
        textures.r_texture.set_texel_(frag.r, address, out_lvl_);
    }

    Fragment nodata_;

private:
    int in_lvl_;
    int out_lvl_;
    MathType fx_;
    MathType fy_;
    linalg::Mat4<MathType> view_matrix_;
    linalg::Mat4<MathType> pose_matrix_;
};

template <typename MathType, typename ImageType, typename DepthType, typename DType, typename IdType, class Mesh, template <class> class Texture>
class DiffRendererBase
    : public RendererBase<MathType, DiffRendererBase<MathType, ImageType, DepthType, DType, IdType, Mesh, Texture>>
{
public:
    struct InTextures
    {
        Texture<ImageType> &diffuse_texture;
    };

    struct OutTextures
    {
        Texture<ImageType> &image_texture;
        Texture<DepthType> &depth_texture;
        Texture<linalg::Vec3<DType>> &jtra_texture;
        Texture<linalg::Vec3<DType>> &jrot_texture;
        Texture<linalg::Vec3<DType>> &jmap_texture;
        Texture<linalg::Vec3<IdType>> &pids_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
        linalg::Vec3<MathType> f_ver;
        linalg::Vec3<MathType> kf_ray;
        MathType depth;
        linalg::Vec3<MathType> baricentric;
        unsigned int vertexId;
        linalg::Vec3<int> pids;
    };

    struct Fragment
    {
        ImageType image;
        DepthType depth;
        linalg::Vec3<DType> jtra;
        linalg::Vec3<DType> jrot;
        linalg::Vec3<DType> jmap;
        linalg::Vec3<IdType> pids;
    };

    DiffRendererBase() = default;
    ~DiffRendererBase() = default;

    void Render(Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                Texture<ImageType> &image_texture,
                Texture<DepthType> &depth_texture,
                Texture<linalg::Vec3<DType>> &jtra_texture,
                Texture<linalg::Vec3<DType>> &jrot_texture,
                Texture<linalg::Vec3<DType>> &jmap_texture,
                Texture<linalg::Vec3<IdType>> &pids_texture)
    {

        nodata_.image = image_texture.nodata();
        nodata_.depth = depth_texture.nodata();
        nodata_.jtra = jtra_texture.nodata();
        nodata_.jrot = jrot_texture.nodata();
        nodata_.jmap = jmap_texture.nodata();
        nodata_.pids = pids_texture.nodata();

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        fx_ = cam.GetParams()(0);
        fy_ = cam.GetParams()(1);
        view_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_;
        pose_matrix_ = pose.matrix();

        const int W = static_cast<int>(jtra_texture.width(out_lvl));
        const int H = static_cast<int>(jtra_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        InTextures intextures{mesh.diffuse_};
        OutTextures outtextures{image_texture, depth_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture};

        RendererBase<MathType, DiffRendererBase>::Render(viewport, mesh, intextures, outtextures);
    }

    BoundingBox<int> texcoord_bound(const Varyings &var0, const Varyings &var1, const Varyings &var2)
    {
        /*
        linalg::Vec2<int> texcoord0(var0.texcoord(0) * in_textues_size_(0), var0.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord1(var1.texcoord(0) * in_textues_size_(0), var1.texcoord(1) * in_textues_size_(1));
        linalg::Vec2<int> texcoord2(var2.texcoord(0) * in_textues_size_(0), var2.texcoord(1) * in_textues_size_(1));

        BoundingBox<int> bb(texcoord0, texcoord1, texcoord2);
        return bb;
        */
        return BoundingBox<int>(0, 0, 0, 0);
    }

    void cache_intextures(InTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void cache_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb)
    {

    }

    void sync_outtextures(OutTextures &textures)
    {

    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
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
        var_over_w_px.baricentric = linalg::Vec3<MathType>(w0,
                                                           w1,
                                                           w2);

        var_over_w_px.pids = linalg::Vec3<int>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

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
        linalg::Vec4<MathType> f_ver = pose_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        gl_Position = view_matrix_ * f_ver;

        linalg::Vec3<MathType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), MathType(1));
        linalg::Vec3<MathType> d_f_ver_d_kf_depth = linalg::Vec3<MathType>(pose_matrix_ * linalg::Vec4<MathType>(kf_ray, MathType(0)));
        // linalg::Vec3<MathType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = linalg::Vec3<MathType>(f_ver);
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.texcoord = vertexdata.texcoord;
    }

    bool fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         const InTextures &textures,
                         Fragment &frag)
    {
        unsigned int width = textures.diffuse_texture.width(out_lvl_);
        unsigned int height = textures.diffuse_texture.height(out_lvl_);

        linalg::Vec3<MathType> f_ver = in_varying.f_ver;
        linalg::Vec3<MathType> kf_ray = in_varying.kf_ray;
        linalg::Vec2<MathType> texcoord = in_varying.texcoord;
        linalg::Vec3<MathType> baricentric = in_varying.baricentric;
        linalg::Vec3<int> vertexid = in_varying.pids;

        // MathType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        ImageType f = sample<ImageType, Texture<ImageType>>(textures.diffuse_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        // if (f == textures.diffuse_texture.nodata())
        //     return;

        linalg::Vec3<DType> f_der = compute_didxy<DType, linalg::Vec3, Texture>(textures.diffuse_texture, in_varying.texcoord(1) * height, in_varying.texcoord(0) * width, in_lvl_);

        // if (f_der(0) == textures.diffuse_texture.nodata() && f_der(1) == textures.diffuse_texture.nodata())
        //     return;

        linalg::Vec3<MathType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = f_der(0) * fx_ * width / f_ver(2);
        d_f_i_d_f_ver(1) = f_der(1) * fy_ * height / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // linalg::Vec3<MathType>d_f_i_d_tra = linalg::Vec3<MathType>(v0, v1, v2);
        linalg::Vec3<MathType> d_f_i_d_rot = linalg::Vec3<MathType>(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2), f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2), -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        linalg::Vec3<MathType> d_f_ver_d_kf_depth = kf_ray; // kfTofPose.rotationMatrix() * kf_ray;
        MathType d_f_i_d_kf_depth = (d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth)(0, 0);

        linalg::Vec3<MathType> d_depth_d_vert_depth = baricentric;

        linalg::Vec3<MathType> jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        linalg::Vec3<IdType> ids = linalg::Vec3<IdType>(vertexid(0), vertexid(1), vertexid(2));

        // textures.image_texture.set_texel_(f, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // textures.depth_texture.set_texel_(f_ver(2), gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // textures.jtra_texture.set_texel_(d_f_i_d_f_ver, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // textures.jrot_texture.set_texel_(d_f_i_d_rot, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // textures.jmap_texture.set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // textures.pids_texture.set_texel_(ids, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);

        frag.image = f;
        frag.depth = f_ver(2);
        frag.jtra = d_f_i_d_f_ver;
        frag.jrot = d_f_i_d_rot;
        frag.jmap = jac;
        frag.pids = ids;
        return true;
    }

    void write_fragment(const Fragment &frag, int address, OutTextures &textures)
    {
        textures.image_texture.set_texel_(frag.image, address, out_lvl_);
        textures.depth_texture.set_texel_(frag.depth, address, out_lvl_);
        textures.jtra_texture.set_texel_(frag.jtra, address, out_lvl_);
        textures.jrot_texture.set_texel_(frag.jrot, address, out_lvl_);
        textures.jmap_texture.set_texel_(frag.jmap, address, out_lvl_);
        textures.pids_texture.set_texel_(frag.pids, address, out_lvl_);
    }

    Fragment nodata_;

private:
    int in_lvl_;
    int out_lvl_;
    MathType fx_;
    MathType fy_;
    linalg::Mat4<MathType> view_matrix_;
    linalg::Mat4<MathType> pose_matrix_;
    // const TextureCPU<float> *kf_texture_;
    // const TextureCPU<float> *f_texture_;
    // const TextureCPU<Vec3> *dfdxy_texture_;
    // TextureCPU<Vec3> *jmap_texture_;
    // TextureCPU<Vec3> *pids_texture_;
    // TextureCPU<float> *r_texture_;
};
