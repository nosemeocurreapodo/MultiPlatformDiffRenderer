#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
// #include "core/types.h"
#include "backends/xrt/hls/math_common.h"
#include "linalg/linalg.h"
#include "core/camera.h"
#include "core/boundingbox.h"
#include "core/render_constants.h"
#include "backends/base/rendererbase.h"
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
                     const BufferViewReadHLS<float> &vertex_buffer,
                     const BufferViewReadHLS<int> &ebo_buffer,
                     const Uniforms &uniforms,
                     const InTextures &intextures,
                     OutTextures &outtextures)
    {
        // MathType depth_buffer[viewport.width_ * viewport.height_] = {MathType(-1)};
        Fragment fragment_buffer[MAX_WIDTH * MAX_HEIGHT];
        RealType depth_buffer[MAX_WIDTH * MAX_HEIGHT];
#pragma HLS BIND_STORAGE variable = fragment_buffer type = ram_t2p impl = uram
#pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = uram

        for (int y = viewport.min_y_; y < viewport.max_y_; y++)
        {
            int base_address = y * viewport.width_;
            for (int x = viewport.min_x_; x < viewport.max_x_; x++)
            {
                int address = base_address + x;
                fragment_buffer[address] = Base::fragment_nodata(outtextures);
                depth_buffer[address] = RealType(-1);
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
                             const BufferViewReadHLS<float> &vertex_buffer,
                             const BufferViewReadHLS<int> &ebo_buffer,
                             const Uniforms &uniforms,
                             const InTextures &intextures,
                             OutTextures &outtextures)
    {
        Triangle triangles[MAX_NUM_TRI];
#pragma HLS BIND_STORAGE variable = triangles type = ram_t2p impl = uram

        int num_triangles;
        this->get_triangles_(vertex_buffer, ebo_buffer, viewport, viewport, uniforms, triangles, MAX_NUM_TRI, num_triangles);

        IntType tile_width = IntType(ceil(RealType(viewport.width_) / NUM_TILES_X));
        IntType tile_height = IntType(ceil(RealType(viewport.height_) / NUM_TILES_Y));

        IntType triangle_count;
        Uniforms uniforms_buffer;
        BoundingBox<IntType> viewport_buffer;
        Triangle triangle_buffer[MAX_TRI_PER_TILE];
        Fragment fragment_buffer[MAX_TILE_WIDTH * MAX_TILE_HEIGHT];
        RealType depth_buffer[MAX_TILE_WIDTH * MAX_TILE_HEIGHT];

#pragma HLS BIND_STORAGE variable = triangle_buffer type = ram_t2p impl = bram
#pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = uram

#pragma HLS aggregate variable = fragment_buffer compact = bit
#pragma HLS BIND_STORAGE variable = fragment_buffer type = ram_t2p impl = uram

    renderbase_render_tiles_loop:
        for (int tile_y = 0; tile_y < NUM_TILES_Y; tile_y++)
        {
//#pragma HLS loop_tripcount min = max_tiles_y max = max_tiles_y avg = max_tiles_y

        renderbase_render_tiles_x_loop:
            for (int tile_x = 0; tile_x < NUM_TILES_X; tile_x++)
            {
                int tile = tile_y * NUM_TILES_X + tile_x;
//#pragma HLS loop_tripcount min = max_tiles_x max = max_tiles_x avg = max_tiles_x

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

                IntType min_x_ = IntType(RealType(viewport.width_ * tile_x) / RealType(NUM_TILES_X)) + viewport.min_x_;
                IntType max_x_ = IntType(RealType(viewport.width_ * (tile_x + 1)) / RealType(NUM_TILES_X)) + viewport.min_x_;
                IntType min_y_ = IntType(RealType(viewport.height_ * tile_y) / RealType(NUM_TILES_Y)) + viewport.min_y_;
                IntType max_y_ = IntType(RealType(viewport.height_ * (tile_y + 1)) / RealType(NUM_TILES_Y)) + viewport.min_y_;

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
                    triangle_count = min(IntType(triangle_count + 1), IntType(MAX_TRI_PER_TILE - 1));
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
                              const BufferViewReadHLS<float> &vertex_buffer,
                              const BufferViewReadHLS<int> &ebo_buffer,
                              const Uniforms &uniforms,
                              const InTextures &intextures,
                              OutTextures &outtextures)
    {
        IntType tile_width = IntType(ceil(RealType(viewport.width_) / NUM_TILES_X));
        IntType tile_height = IntType(ceil(RealType(viewport.height_) / NUM_TILES_Y));
        // IntType num_tiles = num_tiles_x * num_tiles_y;

        Triangle triangle_buffer[MAX_TRI_PER_TILE];
        Fragment fragment_buffer[MAX_TILE_WIDTH * MAX_TILE_HEIGHT];
        RealType depth_buffer[MAX_TILE_WIDTH * MAX_TILE_HEIGHT];

// #pragma HLS BIND_STORAGE variable = triangle_buffer type = ram_t2p impl = bram
// #pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = uram
// #pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = bram
//  #pragma HLS array_partition variable = depth_buffer type = cyclic factor = tile_width * 2
//  #pragma HLS array_partition variable = depth_buffer type = complete

// #pragma HLS aggregate variable = fragment_buffer compact = bit
#pragma HLS BIND_STORAGE variable = fragment_buffer type = ram_t2p impl = uram
        // #pragma HLS array_partition variable = fragment_buffer type = cyclic factor = 1 dim = 0

    renderbase_render_tiles_loop:
        for (int tile_y = 0; tile_y < NUM_TILES_Y; tile_y++)
        {
//#pragma HLS loop_tripcount min = max_tiles_y max = max_tiles_y avg = max_tiles_y

        renderbase_render_tiles_x_loop:
            for (int tile_x = 0; tile_x < NUM_TILES_X; tile_x++)
            {
//#pragma HLS loop_tripcount min = max_tiles_x max = max_tiles_x avg = max_tiles_x

                // #pragma HLS dependence variable = fragment_buffer type = inter false
                // #pragma HLS dependence variable = fragment_buffer type = intra false

                // #pragma HLS dependence variable = depth_buffer type = inter false
                // #pragma HLS dependence variable = depth_buffer type = intra false

                // #pragma HLS dependence variable = triangle_buffer type = inter false
                // #pragma HLS dependence variable = triangle_buffer type = intra false

                // #pragma HLS dependence variable = intextures type = inter false
                // #pragma HLS dependence variable = intextures type = intra false

                // #pragma HLS dependence variable = outtextures type = inter false
                // #pragma HLS dependence variable = outtextures type = intra false

            renderbase_reset_depth_buffer_loop_y:
                for (int y = 0; y < tile_height; y++)
                {
                    #pragma HLS loop_tripcount min = MAX_TILE_HEIGHT max = MAX_TILE_HEIGHT avg = MAX_TILE_HEIGHT

                    int base_address = y * tile_width;
                    for(int x = 0; x < tile_width; x++)
                    {
                                            #pragma HLS loop_tripcount min = MAX_TILE_WIDTH max = MAX_TILE_WIDTH avg = MAX_TILE_WIDTH

                        int address = x + base_address;
                        fragment_buffer[address] = Base::fragment_nodata(outtextures);
                        depth_buffer[address] = RealType(-1);
                    }
                }

                IntType min_x_ = IntType(RealType(viewport.width_ * tile_x) / RealType(NUM_TILES_X)) + viewport.min_x_;
                IntType max_x_ = IntType(RealType(viewport.width_ * (tile_x + 1)) / RealType(NUM_TILES_X)) + viewport.min_x_;
                IntType min_y_ = IntType(RealType(viewport.height_ * tile_y) / RealType(NUM_TILES_Y)) + viewport.min_y_;
                IntType max_y_ = IntType(RealType(viewport.height_ * (tile_y + 1)) / RealType(NUM_TILES_Y)) + viewport.min_y_;

                BoundingBox<IntType> viewport_buffer = BoundingBox<IntType>(min_x_, max_x_, min_y_, max_y_);
                IntType triangle_count = 0;

                this->get_triangles_(vertex_buffer, ebo_buffer, viewport, viewport_buffer, uniforms, triangle_buffer, MAX_TRI_PER_TILE, triangle_count);

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

        Triangle triangles[MAX_NUM_TRI];
        // Pack the aggregate (newer pragma)
// #pragma HLS aggregate variable = triangles compact = bit
//  or, in some versions:
//  #pragma HLS data_pack variable=triangles
#pragma HLS BIND_STORAGE variable = triangles type = ram_t2p impl = uram

        IntType num_triangles;
        this->get_triangles_(mesh, viewport, uniforms, triangles, IntType(MAX_NUM_TRI), num_triangles);

        IntType tile_width = IntType(ceil(RealType(viewport.width_) / NUM_TILES_X));
        IntType tile_height = IntType(ceil(RealType(viewport.height_) / NUM_TILES_Y));

        IntType triangle_count[NUM_BUFFERS];
        Uniforms uniforms_buffer[NUM_BUFFERS];
        BoundingBox<IntType> viewport_buffer[NUM_BUFFERS];
        Triangle triangle_buffer[NUM_BUFFERS][MAX_TRI_PER_TILE];
        Fragment fragment_buffer[NUM_BUFFERS][MAX_TILE_WIDTH * MAX_TILE_HEIGHT];
        RealType depth_buffer[NUM_BUFFERS][MAX_TILE_WIDTH * MAX_TILE_HEIGHT];

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
        for (int tile_y = 0; tile_y < NUM_TILES_Y; tile_y++)
        {
//#pragma HLS loop_tripcount min = max_tiles_y max = max_tiles_y avg = max_tiles_y

        renderbase_render_tiles_x_loop:
            for (int tile_x = 0; tile_x < NUM_TILES_X; tile_x += NUM_BUFFERS)
            {
                int tile = tile_y * NUM_TILES_X + tile_x;
#pragma HLS loop_tripcount min = NUM_TILES_X / NUM_BUFFERS max = NUM_TILES_X / NUM_BUFFERS avg = NUM_TILES_X / NUM_BUFFERS

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
                for (int j = 0; j < NUM_BUFFERS; j++)
                {
                    // viewport_buffer[j] = viewport_tiles[tile + j];
                    IntType ttile_x = tile_x + j;
                    IntType min_x_ = IntType(RealType(viewport.width_ * ttile_x) / RealType(NUM_TILES_X)) + viewport.min_x_;
                    IntType max_x_ = IntType(RealType(viewport.width_ * (ttile_x + 1)) / RealType(NUM_TILES_X)) + viewport.min_x_;
                    IntType min_y_ = IntType(RealType(viewport.height_ * tile_y) / RealType(NUM_TILES_Y)) + viewport.min_y_;
                    IntType max_y_ = IntType(RealType(viewport.height_ * (tile_y + 1)) / RealType(NUM_TILES_Y)) + viewport.min_y_;

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
                    for (int i = 0; i < NUM_BUFFERS; i++)
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
                        triangle_count[i] = min(IntType(triangle_count[i] + 1), IntType(MAX_TRI_PER_TILE - 1));
                    }
                }

            renderbase_reset_depth_buffer_loop:
                for (int j = 0; j < tile_width * tile_height; j++)
                {
                // #pragma HLS pipeline II = 1
                renderbase_reset_depth_buffer_i_loop:
                    for (int i = 0; i < NUM_BUFFERS; i++)
                    {
                        // #pragma HLS unroll
                        fragment_buffer[i][j] = Base::fragment_nodata(outtextures);
                        depth_buffer[i][j] = RealType(-1);
                    }
                }

            renderbase_render_tile_loop:
                for (int i = 0; i < NUM_BUFFERS; i++)
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
                for (int i = 0; i < NUM_BUFFERS; i++)
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
        Triangle triangles[MAX_NUM_TRI];
        // Pack the aggregate (newer pragma)
// #pragma HLS aggregate variable = triangles compact = bit
//  or, in some versions:
//  #pragma HLS data_pack variable=triangles
#pragma HLS BIND_STORAGE variable = triangles type = ram_t2p impl = uram

        IntType num_triangles;
        this->get_triangles_(mesh, viewport, uniforms, triangles, IntType(MAX_NUM_TRI), num_triangles);

        IntType tile_width = IntType(ceil(RealType(viewport.width_) / NUM_TILES_X));
        IntType tile_height = IntType(ceil(RealType(viewport.height_) / NUM_TILES_Y));
        //IntType num_tiles = num_tiles_x * num_tiles_y;

        Uniforms uniforms_buffer[NUM_BUFFERS];
        BoundingBox<IntType> viewport_buffer[NUM_BUFFERS];
        Fragment fragment_buffer[NUM_BUFFERS][MAX_TILE_WIDTH * MAX_TILE_HEIGHT];
        RealType depth_buffer[NUM_BUFFERS][MAX_TILE_WIDTH * MAX_TILE_HEIGHT];

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
        for (int tile_y = 0; tile_y < NUM_TILES_Y; tile_y++)
        {
//#pragma HLS loop_tripcount min = max_tiles_y max = max_tiles_y avg = max_tiles_y

        renderbase_render_tiles_x_loop:
            for (int tile_x = 0; tile_x < NUM_TILES_X; tile_x += NUM_BUFFERS)
            {
                int tile = tile_y * NUM_TILES_Y + tile_x;
#pragma HLS loop_tripcount min = NUM_TILES_X / NUM_BUFFERS max = NUM_TILES_X / NUM_BUFFERS avg = NUM_TILES_X / NUM_BUFFERS

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
                    for (int i = 0; i < NUM_BUFFERS; i++)
                    {
                        // #pragma HLS unroll
                        fragment_buffer[i][j] = Base::fragment_nodata(outtextures);
                        depth_buffer[i][j] = RealType(-1);
                    }
                }

            renderbasehls_reset:
                for (int j = 0; j < NUM_BUFFERS; j++)
                {
                    // viewport_buffer[j] = viewport_tiles[tile + j];
                    IntType ttile_x = tile_x + j;
                    IntType min_x_ = IntType(RealType(viewport.width_ * ttile_x) / RealType(NUM_TILES_X)) + viewport.min_x_;
                    IntType max_x_ = IntType(RealType(viewport.width_ * (ttile_x + 1)) / RealType(NUM_TILES_X)) + viewport.min_x_;
                    IntType min_y_ = IntType(RealType(viewport.height_ * tile_y) / RealType(NUM_TILES_Y)) + viewport.min_y_;
                    IntType max_y_ = IntType(RealType(viewport.height_ * (tile_y + 1)) / RealType(NUM_TILES_Y)) + viewport.min_y_;

                    viewport_buffer[j] = BoundingBox<IntType>(min_x_, max_x_, min_y_, max_y_);
                    uniforms_buffer[j] = uniforms;
                }

                hls::stream<Triangle> triangles_streams[NUM_BUFFERS];

            renderbase_triangle_buffer_loop:
                for (int j = 0; j < num_triangles; j++)
                {
                    for (int i = 0; i < NUM_BUFFERS; i++)
                        triangles_streams[i].write(triangles[j]);
                }

            renderbase_triangle_tile_loop:
                for (int i = 0; i < NUM_BUFFERS; i++)
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
                for (int i = 0; i < NUM_BUFFERS; i++)
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
        // #pragma HLS INLINE

    render_tile_loop:
        for (IntType tri = 0; tri < num_triangles; tri++)
        {
// #pragma HLS pipeline off
#pragma HLS loop_tripcount min = MAX_TRI_PER_TILE max = MAX_TRI_PER_TILE avg = MAX_TRI_PER_TILE

            Triangle triangle = triangles[tri];
            this->draw_triangle_(triangle, viewport_tile, depth_buffer, uniforms, intextures, fragment_buffer);
        }
    }
};

class DepthRendererHLS
    : public RendererBaseHLS<DepthRendererHLS,
                             DepthRendererBase<TextureViewWriteHLS>>
{
public:
    using Base = DepthRendererBase<TextureViewWriteHLS>;

    // DepthRendererHLS() = default;
    //~DepthRendererHLS() = default;

    void Render(float *vertex_buffer_data,
                int *ebo_buffer_data,
                float *out_texture_data,
                int out_texture_offset,
                unsigned int vertex_buffer_size,
                unsigned int ebo_buffer_size,
                unsigned int out_texture_width,
                unsigned int out_texture_height,
                float out_nodata_value,
                float q_x, float q_y, float q_z, float q_w,
                float t_x, float t_y, float t_z,
                float fx, float fy, float cx, float cy)
    {
        linalg::SE3<RealType> pose(linalg::SO3<RealType>(linalg::Quaternion<RealType>(q_w, q_x, q_y, q_z)), linalg::Vec3<RealType>(t_x, t_y, t_z));
        PinholeCamera<RealType> cam(fx, fy, cx, cy);

        // copy data to bram
        // MeshHLS mesh(vertex_buffer_data, vertex_buffer_size,
        //             ebo_buffer_data, ebo_buffer_size);

        BufferViewReadHLS<float> vertex_buffer(vertex_buffer_data, vertex_buffer_size);
        BufferViewReadHLS<int> ebo_buffer(ebo_buffer_data, ebo_buffer_size);

        // data too large, has to be in ram
        TextureViewWriteHLS<float> out_texture(out_texture_data + out_texture_offset, out_texture_width, out_texture_height, out_nodata_value);
        // TextureRAM<float> out_texture(out_texture_width, out_texture_height, out_nodata_value, out_texture_data);

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

        Base::InTextures intextures{0};
        Base::OutTextures outtextures{
            TextureViewWriteHLS<float>(out_texture_data + out_texture_offset, out_texture_width, out_texture_height, out_nodata_value)};

        // RendererBaseHLS<DepthRendererHLS, Base>::RenderNaive(viewport, vertex_buffer, ebo_buffer, uniforms, intextures_ch1, outtextures);
        RendererBaseHLS<DepthRendererHLS, Base>::RenderTiledFragBuff2(viewport, vertex_buffer, ebo_buffer, uniforms, intextures, outtextures);
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
    : public RendererBaseHLS<ImageRendererHLS,
                             ImageRendererBase<TextureViewReadHLS, TextureViewWriteHLS>>
{
public:
    using Base = ImageRendererBase<TextureViewReadHLS, TextureViewWriteHLS>;

    ImageRendererHLS() = default;
    ~ImageRendererHLS() = default;

    void Render(float *vertex_buffer_data,
                int *ebo_buffer_data,
                ImageType *diffuse_texture_data,
                ImageType *out_texture_data,
                int diffuse_texture_offset,
                int out_texture_offset,
                unsigned int vertex_buffer_size,
                unsigned int ebo_buffer_size,
                unsigned int diffuse_texture_width,
                unsigned int diffuse_texture_height,
                ImageType diffuse_nodata_value,
                unsigned int out_texture_width,
                unsigned int out_texture_height,
                ImageType out_nodata_value,
                float q_x, float q_y, float q_z, float q_w,
                float t_x, float t_y, float t_z,
                float fx, float fy, float cx, float cy,
                float exp_a, float exp_b)
    {
        // #pragma HLS cache port = diffuse_texture_data_ch1 lines = 64 depth = 64
        // #pragma HLS cache port = diffuse_texture_data_ch2 lines = 64 depth = 64

        linalg::SE3<RealType> pose(linalg::SO3<RealType>(
                                       linalg::Quaternion<RealType>(q_w, q_x, q_y, q_z)),
                                   linalg::Vec3<RealType>(t_x, t_y, t_z));
        linalg::Vec2<RealType> exposure(exp_a, exp_b);
        PinholeCamera<RealType> cam(fx, fy, cx, cy);

        // MeshHLS mesh(vertex_buffer_data, vertex_buffer_size,
        //              ebo_buffer_data, ebo_buffer_size);
        BufferViewReadHLS<float> vertex_buffer(vertex_buffer_data, vertex_buffer_size);
        BufferViewReadHLS<int> ebo_buffer(ebo_buffer_data, ebo_buffer_size);

        Base::Uniforms uniforms;

        linalg::Mat4<RealType> opencv2opengl = linalg::Mat4<RealType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;
        linalg::Mat4<RealType> projmat = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        linalg::Mat4<RealType> viewmat = projmat * opencv2opengl;
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = viewmat;
        // imagerenderer_opencv2opengl_loop:
        //     for (int i = 0; i < 4; i++)
        //     {
        //         uniforms.view_matrix(1, i) = -uniforms.view_matrix(1, i);
        //         uniforms.view_matrix(2, i) = -uniforms.view_matrix(2, i);
        //     }
        uniforms.camera = cam;
        uniforms.exposure = exposure;

        BoundingBox<IntType> viewport(0, out_texture_width, 0, out_texture_height);

        Base::InTextures intextures{TextureViewReadHLS<ImageType>((ImageType *)diffuse_texture_data + diffuse_texture_offset,
                                                                  diffuse_texture_width,
                                                                  diffuse_texture_height,
                                                                  diffuse_nodata_value)};

        Base::OutTextures outtextures{TextureViewWriteHLS<ImageType>((ImageType *)out_texture_data + out_texture_offset,
                                                                     out_texture_width,
                                                                     out_texture_height,
                                                                     out_nodata_value)};

        // RendererBaseHLS<ImageRendererHLS, Base>::RenderNaive(
        //     viewport, vertex_buffer, ebo_buffer, uniforms, intextures_ch1, outtextures);

        RendererBaseHLS<ImageRendererHLS, Base>::RenderTiledFragBuff2(
            viewport, vertex_buffer, ebo_buffer, uniforms, intextures, outtextures);
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

class DIDxyRendererHLS
    : public RendererBaseHLS<DIDxyRendererHLS,
                             DIDxyRendererBase<TextureViewReadHLS, TextureViewWriteHLS>>
{
public:
    using Base = DIDxyRendererBase<TextureViewReadHLS, TextureViewWriteHLS>;

    DIDxyRendererHLS() = default;
    ~DIDxyRendererHLS() = default;

    void Render(float *vertex_buffer_data,
                int *ebo_buffer_data,
                ap_uint<8> *in_texture_data,
                ap_uint<8> *out_texture_data,
                int in_texture_offset,
                int out_texture_offset,
                unsigned int vertex_buffer_size,
                unsigned int ebo_buffer_size,
                unsigned int in_texture_width,
                unsigned int in_texture_height,
                ImageType in_nodata_value,
                unsigned int out_texture_width,
                unsigned int out_texture_height,
                float out_nodata_value_x,
                float out_nodata_value_y,
                float out_nodata_value_z)
    {
        BufferViewReadHLS<float> vertex_buffer(vertex_buffer_data, vertex_buffer_size);
        BufferViewReadHLS<int> ebo_buffer(ebo_buffer_data, ebo_buffer_size);

        Base::Uniforms uniforms;

        uniforms.in_lvl = 0;
        uniforms.out_lvl = 0;

        BoundingBox<IntType> viewport(0, out_texture_width, 0, out_texture_height);

        Base::InTextures intextures_ch1{TextureViewReadHLS<ImageType>((ImageType *)in_texture_data + in_texture_offset,
                                                                      in_texture_width,
                                                                      in_texture_height,
                                                                      in_nodata_value)};
        Base::OutTextures outtextures{TextureViewWriteHLS<Vec3<float>>((Vec3<float> *)out_texture_data + out_texture_offset,
                                                                       out_texture_width,
                                                                       out_texture_height,
                                                                       Vec3<float>(out_nodata_value_x, out_nodata_value_y, out_nodata_value_z))};

        // RendererBaseHLS<ImageRendererHLS, Base>::RenderNaive(
        //     viewport, vertex_buffer, ebo_buffer, uniforms, intextures_ch1, outtextures);

        RendererBaseHLS<DIDxyRendererHLS, Base>::RenderTiledFragBuff2(
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

class DiffRendererHLS
    : public RendererBaseHLS<DiffRendererHLS,
                             DiffRendererBase<TextureViewReadHLS, TextureViewWriteHLS>>
{
public:
    using Base = DiffRendererBase<TextureViewReadHLS, TextureViewWriteHLS>;

    DiffRendererHLS() = default;
    ~DiffRendererHLS() = default;

    void Render(float *vertex_buffer_data,
                int *ebo_buffer_data,
                ap_uint<8> *kf_texture_data,
                ap_uint<8> *dkfdxy_texture_data,
                ap_uint<8> *image_texture_data,
                ap_uint<8> *jtra_texture_data,
                ap_uint<8> *jrot_texture_data,
                ap_uint<8> *jexp_texture_data,
                ap_uint<8> *jmap_texture_data,
                ap_uint<8> *pids_texture_data,
                int in_texture_offset,
                int out_texture_offset,
                unsigned int vertex_buffer_size,
                unsigned int ebo_buffer_size,
                unsigned int in_texture_width,
                unsigned int in_texture_height,
                unsigned int out_texture_width,
                unsigned int out_texture_height,
                float q_x, float q_y, float q_z, float q_w,
                float t_x, float t_y, float t_z,
                float fx, float fy, float cx, float cy,
                float exp_a, float exp_b)
    {
        linalg::SE3<RealType> pose(linalg::SO3<RealType>(
                                       linalg::Quaternion<RealType>(q_w, q_x, q_y, q_z)),
                                   linalg::Vec3<RealType>(t_x, t_y, t_z));
        linalg::Vec2<RealType> exposure(exp_a, exp_b);
        PinholeCamera<RealType> cam(fx, fy, cx, cy);

        // MeshHLS mesh(vertex_buffer_data, vertex_buffer_size,
        //              ebo_buffer_data, ebo_buffer_size);
        BufferViewReadHLS<float> vertex_buffer(vertex_buffer_data, vertex_buffer_size);
        BufferViewReadHLS<int> ebo_buffer(ebo_buffer_data, ebo_buffer_size);

        ImageType *kf_pointer = (ImageType *)kf_texture_data;
        kf_pointer += in_texture_offset;
        Vec3<float> *dkfdxy_pointer = (Vec3<float> *)dkfdxy_texture_data;
        dkfdxy_pointer += in_texture_offset;

        ImageType *image_pointer = (ImageType *)image_texture_data;
        image_pointer += out_texture_offset;
        Vec3<float> *jtra_pointer = (Vec3<float> *)jtra_texture_data;
        jtra_pointer += out_texture_offset;
        Vec3<float> *jrot_pointer = (Vec3<float> *)jrot_texture_data;
        jrot_pointer += out_texture_offset;
        Vec3<float> *jexp_pointer = (Vec3<float> *)jexp_texture_data;
        jexp_pointer += out_texture_offset;
        Vec3<float> *jmap_pointer = (Vec3<float> *)jmap_texture_data;
        jmap_pointer += out_texture_offset;
        Vec3<float> *pids_pointer = (Vec3<float> *)pids_texture_data;
        pids_pointer += out_texture_offset;

        TextureViewReadHLS<ImageType> kf_texture(kf_pointer, in_texture_width, in_texture_height, 0);
        TextureViewWriteHLS<Vec3<float>> dkfdxy_texture(dkfdxy_pointer, in_texture_width, in_texture_height, Vec3<float>(0.0, 0.0, 0.0));
        TextureViewReadHLS<ImageType> image_texture(image_pointer, out_texture_width, out_texture_height, 0);
        TextureViewWriteHLS<Vec3<float>> jtra_texture(jtra_pointer, out_texture_width, out_texture_height, Vec3<float>(0.0, 0.0, 0.0));
        TextureViewWriteHLS<Vec3<float>> jrot_texture(jrot_pointer, out_texture_width, out_texture_height, Vec3<float>(0.0, 0.0, 0.0));
        TextureViewWriteHLS<Vec3<float>> jexp_texture(jexp_pointer, out_texture_width, out_texture_height, Vec3<float>(0.0, 0.0, 0.0));
        TextureViewWriteHLS<Vec3<float>> jmap_texture(jmap_pointer, out_texture_width, out_texture_height, Vec3<float>(0.0, 0.0, 0.0));
        TextureViewWriteHLS<Vec3<float>> pids_texture(pids_pointer, out_texture_width, out_texture_height, Vec3<float>(-1.0, -1.0, -1.0));

        Mat4<RealType> opencv2opengl = Mat4<RealType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        const int W = static_cast<int>(image_texture.width());
        const int H = static_cast<int>(image_texture.height());
        BoundingBox<IntType> viewport(0, W, 0, H);

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.exposure = exposure;
        uniforms.out_width = W;
        uniforms.out_height = H;

        Base::InTextures intextures{TextureViewReadHLS<ImageType>(kf_pointer, in_texture_width, in_texture_height, 0),
                                    TextureViewWriteHLS<Vec3<float>>(dkfdxy_pointer, in_texture_width, in_texture_height, Vec3<float>(0.0, 0.0, 0.0))};
        Base::OutTextures outtextures{TextureViewWriteHLS<Vec3<float>>(jtra_pointer, out_texture_width, out_texture_height, Vec3<float>(0.0, 0.0, 0.0)),
                                      TextureViewWriteHLS<Vec3<float>>(jrot_pointer, out_texture_width, out_texture_height, Vec3<float>(0.0, 0.0, 0.0)),
                                      TextureViewWriteHLS<Vec3<float>>(jexp_pointer, out_texture_width, out_texture_height, Vec3<float>(0.0, 0.0, 0.0)),
                                      TextureViewWriteHLS<Vec3<float>>(jmap_pointer, out_texture_width, out_texture_height, Vec3<float>(0.0, 0.0, 0.0)),
                                      TextureViewWriteHLS<Vec3<float>>(pids_pointer, out_texture_width, out_texture_height, Vec3<float>(-1.0, -1.0, -1.0)),
                                      TextureViewReadHLS<ImageType>(image_pointer, out_texture_width, out_texture_height, 0)};

        // RendererBaseHLS<JPoseExpMapRendererHLS, Base>::RenderNaive(
        //     viewport, vertex_buffer, ebo_buffer, uniforms, intextures, outtextures);

        RendererBaseHLS<DiffRendererHLS, Base>::RenderTiledFragBuff2(
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
