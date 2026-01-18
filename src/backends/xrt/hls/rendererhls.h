#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
// #include "core/types.h"
#include "linalg/linalg.h"
#include "core/camera.h"
#include "core/boundingbox.h"
#include "core/render_constants.h"
#include "backends/base/rendererbase.h"
#include "backends/base/MappedView.h"
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/bufferhls.h"
#include "backends/xrt/hls/meshhls.h"

#include "hls_task.h"

template <class Derived, class Base>
class RendererBaseHLS
    : public RendererBase<Base>
{
public:
    // RendererBaseHLS() = default;
    //~RendererBaseHLS() = default;

    using Fragment = typename Base::Fragment;
    using Triangle = typename RendererBase<Base>::Triangle;

    template <typename Uniforms, typename InTextures, typename OutTextures>
    void RenderNaive(const BoundingBox<IntType> &viewport,
                     const BufferViewHLS<RealType> &vertex_buffer,
                     const BufferViewHLS<IntType> &ebo_buffer,
                     const Uniforms &uniforms,
                     const InTextures &intextures,
                     OutTextures &outtextures)
    {
        // MathType depth_buffer[viewport.width_ * viewport.height_] = {MathType(-1)};
        Fragment fragment_buffer[max_width * max_height];
        RealType depth_buffer[max_width * max_height];
#pragma HLS BIND_STORAGE variable = fragment_buffer type = ram_t2p impl = uram
#pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = uram

        for (int y = viewport.min_y_; y < viewport.max_y_; y++)
        {
            for (int x = viewport.min_x_; x < viewport.max_x_; x++)
            {
                fragment_buffer[y * viewport.width_ + x] = Base::fragment_nodata(outtextures);
                depth_buffer[y * viewport.width_ + x] = RealType(-1);
            }
        }

    // Loop over triangles
    renderbase_render_triangles_loop:
        for (unsigned int i = 0; i + 2 < ebo_buffer.size(); i += 3)
        {
#pragma HLS loop_tripcount min = 768 max = 768 avg = 768

            IntType vertexids[3];

            vertexids[0] = ebo_buffer[i + 0];
            vertexids[1] = ebo_buffer[i + 1];
            vertexids[2] = ebo_buffer[i + 2];

            typename Base::VertexData vertexdata[3];

            vertexdata[0] = Base::get_vertex_data(vertex_buffer, vertexids[0]);
            vertexdata[1] = Base::get_vertex_data(vertex_buffer, vertexids[1]);
            vertexdata[2] = Base::get_vertex_data(vertex_buffer, vertexids[2]);

            Triangle triangle;

            this->create_triangle_(vertexdata, vertexids, viewport, uniforms, triangle);

            // directly write to dram
            this->draw_triangle_(triangle, viewport, depth_buffer, uniforms, intextures, fragment_buffer);
        }

        Base::sync_outtextures(outtextures, viewport, fragment_buffer, uniforms);
    }

    template <typename Uniforms, typename InTextures, typename OutTextures>
    void RenderTiledFragBuff(const BoundingBox<IntType> &viewport,
                             const BufferViewHLS<RealType> &vertex_buffer,
                             const BufferViewHLS<IntType> &ebo_buffer,
                             const Uniforms &uniforms,
                             const InTextures &intextures,
                             OutTextures &outtextures)
    {
        Triangle triangles[max_num_tri];
#pragma HLS BIND_STORAGE variable = triangles type = ram_t2p impl = uram

        int num_triangles;
        this->get_triangles_(vertex_buffer, ebo_buffer, viewport, viewport, uniforms, triangles, max_num_tri, num_triangles);

        IntType num_tiles_x = IntType(ceil(RealType(viewport.width_) / tile_width));
        IntType num_tiles_y = IntType(ceil(RealType(viewport.height_) / tile_height));
        IntType num_tiles = num_tiles_x * num_tiles_y;

        IntType triangle_count;
        Uniforms uniforms_buffer;
        BoundingBox<IntType> viewport_buffer;
        Triangle triangle_buffer[max_tri_per_tile];
        Fragment fragment_buffer[tile_width * tile_height];
        RealType depth_buffer[tile_width * tile_height];

#pragma HLS BIND_STORAGE variable = triangle_buffer type = ram_t2p impl = bram
#pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = uram

#pragma HLS aggregate variable = fragment_buffer compact = bit
#pragma HLS BIND_STORAGE variable = fragment_buffer type = ram_t2p impl = uram

    renderbase_render_tiles_loop:
        for (int tile_y = 0; tile_y < num_tiles_y; tile_y++)
        {
#pragma HLS loop_tripcount min = max_tiles_y max = max_tiles_y avg = max_tiles_y

        renderbase_render_tiles_x_loop:
            for (int tile_x = 0; tile_x < num_tiles_x; tile_x++)
            {
                int tile = tile_y * num_tiles_x + tile_x;
#pragma HLS loop_tripcount min = max_tiles_x max = max_tiles_x avg = max_tiles_x

#pragma HLS dependence variable = fragment_buffer type = inter false
#pragma HLS dependence variable = fragment_buffer type = intra false

#pragma HLS dependence variable = depth_buffer type = inter false
#pragma HLS dependence variable = depth_buffer type = intra false

#pragma HLS dependence variable = triangle_buffer type = inter false
#pragma HLS dependence variable = triangle_buffer type = intra false

#pragma HLS dependence variable = viewport_buffer type = inter false
#pragma HLS dependence variable = viewport_buffer type = intra false

#pragma HLS dependence variable = uniforms_buffer type = inter false
#pragma HLS dependence variable = uniforms_buffer type = intra false

#pragma HLS dependence variable = intextures type = inter false
#pragma HLS dependence variable = intextures type = intra false

#pragma HLS dependence variable = outtextures type = inter false
#pragma HLS dependence variable = outtextures type = intra false

                IntType min_x_ = IntType(RealType(viewport.width_ * tile_x) / RealType(num_tiles_x)) + viewport.min_x_;
                IntType max_x_ = IntType(RealType(viewport.width_ * (tile_x + 1)) / RealType(num_tiles_x)) + viewport.min_x_;
                IntType min_y_ = IntType(RealType(viewport.height_ * tile_y) / RealType(num_tiles_y)) + viewport.min_y_;
                IntType max_y_ = IntType(RealType(viewport.height_ * (tile_y + 1)) / RealType(num_tiles_y)) + viewport.min_y_;

                viewport_buffer = BoundingBox<IntType>(min_x_, max_x_, min_y_, max_y_);
                triangle_count = 0;
                uniforms_buffer = uniforms;

            renderbase_triangle_buffer_loop:
                for (int j = 0; j < num_triangles; j++)
                {
#pragma HLS loop_tripcount min = 768 max = 768 avg = 768

                    Triangle triangle = triangles[j];
                    BoundingBox<RealType> tri_bb(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen);

                    // IntType min_x = max(viewport_buffer.min_x_, static_cast<IntType>(floor(tri_bb.min_x_)));
                    // IntType max_x = min(viewport_buffer.max_x_, static_cast<IntType>(ceil(tri_bb.max_x_)));
                    // IntType min_y = max(viewport_buffer.min_y_, static_cast<IntType>(floor(tri_bb.min_y_)));
                    // IntType max_y = min(viewport_buffer.max_y_, static_cast<IntType>(ceil(tri_bb.max_y_)));

                    IntType min_x = max(viewport_buffer.min_x_, static_cast<IntType>(tri_bb.min_x_));
                    IntType max_x = min(viewport_buffer.max_x_, static_cast<IntType>(tri_bb.max_x_ + 1));
                    IntType min_y = max(viewport_buffer.min_y_, static_cast<IntType>(tri_bb.min_y_));
                    IntType max_y = min(viewport_buffer.max_y_, static_cast<IntType>(tri_bb.max_y_ + 1));

                    if (min_x >= max_x || min_y >= max_y)
                        continue;

                    triangle_buffer[triangle_count] = triangle;
                    triangle_count = min(IntType(triangle_count + 1), IntType(max_tri_per_tile - 1));
                }

            renderbase_reset_depth_buffer_loop:
                for (int j = 0; j < tile_width * tile_height; j++)
                {
                    fragment_buffer[j] = Base::fragment_nodata(outtextures);
                    depth_buffer[j] = RealType(-1);
                }

                render_tile_(fragment_buffer, depth_buffer, triangle_buffer, triangle_count, viewport_buffer, uniforms_buffer, intextures);
                Base::sync_outtextures(outtextures, viewport_buffer, fragment_buffer, uniforms_buffer);
            }
        }
    }

    template <typename Uniforms, typename InTextures, typename OutTextures>
    void RenderTiledFragBuff2(const BoundingBox<IntType> &viewport,
                              const BufferViewHLS<RealType> &vertex_buffer,
                              const BufferViewHLS<IntType> &ebo_buffer,
                              const Uniforms &uniforms,
                              const InTextures &intextures,
                              OutTextures &outtextures)
    {
        IntType num_tiles_x = IntType(ceil(RealType(viewport.width_) / tile_width));
        IntType num_tiles_y = IntType(ceil(RealType(viewport.height_) / tile_height));
        // IntType num_tiles = num_tiles_x * num_tiles_y;

        Triangle triangle_buffer[max_tri_per_tile];
        Fragment fragment_buffer[tile_width * tile_height];
        RealType depth_buffer[tile_width * tile_height];

#pragma HLS BIND_STORAGE variable = triangle_buffer type = ram_t2p impl = bram
#pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = uram

#pragma HLS aggregate variable = fragment_buffer compact = bit
#pragma HLS BIND_STORAGE variable = fragment_buffer type = ram_t2p impl = uram

    renderbase_render_tiles_loop:
        for (int tile_y = 0; tile_y < num_tiles_y; tile_y++)
        {
#pragma HLS loop_tripcount min = max_tiles_y max = max_tiles_y avg = max_tiles_y

        renderbase_render_tiles_x_loop:
            for (int tile_x = 0; tile_x < num_tiles_x; tile_x++)
            {
#pragma HLS loop_tripcount min = max_tiles_x max = max_tiles_x avg = max_tiles_x

#pragma HLS dependence variable = fragment_buffer type = inter false
#pragma HLS dependence variable = fragment_buffer type = intra false

#pragma HLS dependence variable = depth_buffer type = inter false
#pragma HLS dependence variable = depth_buffer type = intra false

#pragma HLS dependence variable = triangle_buffer type = inter false
#pragma HLS dependence variable = triangle_buffer type = intra false

#pragma HLS dependence variable = intextures type = inter false
#pragma HLS dependence variable = intextures type = intra false

#pragma HLS dependence variable = outtextures type = inter false
#pragma HLS dependence variable = outtextures type = intra false

            renderbase_reset_depth_buffer_loop:
                for (int j = 0; j < tile_width * tile_height; j++)
                {
                    fragment_buffer[j] = Base::fragment_nodata(outtextures);
                    depth_buffer[j] = RealType(-1);
                }

                IntType min_x_ = IntType(RealType(viewport.width_ * tile_x) / RealType(num_tiles_x)) + viewport.min_x_;
                IntType max_x_ = IntType(RealType(viewport.width_ * (tile_x + 1)) / RealType(num_tiles_x)) + viewport.min_x_;
                IntType min_y_ = IntType(RealType(viewport.height_ * tile_y) / RealType(num_tiles_y)) + viewport.min_y_;
                IntType max_y_ = IntType(RealType(viewport.height_ * (tile_y + 1)) / RealType(num_tiles_y)) + viewport.min_y_;

                BoundingBox<IntType> viewport_buffer = BoundingBox<IntType>(min_x_, max_x_, min_y_, max_y_);
                IntType triangle_count = 0;

                this->get_triangles_(vertex_buffer, ebo_buffer, viewport, viewport_buffer, uniforms, triangle_buffer, max_tri_per_tile, triangle_count);

                render_tile_(fragment_buffer, depth_buffer, triangle_buffer, triangle_count, viewport_buffer, uniforms, intextures);
                Base::sync_outtextures(outtextures, viewport_buffer, fragment_buffer, uniforms);
            }
        }
    }

    template <typename Mesh, typename Uniforms, typename InTextures, typename OutTextures>
    void RenderTiledFragBuffInChannels(const BoundingBox<IntType> &viewport,
                                       const Mesh &mesh,
                                       const Uniforms &uniforms,
                                       const InTextures &intextures_ch1,
                                       const InTextures &intextures_ch2,
                                       const InTextures &intextures_ch3,
                                       const InTextures &intextures_ch4,
                                       OutTextures &outtextures)
    {
        // typename Base::VertexData vertex_data[max_num_tri * 3];
        // #pragma HLS BIND_STORAGE variable = vertex_data type = ram_t2p impl = uram

        // this->get_vertex_data(mesh, vertex_data);

        Triangle triangles[max_num_tri];
        // Pack the aggregate (newer pragma)
// #pragma HLS aggregate variable = triangles compact = bit
//  or, in some versions:
//  #pragma HLS data_pack variable=triangles
#pragma HLS BIND_STORAGE variable = triangles type = ram_t2p impl = uram

        IntType num_triangles;
        this->get_triangles_(mesh, viewport, uniforms, triangles, IntType(max_num_tri), num_triangles);

        IntType num_tiles_x = IntType(ceil(RealType(viewport.width_) / tile_width));
        IntType num_tiles_y = IntType(ceil(RealType(viewport.height_) / tile_height));
        IntType num_tiles = num_tiles_x * num_tiles_y;

        IntType triangle_count[num_buffers];
        Uniforms uniforms_buffer[num_buffers];
        BoundingBox<IntType> viewport_buffer[num_buffers];
        Triangle triangle_buffer[num_buffers][max_tri_per_tile];
        Fragment fragment_buffer[num_buffers][tile_width * tile_height];
        RealType depth_buffer[num_buffers][tile_width * tile_height];

#pragma HLS array_partition variable = triangle_count complete dim = 1

        // Pack the aggregate (newer pragma)
// #pragma HLS aggregate variable = uniforms_buffer compact = bit
//  or, in some versions:
//  #pragma HLS data_pack variable=uniforms_buffer
#pragma HLS array_partition variable = uniforms_buffer complete dim = 1

        // Pack the aggregate (newer pragma)
// #pragma HLS aggregate variable = viewport_buffer compact = bit
//  or, in some versions:
//  #pragma HLS data_pack variable=viewport_buffer
#pragma HLS array_partition variable = viewport_buffer complete dim = 1

        // Pack the aggregate (newer pragma)
// #pragma HLS aggregate variable = triangle_buffer compact = bit
//  or, in some versions:
//  #pragma HLS data_pack variable=triangle_buffer
#pragma HLS BIND_STORAGE variable = triangle_buffer type = ram_t2p impl = bram
#pragma HLS array_partition variable = triangle_buffer complete dim = 1

#pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = uram
#pragma HLS array_partition variable = depth_buffer complete dim = 1
        //   #pragma HLS array_partition variable = depth_buffer cyclic factor = 2 dim = 2

        // Pack the aggregate (newer pragma)
#pragma HLS aggregate variable = fragment_buffer compact = bit
        // or, in some versions:
        // #pragma HLS data_pack variable=fragment_buffer
#pragma HLS BIND_STORAGE variable = fragment_buffer type = ram_t2p impl = uram
#pragma HLS array_partition variable = fragment_buffer complete dim = 1
        //   #pragma HLS array_partition variable = fragment_buffer cyclic factor = 2 dim = 2

    renderbase_render_tiles_loop:
        for (int tile_y = 0; tile_y < num_tiles_y; tile_y++)
        {
#pragma HLS loop_tripcount min = max_tiles_y max = max_tiles_y avg = max_tiles_y

        renderbase_render_tiles_x_loop:
            for (int tile_x = 0; tile_x < num_tiles_x; tile_x += num_buffers)
            {
                int tile = tile_y * num_tiles_x + tile_x;
#pragma HLS loop_tripcount min = max_tiles_x / num_buffers max = max_tiles_x / num_buffers avg = max_tiles_x / num_buffers

#pragma HLS dependence variable = fragment_buffer type = inter false
#pragma HLS dependence variable = fragment_buffer type = intra false

#pragma HLS dependence variable = depth_buffer type = inter false
#pragma HLS dependence variable = depth_buffer type = intra false

#pragma HLS dependence variable = triangle_buffer type = inter false
#pragma HLS dependence variable = triangle_buffer type = intra false

#pragma HLS dependence variable = viewport_buffer type = inter false
#pragma HLS dependence variable = viewport_buffer type = intra false

#pragma HLS dependence variable = uniforms_buffer type = inter false
#pragma HLS dependence variable = uniforms_buffer type = intra false

#pragma HLS dependence variable = intextures_ch1 type = inter false
#pragma HLS dependence variable = intextures_ch1 type = intra false

#pragma HLS dependence variable = intextures_ch2 type = inter false
#pragma HLS dependence variable = intextures_ch2 type = intra false

#pragma HLS dependence variable = intextures_ch3 type = inter false
#pragma HLS dependence variable = intextures_ch3 type = intra false

#pragma HLS dependence variable = intextures_ch4 type = inter false
#pragma HLS dependence variable = intextures_ch4 type = intra false

            renderbasehls_reset:
                for (int j = 0; j < num_buffers; j++)
                {
                    // viewport_buffer[j] = viewport_tiles[tile + j];
                    IntType ttile_x = tile_x + j;
                    IntType min_x_ = IntType(RealType(viewport.width_ * ttile_x) / RealType(num_tiles_x)) + viewport.min_x_;
                    IntType max_x_ = IntType(RealType(viewport.width_ * (ttile_x + 1)) / RealType(num_tiles_x)) + viewport.min_x_;
                    IntType min_y_ = IntType(RealType(viewport.height_ * tile_y) / RealType(num_tiles_y)) + viewport.min_y_;
                    IntType max_y_ = IntType(RealType(viewport.height_ * (tile_y + 1)) / RealType(num_tiles_y)) + viewport.min_y_;

                    viewport_buffer[j] = BoundingBox<IntType>(min_x_, max_x_, min_y_, max_y_);
                    triangle_count[j] = 0;
                    uniforms_buffer[j] = uniforms;
                }

            renderbase_triangle_buffer_loop:
                for (int j = 0; j < num_triangles; j++)
                {
#pragma HLS loop_tripcount min = 768 max = 768 avg = 768

                    // typename RendererBase<MathType, Base>::Triangle triangle;
                    // create_triangle_(vertexdata, vertexids, viewport, uniforms, triangle);
                    // this->get_triangles(mesh, viewport, uniforms, triangles, num_triangles);

                    Triangle triangle = triangles[j];
                    BoundingBox<RealType> tri_bb(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen);

                renderbase_triangle_tile_loop:
                    for (int i = 0; i < num_buffers; i++)
                    {
                        // IntType min_x = max(viewport_buffer[i].min_x_, static_cast<IntType>(floor(tri_bb.min_x_)));
                        // IntType max_x = min(viewport_buffer[i].max_x_, static_cast<IntType>(ceil(tri_bb.max_x_)));
                        // IntType min_y = max(viewport_buffer[i].min_y_, static_cast<IntType>(floor(tri_bb.min_y_)));
                        // IntType max_y = min(viewport_buffer[i].max_y_, static_cast<IntType>(ceil(tri_bb.max_y_)));

                        IntType min_x = max(viewport_buffer[i].min_x_, static_cast<IntType>(tri_bb.min_x_));
                        IntType max_x = min(viewport_buffer[i].max_x_, static_cast<IntType>(tri_bb.max_x_ + 1));
                        IntType min_y = max(viewport_buffer[i].min_y_, static_cast<IntType>(tri_bb.min_y_));
                        IntType max_y = min(viewport_buffer[i].max_y_, static_cast<IntType>(tri_bb.max_y_ + 1));

                        if (min_x >= max_x || min_y >= max_y)
                            continue;

                        triangle_buffer[i][triangle_count[i]] = triangle;
                        triangle_count[i] = min(IntType(triangle_count[i] + 1), IntType(max_tri_per_tile - 1));
                    }
                }

            renderbase_reset_depth_buffer_loop:
                for (int j = 0; j < tile_width * tile_height; j++)
                {
                // #pragma HLS pipeline II = 1
                renderbase_reset_depth_buffer_i_loop:
                    for (int i = 0; i < num_buffers; i++)
                    {
                        // #pragma HLS unroll
                        fragment_buffer[i][j] = Base::fragment_nodata(outtextures);
                        depth_buffer[i][j] = RealType(-1);
                    }
                }

            renderbase_render_tile_loop:
                for (int i = 0; i < num_buffers; i++)
                {
#pragma HLS unroll
                    if (i == 0)
                        render_tile_(fragment_buffer[i], depth_buffer[i], triangle_buffer[i], triangle_count[i], viewport_buffer[i], uniforms_buffer[i], intextures_ch1);
                    if (i == 1)
                        render_tile_(fragment_buffer[i], depth_buffer[i], triangle_buffer[i], triangle_count[i], viewport_buffer[i], uniforms_buffer[i], intextures_ch2);
                    if (i == 2)
                        render_tile_(fragment_buffer[i], depth_buffer[i], triangle_buffer[i], triangle_count[i], viewport_buffer[i], uniforms_buffer[i], intextures_ch3);
                    if (i == 3)
                        render_tile_(fragment_buffer[i], depth_buffer[i], triangle_buffer[i], triangle_count[i], viewport_buffer[i], uniforms_buffer[i], intextures_ch4);
                }

            depthrendererhls_loop:
                for (int i = 0; i < num_buffers; i++)
                {
                    Base::sync_outtextures(outtextures, viewport_buffer[i], fragment_buffer[i], uniforms_buffer[i]);
                }
            }
        }
    }

    template <typename Mesh, typename Uniforms, typename InTextures, typename OutTextures>
    void RenderTiledFragBuffInChannels2(const BoundingBox<IntType> &viewport,
                                        const Mesh &mesh,
                                        const Uniforms &uniforms,
                                        const InTextures &intextures_ch1,
                                        const InTextures &intextures_ch2,
                                        const InTextures &intextures_ch3,
                                        const InTextures &intextures_ch4,
                                        OutTextures &outtextures)
    {
        Triangle triangles[max_num_tri];
        // Pack the aggregate (newer pragma)
// #pragma HLS aggregate variable = triangles compact = bit
//  or, in some versions:
//  #pragma HLS data_pack variable=triangles
#pragma HLS BIND_STORAGE variable = triangles type = ram_t2p impl = uram

        IntType num_triangles;
        this->get_triangles_(mesh, viewport, uniforms, triangles, IntType(max_num_tri), num_triangles);

        IntType num_tiles_x = IntType(ceil(RealType(viewport.width_) / tile_width));
        IntType num_tiles_y = IntType(ceil(RealType(viewport.height_) / tile_height));
        IntType num_tiles = num_tiles_x * num_tiles_y;

        Uniforms uniforms_buffer[num_buffers];
        BoundingBox<IntType> viewport_buffer[num_buffers];
        Fragment fragment_buffer[num_buffers][tile_width * tile_height];
        RealType depth_buffer[num_buffers][tile_width * tile_height];

        // Pack the aggregate (newer pragma)
// #pragma HLS aggregate variable = uniforms_buffer compact = bit
//  or, in some versions:
//  #pragma HLS data_pack variable=uniforms_buffer
#pragma HLS array_partition variable = uniforms_buffer complete dim = 1

        // Pack the aggregate (newer pragma)
// #pragma HLS aggregate variable = viewport_buffer compact = bit
//  or, in some versions:
//  #pragma HLS data_pack variable=viewport_buffer
#pragma HLS array_partition variable = viewport_buffer complete dim = 1

#pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = uram
#pragma HLS array_partition variable = depth_buffer complete dim = 1
        //   #pragma HLS array_partition variable = depth_buffer cyclic factor = 2 dim = 2

        // Pack the aggregate (newer pragma)
#pragma HLS aggregate variable = fragment_buffer compact = bit
        // or, in some versions:
        // #pragma HLS data_pack variable=fragment_buffer
#pragma HLS BIND_STORAGE variable = fragment_buffer type = ram_t2p impl = uram
#pragma HLS array_partition variable = fragment_buffer complete dim = 1
        //   #pragma HLS array_partition variable = fragment_buffer cyclic factor = 2 dim = 2

    renderbase_render_tiles_loop:
        for (int tile_y = 0; tile_y < num_tiles_y; tile_y++)
        {
#pragma HLS loop_tripcount min = max_tiles_y max = max_tiles_y avg = max_tiles_y

        renderbase_render_tiles_x_loop:
            for (int tile_x = 0; tile_x < num_tiles_x; tile_x += num_buffers)
            {
                int tile = tile_y * num_tiles_y + tile_x;
#pragma HLS loop_tripcount min = max_tiles_x / num_buffers max = max_tiles_x / num_buffers avg = max_tiles_x / num_buffers

#pragma HLS dependence variable = fragment_buffer type = inter false
#pragma HLS dependence variable = fragment_buffer type = intra false

#pragma HLS dependence variable = depth_buffer type = inter false
#pragma HLS dependence variable = depth_buffer type = intra false

#pragma HLS dependence variable = viewport_buffer type = inter false
#pragma HLS dependence variable = viewport_buffer type = intra false

#pragma HLS dependence variable = uniforms_buffer type = inter false
#pragma HLS dependence variable = uniforms_buffer type = intra false

                // #pragma HLS dataflow

            renderbase_reset_depth_buffer_loop:
                for (int j = 0; j < tile_width * tile_height; j++)
                {
                // #pragma HLS pipeline II = 1
                renderbase_reset_depth_buffer_i_loop:
                    for (int i = 0; i < num_buffers; i++)
                    {
                        // #pragma HLS unroll
                        fragment_buffer[i][j] = Base::fragment_nodata(outtextures);
                        depth_buffer[i][j] = RealType(-1);
                    }
                }

            renderbasehls_reset:
                for (int j = 0; j < num_buffers; j++)
                {
                    // viewport_buffer[j] = viewport_tiles[tile + j];
                    IntType ttile_x = tile_x + j;
                    IntType min_x_ = IntType(RealType(viewport.width_ * ttile_x) / RealType(num_tiles_x)) + viewport.min_x_;
                    IntType max_x_ = IntType(RealType(viewport.width_ * (ttile_x + 1)) / RealType(num_tiles_x)) + viewport.min_x_;
                    IntType min_y_ = IntType(RealType(viewport.height_ * tile_y) / RealType(num_tiles_y)) + viewport.min_y_;
                    IntType max_y_ = IntType(RealType(viewport.height_ * (tile_y + 1)) / RealType(num_tiles_y)) + viewport.min_y_;

                    viewport_buffer[j] = BoundingBox<IntType>(min_x_, max_x_, min_y_, max_y_);
                    uniforms_buffer[j] = uniforms;
                }

                hls::stream<Triangle> triangles_streams[num_buffers];

            renderbase_triangle_buffer_loop:
                for (int j = 0; j < num_triangles; j++)
                {
                    for (int i = 0; i < num_buffers; i++)
                        triangles_streams[i].write(triangles[j]);
                }

            renderbase_triangle_tile_loop:
                for (int i = 0; i < num_buffers; i++)
                {
#pragma HLS unroll
                    if (i == 0)
                        draw_triangle_stream_(triangles_streams[i], viewport_buffer[i], depth_buffer[i], uniforms_buffer[i], intextures_ch1, fragment_buffer[i]);
                    if (i == 1)
                        draw_triangle_stream_(triangles_streams[i], viewport_buffer[i], depth_buffer[i], uniforms_buffer[i], intextures_ch2, fragment_buffer[i]);
                    if (i == 2)
                        draw_triangle_stream_(triangles_streams[i], viewport_buffer[i], depth_buffer[i], uniforms_buffer[i], intextures_ch3, fragment_buffer[i]);
                    if (i == 3)
                        draw_triangle_stream_(triangles_streams[i], viewport_buffer[i], depth_buffer[i], uniforms_buffer[i], intextures_ch4, fragment_buffer[i]);
                }

            depthrendererhls_loop:
                for (int i = 0; i < num_buffers; i++)
                {
                    Base::sync_outtextures(outtextures, viewport_buffer[i], fragment_buffer[i], uniforms_buffer[i]);
                }
            }
        }
    }

private:
    template <typename Uniforms, typename InTextures, typename Fragment>
    void draw_triangle_stream_(hls::stream<Triangle> &triangles_stream, const BoundingBox<IntType> &viewport, RealType *depth_buffer, const Uniforms &uniforms, const InTextures &intextures, Fragment *fragment_buffer)
    {
        Triangle triangle = triangles_stream.read();
        this->draw_triangle_(triangle, viewport, depth_buffer, uniforms, intextures, fragment_buffer);
    }

    template <typename Fragment, typename Uniforms, typename InTextures>
    void render_tile_(Fragment fragment_buffer[], RealType depth_buffer[], const Triangle triangles[], int num_triangles, const BoundingBox<IntType> &viewport_tile, const Uniforms &uniforms, const InTextures &intextures)
    {
#pragma HLS INLINE

    render_tile_loop:
        for (IntType tri = 0; tri < num_triangles; tri++)
        {
// #pragma HLS pipeline off
#pragma HLS loop_tripcount min = max_tri_per_tile max = max_tri_per_tile avg = max_tri_per_tile

            Triangle triangle = triangles[tri];
            this->draw_triangle_(triangle, viewport_tile, depth_buffer, uniforms, intextures, fragment_buffer);
        }
    }
};

class DepthRendererHLS
    : public RendererBaseHLS<DepthRendererHLS, DepthRendererBase<TextureViewHLS>>
{
public:
    using Base = DepthRendererBase<TextureViewHLS>;

    // DepthRendererHLS() = default;
    //~DepthRendererHLS() = default;

    void Render(const BufferViewHLS<RealType> &vertex_buffer,
                const BufferViewHLS<IntType> &ebo_buffer,
                const linalg::SE3<RealType> &pose,
                const PinholeCamera<RealType> &cam,
                TextureViewHLS<float> &out_texture)
    {
        Base::Uniforms uniforms;

        linalg::Mat4<RealType> opencv2opengl = linalg::Mat4<RealType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        // depthrenderer_opencv2opengl_loop:
        //     for (int i = 0; i < 4; i++)
        //     {
        //         uniforms.view_matrix(1, i) = -uniforms.view_matrix(1, i);
        //         uniforms.view_matrix(2, i) = -uniforms.view_matrix(2, i);
        //    }

        const int W = static_cast<int>(out_texture.width());
        const int H = static_cast<int>(out_texture.height());
        BoundingBox<IntType> viewport(0, W, 0, H);

        Base::InTextures intextures_ch1{0};
        Base::InTextures intextures_ch2{0};
        Base::InTextures intextures_ch3{0};
        Base::InTextures intextures_ch4{0};
        Base::OutTextures outtextures{out_texture};

        // RendererBaseHLS<DepthRendererHLS, Base>::RenderNaive(viewport, vertex_buffer, ebo_buffer, uniforms, intextures_ch1, outtextures);
        RendererBaseHLS<DepthRendererHLS, Base>::RenderTiledFragBuff2(viewport, vertex_buffer, ebo_buffer, uniforms, intextures_ch1, outtextures);
        // RendererBaseHLS<DepthRendererHLS, Base>::RenderTiledFragBuffInChannels(viewport, mesh, uniforms, intextures_ch1, intextures_ch2, intextures_ch3, intextures_ch4, outtextures);
    }
};

/*
class DepthRendererRAM
    : public DepthRendererBase<MathType, DepthType, MeshHLS, TextureRAM>
{
public:
    DepthRendererRAM() = default;
    ~DepthRendererRAM() = default;

    void Render(const MeshHLS &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int out_lvl,
                TextureRAM<float> &out_texture)
    {
        DepthRendererBase::Render(mesh, pose, cam, out_lvl, out_texture);
    }

    void sync_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb, const Fragment *fragment_buffer)
    {
        // #pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (int iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = tile_height max = tile_height avg = tile_height

        depthrendererbase_sync_outtexture_x_loop:
            for (int ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = tile_width max = tile_width avg = tile_width

                int x = ix + tex_bb.min_x_;
                int y = iy + tex_bb.min_y_;
                int address = iy * tex_bb.width_ + ix;

                MathType depth = fragment_buffer[address].depth;
                textures.out_texture.set_texel_(depth, y, x, out_lvl_);
            }
        }
    }

private:
};

*/
/*
class DepthRendererRAM2
    : public DepthRendererBase<MathType, DepthType, MeshHLS, TextureRAM>
{
public:
    DepthRendererRAM2() = default;
    ~DepthRendererRAM2() = default;

    void Render(const MeshHLS &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int out_lvl,
                TextureRAM<float> &out_texture_ch1,
                TextureRAM<float> &out_texture_ch2)
    {

        const unsigned int crop_W = 320;
        const unsigned int crop_H = 240;

        out_lvl_ = out_lvl;

        unsigned int W = out_texture_ch1.width(out_lvl);
        unsigned int H = out_texture_ch1.height(out_lvl);

        unsigned int x_size = W / crop_W;
        unsigned int y_size = H / crop_H;

        MathType scale_W = MathType(W) / MathType(crop_W);
        MathType scale_H = MathType(H) / MathType(crop_H);

        BoundingBox<int> viewport(0, crop_W, 0, crop_H);
        TextureRAM<DepthType> out_texture_part(out_texture.nodata());
        Textures textures{out_texture_part};

    depth_renderer_loop_y:
        for (int py = 0; py < y_size; py++)
        {
#pragma HLS loop_tripcount min = 15 max = 15 avg = 15

        depth_renderer_loop_x:
            for (int px = 0; px < x_size; px++)
            {
#pragma HLS loop_tripcount min = 20 max = 20 avg = 20

                unsigned int start_W = px * crop_W;
                // Int Wf = (px + 1) * Wn;
                unsigned int start_H = py * crop_H;
                // Int Hf = (py + 1) * Hn;

                linalg::Vec4<MathType> cam_params = cam.GetParams();
                cam_params(0) = cam_params(0) * MathType(W) / MathType(crop_W);
                cam_params(1) = cam_params(1) * MathType(H) / MathType(crop_H);
                cam_params(2) = (cam_params(2) * MathType(W) - MathType(start_W)) / MathType(crop_W);
                cam_params(3) = (cam_params(3) * scale_H - MathType(y_size - 1 - py));
                Camera<MathType> new_cam;
                new_cam.SetParams(cam_params);

                t_matrix_ = new_cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_ * pose.matrix();

                out_texture_part.fill(out_lvl, out_texture_part.nodata());
            }
        }

        RendererBase::RenderSingle(viewport_1, mesh, textures);
        RendererBase::RenderSingle(viewport_2, mesh, textures);

        void Render(const Mesh &mesh,
                    const linalg::SE3<MathType> &pose,
                    const Camera<MathType> &cam,
                    unsigned int out_lvl,
                    Texture<DepthType> &out_texture)
    }

private:
    DepthRendererBase<MathType, DepthType, MeshHLS, TextureRAM> renderer1;
    DepthRendererBase<MathType, DepthType, MeshHLS, TextureRAM> renderer2;
};
*/

class ImageRendererHLS
    : public RendererBaseHLS<ImageRendererHLS, ImageRendererBase<TextureViewHLS>>
{
public:
    using Base = ImageRendererBase<TextureViewHLS>;

    ImageRendererHLS() = default;
    ~ImageRendererHLS() = default;

    void Render(const BufferViewHLS<RealType> &vertex_buffer,
                const BufferViewHLS<IntType> &ebo_buffer,
                const linalg::SE3<RealType> &pose,
                const linalg::Vec2<RealType> &exposure,
                const PinholeCamera<RealType> &cam,
                TextureViewHLS<ImageType> &diffuse_texture_ch1,
                TextureViewHLS<ImageType> &diffuse_texture_ch2,
                TextureViewHLS<ImageType> &diffuse_texture_ch3,
                TextureViewHLS<ImageType> &diffuse_texture_ch4,
                TextureViewHLS<ImageType> &out_texture)
    {
        Base::Uniforms uniforms;

        linalg::Mat4<RealType> opencv2opengl = linalg::Mat4<RealType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        // imagerenderer_opencv2opengl_loop:
        //     for (int i = 0; i < 4; i++)
        //     {
        //         uniforms.view_matrix(1, i) = -uniforms.view_matrix(1, i);
        //         uniforms.view_matrix(2, i) = -uniforms.view_matrix(2, i);
        //     }
        uniforms.camera = cam;
        uniforms.exposure = exposure;

        const int W = static_cast<int>(out_texture.width());
        const int H = static_cast<int>(out_texture.height());
        BoundingBox<IntType> viewport(0, W, 0, H);

        Base::InTextures intextures_ch1{diffuse_texture_ch1};
        Base::InTextures intextures_ch2{diffuse_texture_ch2};
        Base::InTextures intextures_ch3{diffuse_texture_ch3};
        Base::InTextures intextures_ch4{diffuse_texture_ch4};

        Base::OutTextures outtextures{out_texture};

        // RendererBaseHLS<ImageRendererHLS, Base>::RenderNaive(
        //     viewport, vertex_buffer, ebo_buffer, uniforms, intextures_ch1, outtextures);

        RendererBaseHLS<ImageRendererHLS, Base>::RenderTiledFragBuff2(
            viewport, vertex_buffer, ebo_buffer, uniforms, intextures_ch1, outtextures);
        /*
        RendererBaseHLS<ImageRendererHLS, Base>::RenderTiledFragBuffInChannels(
            viewport,
            mesh,
            uniforms,
            intextures_ch1,
            intextures_ch2,
            intextures_ch3,
            intextures_ch4,
            outtextures);
        */
    }

private:
};

class JPoseExpMapRendererHLS
    : public RendererBaseHLS<JPoseExpMapRendererHLS, JPoseExpMapRendererBase<TextureViewHLS>>
{
public:
    using Base = JPoseExpMapRendererBase<TextureViewHLS>;

    JPoseExpMapRendererHLS() = default;
    ~JPoseExpMapRendererHLS() = default;

    void Render(const BufferViewHLS<RealType> &vertex_buffer,
                const BufferViewHLS<IntType> &ebo_buffer,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                TextureViewHLS<ImageType> &kf_texture,
                TextureViewHLS<ImageType> &f_texture,
                TextureViewHLS<Vec3<float>> &dfdxy_texture,
                TextureViewHLS<Vec3<float>> &jtra_texture,
                TextureViewHLS<Vec3<float>> &jrot_texture,
                TextureViewHLS<Vec3<float>> &jexp_texture,
                TextureViewHLS<Vec3<float>> &jmap_texture,
                TextureViewHLS<Vec3<PidType>> &pids_texture,
                TextureViewHLS<float> &r_texture)
    {
        Mat4<float> opencv2opengl = Mat4<float>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        const int W = static_cast<int>(r_texture.width());
        const int H = static_cast<int>(r_texture.height());
        BoundingBox<int> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.camera = cam;
        uniforms.exposure = exposure;
        uniforms.out_width = W;
        uniforms.out_height = H;

        Base::InTextures intextures{kf_texture, f_texture, dfdxy_texture};
        Base::OutTextures outtextures{jtra_texture, jrot_texture, jexp_texture, jmap_texture, pids_texture, r_texture};

        // RendererBaseHLS<JPoseExpMapRendererHLS, Base>::RenderNaive(
        //     viewport, vertex_buffer, ebo_buffer, uniforms, intextures, outtextures);

        RendererBaseHLS<JPoseExpMapRendererHLS, Base>::RenderTiledFragBuff2(
            viewport, vertex_buffer, ebo_buffer, uniforms, intextures, outtextures);
    }

private:
};

/*
class ImageRendererBRAM
    : public ImageRendererBase<MathType, ImageType, MeshHLS<MeshType, BufferRAM, TextureRAM>, TextureRAM, TextureBRAM, TextureBRAM>
{
public:
    ImageRendererBRAM() = default;
    ~ImageRendererBRAM() = default;

    void Render(MeshHLS<MeshType, BufferRAM, TextureRAM> &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                unsigned int in_lvl,
                unsigned int out_lvl,
                TextureRAM<MathType> &depth_texture,
                TextureRAM<ImageType> &out_texture)
    {
        TextureBRAM<MathType> depth_texture_internal(depth_texture.width(0), depth_texture.height(0), depth_texture.nodata());

    imagerendererbram_incopy_y_loop:
        for (int y = 0; y < depth_texture.height(out_lvl); y++)
        {
#pragma HLS loop_tripcount min = 480 max = 480 avg = 480

        imagerendererbram_incopy_x_loop:
            for (int x = 0; x < depth_texture.width(out_lvl); x++)
            {
#pragma HLS loop_tripcount min = 640 max = 640 avg = 640

                MathType data = depth_texture.texel_(y, x, out_lvl);
                depth_texture_internal.set_texel_(data, y, x, out_lvl);
            }
        }

        ImageRendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, depth_texture_internal, out_texture);

    imagerendererbram_outcopy_y_loop:
        for (int y = 0; y < depth_texture.height(out_lvl); y++)
        {
#pragma HLS loop_tripcount min = 480 max = 480 avg = 480

        imagerendererbram_outcopy_x_loop:
            for (int x = 0; x < depth_texture.width(out_lvl); x++)
            {
#pragma HLS loop_tripcount min = 640 max = 640 avg = 640

                MathType data = depth_texture_internal.texel_(y, x, out_lvl);
                depth_texture.set_texel_(data, y, x, out_lvl);
            }
        }
    }

private:
};
*/
/*
class ImageRendererBRAM2
    : public ImageRendererBase<MathType, ImageType, MeshHLS<MeshType, BufferRAM, TextureRAMCached2>, TextureRAMCached2, TextureRAMCached2, TextureRAMCached2>
{
public:
    ImageRendererBRAM2() = default;
    ~ImageRendererBRAM2() = default;

    void Render(MeshHLS<MeshType, BufferRAM, TextureRAMCached2> &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                unsigned int in_lvl,
                unsigned int out_lvl,
                TextureRAMCached2<ImageType> &depth_texture,
                TextureRAMCached2<ImageType> &out_texture)
    {
        const unsigned int crop_W = 32;
        const unsigned int crop_H = 32;

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;

        unsigned int in_W = mesh.diffuse_.width(out_lvl);
        unsigned int in_H = mesh.diffuse_.height(out_lvl);

        unsigned int out_W = out_texture.width(out_lvl);
        unsigned int out_H = out_texture.height(out_lvl);

        unsigned int x_size = out_W / crop_W;
        unsigned int y_size = out_H / crop_H;

        MathType scale_W = MathType(out_W) / MathType(crop_W);
        MathType scale_H = MathType(out_H) / MathType(crop_H);

        BoundingBox<int> viewport(0, crop_W, 0, crop_H);

        Textures textures{depth_texture, mesh.diffuse_, out_texture};

    depth_renderer_loop_y:
        for (int py = 0; py < y_size; py++)
        {
#pragma HLS loop_tripcount min = 15 max = 15 avg = 15

        depth_renderer_loop_x:
            for (int px = 0; px < x_size; px++)
            {
#pragma HLS loop_tripcount min = 20 max = 20 avg = 20

                unsigned int start_W = px * crop_W;
                // Int Wf = (px + 1) * Wn;
                unsigned int start_H = py * crop_H;
                // Int Hf = (py + 1) * Hn;

                //depth_texture.set_cache_addr_(start_H, start_W, out_lvl);
                //mesh.diffuse_.set_cache_addr_(start_H, start_W, out_lvl);
                //out_texture.set_cache_addr_(start_H, start_W, out_lvl);

                linalg::Vec4<MathType> cam_params = cam.GetParams();
                cam_params(0) = cam_params(0) * MathType(out_W) / MathType(crop_W);
                cam_params(1) = cam_params(1) * MathType(out_H) / MathType(crop_H);
                cam_params(2) = (cam_params(2) * MathType(out_W) - MathType(start_W)) / MathType(crop_W);
                cam_params(3) = (cam_params(3) * scale_H - MathType(y_size - 1 - py));
                Camera<MathType> new_cam;
                new_cam.SetParams(cam_params);

                t_matrix_ = new_cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                            this->opencv2opengl_ *
                            pose.matrix();

                // depth_texture.fill_cache(depth_texture.nodata());
                //out_texture.fill_cache(out_texture.nodata());
                //mesh.diffuse_.cache_read_();

                RendererBase::Render(viewport, mesh, textures);

                // depth_texture.cache_write_();
                //out_texture.cache_write_();
            }
        }
    }

private:
};
*/
