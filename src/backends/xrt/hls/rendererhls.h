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
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/bufferhls.h"
#include "backends/xrt/hls/meshhls.h"

#define num_buffers 4

#define tile_width 64
#define tile_height 64

// static constexpr int max_width = 640;
// static constexpr int max_height = 480;

// taken from the planet dataset
#define max_num_tri 2048 // 768;

#define max_num_tiles_x 128
#define max_num_tiles_y 128
#define max_num_tiles max_num_tiles_x *max_num_tiles_y

// only for performance metrics
// taken from the planet dataset
#define max_tri_width 70
#define max_tri_height 70
// only half of the triangles are visible
// then try to estimate how many triangles there are per tile
#define max_tri_per_tile 16
//(max_num_tri / 2) * tile_width *tile_height / (640 * 480)

template <class Derived, class Base>
class RendererBaseHLS
    : public RendererBase<MathType, Base>
{
public:
    // RendererBaseHLS() = default;
    //~RendererBaseHLS() = default;

    template <typename Mesh, typename Uniforms, typename InTextures, typename OutTextures>
    void RenderNaive(const BoundingBox<int> &viewport,
                     const Mesh &mesh,
                     const Uniforms &uniforms,
                     InTextures &intextures,
                     OutTextures &outtextures)
    {
        // MathType depth_buffer[viewport.width_ * viewport.height_] = {MathType(-1)};
        MathType depth_buffer[640 * 480] = {MathType(-1)};

    // Loop over triangles
    renderbase_render_triangles_loop:
        for (unsigned int i = 0; i + 2 < mesh.ebo_buffer_.size(); i += 3)
        {
#pragma HLS loop_tripcount min = 768 max = 768 avg = 768

            unsigned int vertexids[3];

            vertexids[0] = mesh.ebo_buffer_[i + 0];
            vertexids[1] = mesh.ebo_buffer_[i + 1];
            vertexids[2] = mesh.ebo_buffer_[i + 2];

            typename Base::VertexData vertexdata[3];

            vertexdata[0] = Base::get_vertex_data(mesh, vertexids[0]);
            vertexdata[1] = Base::get_vertex_data(mesh, vertexids[1]);
            vertexdata[2] = Base::get_vertex_data(mesh, vertexids[2]);

            typename RendererBase<MathType, Base>::Triangle triangle;

            this->create_triangle_(vertexdata, vertexids, viewport, uniforms, triangle);

            // directly write to dram
            this->draw_triangle_(triangle, viewport, depth_buffer, uniforms, intextures, outtextures);
        }
    }

    template <typename Mesh, typename Uniforms, typename InTextures, typename OutTextures>
    void RenderDualOutChannels(const BoundingBox<int> &viewport,
                               const Mesh &mesh,
                               const Uniforms &uniforms,
                               InTextures &intextures,
                               OutTextures &outtextures_ch1,
                               OutTextures &outtextures_ch2)
    {

        typename RendererBase<MathType, Base>::Triangle triangles[max_num_tri];
#pragma HLS BIND_STORAGE variable = triangles type = ram_t2p impl = uram

        int num_triangles;
        this->get_triangles(mesh, viewport, uniforms, triangles, num_triangles);

        BoundingBox<int> viewport_tiles[max_num_tiles];

        // #pragma HLS BIND_STORAGE variable = viewport_tiles type = ram_t2p impl = uram
#pragma HLS ARRAY_PARTITION variable = viewport_tiles complete dim = 1

        int num_tiles_x = int(ceil(MathType(viewport.width_) / tile_width));
        int num_tiles_y = int(ceil(MathType(viewport.height_) / tile_height));
        int num_tiles = num_tiles_x * num_tiles_y;

        this->create_tile_viewports_(viewport_tiles, viewport, num_tiles_x, num_tiles_y);

        typename Base::Uniforms uniforms_buffers[num_buffers];
        typename RendererBase<MathType, Base>::Triangle triangle_buffer[num_buffers][max_tri_per_tile];
        MathType depth_buffer[num_buffers][tile_width * tile_height];

#pragma HLS array_partition variable = uniforms_buffers complete dim = 1

#pragma HLS BIND_STORAGE variable = triangle_buffer type = ram_t2p impl = uram
#pragma HLS array_partition variable = triangle_buffer complete dim = 1

#pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = uram
#pragma HLS array_partition variable = depth_buffer complete dim = 1
        //   #pragma HLS array_partition variable = depth_buffer cyclic factor = 2 dim = 2

        // #pragma HLS BIND_STORAGE variable = fragment_buffer type = ram_t2p impl = uram
        // #pragma HLS array_partition variable = fragment_buffer complete dim = 1
        //    #pragma HLS array_partition variable = fragment_buffer cyclic factor = 2 dim = 2

    renderbase_render_tiles_loop:
        for (int tile = 0; tile < num_tiles; tile += num_buffers)
        {
#pragma HLS loop_tripcount min = max_num_tiles / num_buffers max = max_num_tiles / num_buffers avg = max_num_tiles / num_buffers

#pragma HLS dependence variable = uniforms_buffers type = inter false
#pragma HLS dependence variable = uniforms_buffers type = intra false

#pragma HLS dependence variable = depth_buffer type = inter false
#pragma HLS dependence variable = depth_buffer type = intra false

#pragma HLS dependence variable = triangle_buffer type = inter false
#pragma HLS dependence variable = triangle_buffer type = intra false

#pragma HLS dependence variable = viewport_tiles type = inter false
#pragma HLS dependence variable = viewport_tiles type = intra false

#pragma HLS dependence variable = intextures type = inter false
#pragma HLS dependence variable = intextures type = intra false

#pragma HLS dependence variable = outtextures_ch1 type = inter false
#pragma HLS dependence variable = outtextures_ch1 type = intra false

#pragma HLS dependence variable = outtextures_ch2 type = inter false
#pragma HLS dependence variable = outtextures_ch2 type = intra false

            int triangle_count[num_buffers] = {0};

        renderbase_triangle_buffer_loop:
            for (int j = 0; j < num_triangles; j++)
            {
#pragma HLS loop_tripcount min = max_num_tri max = max_num_tri avg = max_num_tri

                typename RendererBase<MathType, Base>::Triangle triangle = triangles[j];
                BoundingBox<MathType> tri_bb(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen);

            renderbase_triangle_tile_loop:
                for (int i = 0; i < num_buffers; i++)
                {
                    int min_x = max(viewport_tiles[tile + i].min_x_, static_cast<int>(floor(tri_bb.min_x_)));
                    int max_x = min(viewport_tiles[tile + i].max_x_, static_cast<int>(ceil(tri_bb.max_x_)));
                    int min_y = max(viewport_tiles[tile + i].min_y_, static_cast<int>(floor(tri_bb.min_y_)));
                    int max_y = min(viewport_tiles[tile + i].max_y_, static_cast<int>(ceil(tri_bb.max_y_)));

                    if (min_x >= max_x || min_y >= max_y)
                        continue;

                    triangle_buffer[i][triangle_count[i]] = triangle;
                    triangle_count[i]++;
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
                    depth_buffer[i][j] = MathType(-1);
                }
            }

        renderbase_render_tile_loop:
            for (int i = 0; i < num_buffers; i++)
            {
#pragma HLS unroll
                if (i == 0)
                    this->render_tile_(outtextures_ch1, depth_buffer[i], triangle_buffer[i], triangle_count[i], viewport_tiles[tile + i], uniforms_buffers[i], intextures);
                else
                    this->render_tile_(outtextures_ch2, depth_buffer[i], triangle_buffer[i], triangle_count[i], viewport_tiles[tile + i], uniforms_buffers[i], intextures);
            }
        }
    }

    template <typename Mesh, typename Uniforms, typename InTextures, typename OutTextures>
    void RenderTiled(const BoundingBox<int> &viewport,
                     const Mesh &mesh,
                     const Uniforms &uniforms,
                     InTextures &intextures,
                     OutTextures &outtextures)
    {
        typename RendererBase<MathType, Base>::Triangle triangles[max_num_tri];
        // Pack the aggregate (newer pragma)
#pragma HLS aggregate variable = triangles compact = bit
        // or, in some versions:
        // #pragma HLS data_pack variable=triangles
#pragma HLS BIND_STORAGE variable = triangles type = ram_t2p impl = uram

        int num_triangles;
        this->get_triangles(mesh, viewport, uniforms, triangles, num_triangles);

        BoundingBox<int> viewport_tiles[max_num_tiles];

        // #pragma HLS BIND_STORAGE variable = viewport_tiles type = ram_t2p impl = uram
#pragma HLS ARRAY_PARTITION variable = viewport_tiles complete dim = 1

        int num_tiles_x = int(ceil(MathType(viewport.width_) / tile_width));
        int num_tiles_y = int(ceil(MathType(viewport.height_) / tile_height));
        int num_tiles = num_tiles_x * num_tiles_y;

        this->create_tile_viewports_(viewport_tiles, viewport, num_tiles_x, num_tiles_y);

        typename RendererBase<MathType, Base>::Triangle triangle_buffer[num_buffers][max_tri_per_tile];
        typename Base::Fragment fragment_buffer[num_buffers][tile_width * tile_height];
        MathType depth_buffer[num_buffers][tile_width * tile_height];

        // Pack the aggregate (newer pragma)
        // Pack the aggregate (newer pragma)
#pragma HLS aggregate variable = triangle_buffer compact = bit
        // or, in some versions:
        // #pragma HLS data_pack variable=triangle_buffer
#pragma HLS aggregate variable = triangle_buffer compact = bit
        // or, in some versions:
        // #pragma HLS data_pack variable=fragment_buffer
#pragma HLS BIND_STORAGE variable = triangle_buffer type = ram_t2p impl = uram
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
        for (int tile = 0; tile < num_tiles; tile += num_buffers)
        {
#pragma HLS loop_tripcount min = max_num_tiles / num_buffers max = max_num_tiles / num_buffers avg = max_num_tiles / num_buffers

#pragma HLS dependence variable = fragment_buffer type = inter false
#pragma HLS dependence variable = fragment_buffer type = intra false

#pragma HLS dependence variable = depth_buffer type = inter false
#pragma HLS dependence variable = depth_buffer type = intra false

#pragma HLS dependence variable = triangle_buffer type = inter false
#pragma HLS dependence variable = triangle_buffer type = intra false

#pragma HLS dependence variable = viewport_tiles type = inter false
#pragma HLS dependence variable = viewport_tiles type = intra false

#pragma HLS dependence variable = intextures type = inter false
#pragma HLS dependence variable = intextures type = intra false

            int triangle_count[num_buffers] = {0};

        renderbase_triangle_buffer_loop:
            for (int j = 0; j < max_num_tri; j++)
            {
                typename RendererBase<MathType, Base>::Triangle triangle = triangles[j];
                BoundingBox<MathType> tri_bb(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen);

            renderbase_triangle_tile_loop:
                for (int i = 0; i < num_buffers; i++)
                {
                    int min_x = max(viewport_tiles[tile + i].min_x_, static_cast<int>(floor(tri_bb.min_x_)));
                    int max_x = min(viewport_tiles[tile + i].max_x_, static_cast<int>(ceil(tri_bb.max_x_)));
                    int min_y = max(viewport_tiles[tile + i].min_y_, static_cast<int>(floor(tri_bb.min_y_)));
                    int max_y = min(viewport_tiles[tile + i].max_y_, static_cast<int>(ceil(tri_bb.max_y_)));

                    if (min_x >= max_x || min_y >= max_y)
                        continue;

                    triangle_buffer[i][triangle_count[i]] = triangle;
                    triangle_count[i]++;
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
                    depth_buffer[i][j] = MathType(-1);
                }
            }

        renderbase_render_tile_loop:
            for (int i = 0; i < num_buffers; i++)
            {
#pragma HLS unroll
                this->render_tile_(fragment_buffer[i], depth_buffer[i], triangle_buffer[i], triangle_count[i], viewport_tiles[tile + i], uniforms, intextures);
            }

        depthrendererhls_loop:
            for (int i = 0; i < num_buffers; i++)
            {
                Derived::sync_outtextures(outtextures, viewport_tiles[tile + i], fragment_buffer[i], uniforms);
            }
        }
    }

    template <typename Mesh, typename Uniforms, typename InTextures, typename OutTextures>
    void RenderTiledInChannels(const BoundingBox<int> &viewport,
                               const Mesh &mesh,
                               const Uniforms &uniforms,
                               InTextures &intextures_ch1,
                               InTextures &intextures_ch2,
                               InTextures &intextures_ch3,
                               InTextures &intextures_ch4,
                               OutTextures &outtextures)
    {
        /*
        InTextures intextures[num_buffers];

        for (int i = 0; i < num_buffers; i++)
        {
            if (i == 0)
                intextures[i] = intextures_ch1;
            if (i == 1)
                intextures[i] = intextures_ch2;
            if (i == 2)
                intextures[i] = intextures_ch3;
            if (i == 3)
                intextures[i] = intextures_ch1;
        }
        */

        typename RendererBase<MathType, Base>::Triangle triangles[max_num_tri];
        // Pack the aggregate (newer pragma)
#pragma HLS aggregate variable = triangles compact = bit
        // or, in some versions:
        // #pragma HLS data_pack variable=triangles
#pragma HLS BIND_STORAGE variable = triangles type = ram_t2p impl = uram

        int num_triangles;
        this->get_triangles(mesh, viewport, uniforms, triangles, num_triangles);

        int num_tiles_x = int(ceil(MathType(viewport.width_) / tile_width));
        int num_tiles_y = int(ceil(MathType(viewport.height_) / tile_height));
        int num_tiles = num_tiles_x * num_tiles_y;

        // BoundingBox<int> viewport_tiles[max_num_tiles];
        // #pragma HLS BIND_STORAGE variable = viewport_tiles type = ram_t2p impl = uram

        //      this->create_tile_viewports_(viewport_tiles, viewport, num_tiles_x, num_tiles_y);

        int triangle_count[num_buffers];
        Uniforms uniforms_buffer[num_buffers];
        BoundingBox<int> viewport_buffer[num_buffers];
        typename RendererBase<MathType, Base>::Triangle triangle_buffer[num_buffers][max_tri_per_tile];
        typename Base::Fragment fragment_buffer[num_buffers][tile_width * tile_height];
        MathType depth_buffer[num_buffers][tile_width * tile_height];

#pragma HLS array_partition variable = triangle_count complete dim = 1

        // Pack the aggregate (newer pragma)
#pragma HLS aggregate variable = uniforms_buffer compact = bit
        // or, in some versions:
        // #pragma HLS data_pack variable=uniforms_buffer
#pragma HLS array_partition variable = uniforms_buffer complete dim = 1

        // Pack the aggregate (newer pragma)
#pragma HLS aggregate variable = viewport_buffer compact = bit
        // or, in some versions:
        // #pragma HLS data_pack variable=viewport_buffer
#pragma HLS array_partition variable = viewport_buffer complete dim = 1

        // Pack the aggregate (newer pragma)
#pragma HLS aggregate variable = triangle_buffer compact = bit
        // or, in some versions:
        // #pragma HLS data_pack variable=triangle_buffer
#pragma HLS BIND_STORAGE variable = triangle_buffer type = ram_t2p impl = uram
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
#pragma HLS loop_tripcount min = max_num_tiles_y max = max_num_tiles_y avg = max_num_tiles_y

        renderbase_render_tiles_x_loop:
            for (int tile_x = 0; tile_x < num_tiles_x; tile_x += num_buffers)
            {
                int tile = tile_y * num_tiles_y + tile_x;
#pragma HLS loop_tripcount min = max_num_tiles_x / num_buffers max = max_num_tiles_x / num_buffers avg = max_num_tiles_x / num_buffers

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
                    int ttile_x = tile_x + j;
                    int min_x_ = int(MathType(viewport.width_ * ttile_x) / MathType(num_tiles_x)) + viewport.min_x_;
                    int max_x_ = int(MathType(viewport.width_ * (ttile_x + 1)) / MathType(num_tiles_x)) + viewport.min_x_;
                    int min_y_ = int(MathType(viewport.height_ * tile_y) / MathType(num_tiles_y)) + viewport.min_y_;
                    int max_y_ = int(MathType(viewport.height_ * (tile_y + 1)) / MathType(num_tiles_y)) + viewport.min_y_;

                    viewport_buffer[j] = BoundingBox<int>(min_x_, max_x_, min_y_, max_y_);
                    triangle_count[j] = 0;
                }

            renderbase_triangle_buffer_loop:
                for (int j = 0; j < num_triangles; j++)
                {
#pragma HLS loop_tripcount min = max_num_tri max = max_num_tri avg = max_num_tri

                    typename RendererBase<MathType, Base>::Triangle triangle = triangles[j];
                    BoundingBox<MathType> tri_bb(triangle.vout[0].screen, triangle.vout[1].screen, triangle.vout[2].screen);

                renderbase_triangle_tile_loop:
                    for (int i = 0; i < num_buffers; i++)
                    {
                        int min_x = max(viewport_buffer[i].min_x_, static_cast<int>(floor(tri_bb.min_x_)));
                        int max_x = min(viewport_buffer[i].max_x_, static_cast<int>(ceil(tri_bb.max_x_)));
                        int min_y = max(viewport_buffer[i].min_y_, static_cast<int>(floor(tri_bb.min_y_)));
                        int max_y = min(viewport_buffer[i].max_y_, static_cast<int>(ceil(tri_bb.max_y_)));

                        if (min_x >= max_x || min_y >= max_y)
                            continue;

                        triangle_buffer[i][triangle_count[i]] = triangle;
                        triangle_count[i]++;
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
                        depth_buffer[i][j] = MathType(-1);
                    }
                }

            renderbase_render_tile_loop:
                for (int i = 0; i < num_buffers; i++)
                {
#pragma HLS unroll
                    if (i == 0)
                        this->render_tile_(fragment_buffer[i], depth_buffer[i], triangle_buffer[i], triangle_count[i], viewport_buffer[i], uniforms_buffer[i], intextures_ch1);
                    if (i == 1)
                        this->render_tile_(fragment_buffer[i], depth_buffer[i], triangle_buffer[i], triangle_count[i], viewport_buffer[i], uniforms_buffer[i], intextures_ch2);
                    if (i == 2)
                        this->render_tile_(fragment_buffer[i], depth_buffer[i], triangle_buffer[i], triangle_count[i], viewport_buffer[i], uniforms_buffer[i], intextures_ch3);
                    if (i == 3)
                        this->render_tile_(fragment_buffer[i], depth_buffer[i], triangle_buffer[i], triangle_count[i], viewport_buffer[i], uniforms_buffer[i], intextures_ch4);
                }

            depthrendererhls_loop:
                for (int i = 0; i < num_buffers; i++)
                {
                    Derived::sync_outtextures(outtextures, viewport_buffer[i], fragment_buffer[i], uniforms_buffer[i]);
                }
            }
        }
    }

private:
};

class DepthRendererHLS
    : public RendererBaseHLS<DepthRendererHLS, DepthRendererBase<MathType, DepthType, TextureRAM>>
{
public:
    using Base = DepthRendererBase<MathType, DepthType, TextureRAM>;

    // DepthRendererHLS() = default;
    //~DepthRendererHLS() = default;

    void RenderNaive(const MeshHLS &mesh,
                     const linalg::SE3<MathType> &pose,
                     const Camera<MathType> &cam,
                     int out_lvl,
                     TextureRAM<DepthType> &out_texture)
    {
        linalg::Mat4<MathType> opencv2opengl = linalg::Mat4<MathType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl * pose.matrix();
        uniforms.out_lvl = out_lvl;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{0};
        Base::OutTextures outtextures{out_texture};

        RendererBaseHLS<DepthRendererHLS, Base>::RenderNaive(viewport, mesh, uniforms, intextures, outtextures);
    }

    void RenderDualOutChannels(const MeshHLS &mesh,
                               const linalg::SE3<MathType> &pose,
                               const Camera<MathType> &cam,
                               int out_lvl,
                               TextureRAM<DepthType> &out_texture_ch1,
                               TextureRAM<DepthType> &out_texture_ch2)
    {
        linalg::Mat4<MathType> opencv2opengl = linalg::Mat4<MathType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl * pose.matrix();
        uniforms.out_lvl = out_lvl;

        const int W = static_cast<int>(out_texture_ch1.width(out_lvl));
        const int H = static_cast<int>(out_texture_ch1.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{0};

        Base::OutTextures outtextures_ch1{out_texture_ch1};
        Base::OutTextures outtextures_ch2{out_texture_ch2};

        RendererBaseHLS<DepthRendererHLS, Base>::RenderDualOutChannels(viewport, mesh, uniforms, intextures, outtextures_ch1, outtextures_ch2);
    }

    void RenderTiled(const MeshHLS &mesh,
                     const linalg::SE3<MathType> &pose,
                     const Camera<MathType> &cam,
                     int out_lvl,
                     TextureRAM<DepthType> &out_texture)
    {
        linalg::Mat4<MathType> opencv2opengl = linalg::Mat4<MathType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl * pose.matrix();
        uniforms.out_lvl = out_lvl;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{0};
        Base::OutTextures outtextures{out_texture};

        RendererBaseHLS<DepthRendererHLS, Base>::RenderTiled(viewport, mesh, uniforms, intextures, outtextures);
    }

    template <typename OutTextures, typename Uniforms, typename Fragment>
    static void sync_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
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
                textures.out_texture.set_texel_(depth, y, x, uniforms.out_lvl);
            }
        }
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
    : public RendererBaseHLS<ImageRendererHLS, ImageRendererBase<MathType, ImageType, TextureRAM, TextureRAM>>
{
public:
    using Base = ImageRendererBase<MathType, ImageType, TextureRAM, TextureRAM>;

    ImageRendererHLS() = default;
    ~ImageRendererHLS() = default;

    void RenderNaive(const MeshHLS &mesh,
                     const linalg::SE3<MathType> &pose,
                     const Camera<MathType> &cam,
                     int in_lvl,
                     int out_lvl,
                     const TextureRAM<ImageType> &diffuse_texture,
                     TextureRAM<ImageType> &out_texture)
    {
        linalg::Mat4<MathType> opencv2opengl = linalg::Mat4<MathType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl * pose.matrix();
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{diffuse_texture};
        Base::OutTextures outtextures{out_texture};

        RendererBaseHLS<ImageRendererHLS, Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

    void RenderTiled(const MeshHLS &mesh,
                     const linalg::SE3<MathType> &pose,
                     const Camera<MathType> &cam,
                     int in_lvl,
                     int out_lvl,
                     const TextureRAM<ImageType> &diffuse_texture,
                     TextureRAM<ImageType> &out_texture)
    {
        linalg::Mat4<MathType> opencv2opengl = linalg::Mat4<MathType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl * pose.matrix();
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{diffuse_texture};
        Base::OutTextures outtextures{out_texture};

        RendererBaseHLS<ImageRendererHLS, Base>::RenderTiled(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

    void RenderTiledInChannels(const MeshHLS &mesh,
                               const linalg::SE3<MathType> &pose,
                               const Camera<MathType> &cam,
                               int in_lvl,
                               int out_lvl,
                               const TextureRAM<ImageType> &diffuse_texture_ch1,
                               const TextureRAM<ImageType> &diffuse_texture_ch2,
                               const TextureRAM<ImageType> &diffuse_texture_ch3,
                               const TextureRAM<ImageType> &diffuse_texture_ch4,
                               TextureRAM<ImageType> &out_texture)
    {
        linalg::Mat4<MathType> opencv2opengl = linalg::Mat4<MathType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl * pose.matrix();
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Base::InTextures intextures[2] = {{diffuse_texture_ch1}, {diffuse_texture_ch2}};
        Base::InTextures intextures_ch1 = {diffuse_texture_ch1};
        Base::InTextures intextures_ch2 = {diffuse_texture_ch2};
        Base::InTextures intextures_ch3 = {diffuse_texture_ch3};
        Base::InTextures intextures_ch4 = {diffuse_texture_ch4};

        Base::OutTextures outtextures{out_texture};

        RendererBaseHLS<ImageRendererHLS, Base>::RenderTiledInChannels(
            viewport,
            mesh,
            uniforms,
            intextures_ch1,
            intextures_ch2,
            intextures_ch3,
            intextures_ch4,
            outtextures);
    }

    template <typename OutTextures, typename Uniforms, typename Fragment>
    static void sync_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
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

                ImageType color = fragment_buffer[address].color;
                textures.out_texture.set_texel_(color, y, x, uniforms.out_lvl);
            }
        }
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

class DiffRendererHLS
    : public RendererBaseHLS<DiffRendererHLS,
                             DiffRendererBase<MathType, ImageType, DType, DType, IdType, TextureRAM>>
{
public:
    using Base = DiffRendererBase<MathType, ImageType, DType, DType, IdType, TextureRAM>;

    DiffRendererHLS() = default;
    ~DiffRendererHLS() = default;

    void RenderNaive(const MeshHLS &mesh,
                     const linalg::SE3<MathType> &pose,
                     const Camera<MathType> &cam,
                     int in_lvl,
                     int out_lvl,
                     const TextureRAM<ImageType> &diffuse_texture,
                     TextureRAM<ImageType> &image_texture,
                     TextureRAM<DepthType> &depth_texture,
                     TextureRAM<linalg::Vec3<DType>> &jtra_texture,
                     TextureRAM<linalg::Vec3<DType>> &jrot_texture,
                     TextureRAM<linalg::Vec3<DType>> &jmap_texture,
                     TextureRAM<linalg::Vec3<IdType>> &pids_texture)
    {
        linalg::Mat4<MathType> opencv2opengl = linalg::Mat4<MathType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;

        const int W = static_cast<int>(image_texture.width(out_lvl));
        const int H = static_cast<int>(image_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Base::InTextures intextures{diffuse_texture};
        Base::OutTextures outtextures{image_texture, depth_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture};

        RendererBaseHLS<DiffRendererHLS, Base>::RenderNaive(
            viewport,
            mesh,
            uniforms,
            intextures,
            outtextures);
    }

    void RenderTiledInChannels(const MeshHLS &mesh,
                               const linalg::SE3<MathType> &pose,
                               const Camera<MathType> &cam,
                               int in_lvl,
                               int out_lvl,
                               const TextureRAM<ImageType> &diffuse_texture_ch1,
                               const TextureRAM<ImageType> &diffuse_texture_ch2,
                               const TextureRAM<ImageType> &diffuse_texture_ch3,
                               const TextureRAM<ImageType> &diffuse_texture_ch4,
                               TextureRAM<ImageType> &image_texture,
                               TextureRAM<DepthType> &depth_texture,
                               TextureRAM<linalg::Vec3<DType>> &jtra_texture,
                               TextureRAM<linalg::Vec3<DType>> &jrot_texture,
                               TextureRAM<linalg::Vec3<DType>> &jmap_texture,
                               TextureRAM<linalg::Vec3<IdType>> &pids_texture)
    {
        linalg::Mat4<MathType> opencv2opengl = linalg::Mat4<MathType>::Identity();
        opencv2opengl(1, 1) = -1.0;
        opencv2opengl(2, 2) = -1.0;

        Base::Uniforms uniforms;
        uniforms.fx = cam.GetParams()(0);
        uniforms.fy = cam.GetParams()(1);
        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl;
        uniforms.in_lvl = in_lvl;
        uniforms.out_lvl = out_lvl;

        const int W = static_cast<int>(image_texture.width(out_lvl));
        const int H = static_cast<int>(image_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Base::InTextures intextures[2] = {{diffuse_texture_ch1}, {diffuse_texture_ch2}};
        Base::InTextures intextures_ch1 = {diffuse_texture_ch1};
        Base::InTextures intextures_ch2 = {diffuse_texture_ch2};
        Base::InTextures intextures_ch3 = {diffuse_texture_ch3};
        Base::InTextures intextures_ch4 = {diffuse_texture_ch4};

        Base::OutTextures outtextures{image_texture, depth_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture};

        RendererBaseHLS<DiffRendererHLS, Base>::RenderTiledInChannels(
            viewport,
            mesh,
            uniforms,
            intextures_ch1,
            intextures_ch2,
            intextures_ch3,
            intextures_ch4,
            outtextures);
    }

    template <typename OutTextures, typename Uniforms, typename Fragment>
    static void sync_outtextures(OutTextures &textures, const BoundingBox<int> &tex_bb, const Fragment *fragment_buffer, Uniforms uniforms)
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

                ImageType image = fragment_buffer[address].image;
                DepthType depth = fragment_buffer[address].depth;
                linalg::Vec3<DType> jtra = fragment_buffer[address].jtra;
                linalg::Vec3<DType> jrot = fragment_buffer[address].jrot;
                linalg::Vec3<DType> jmap = fragment_buffer[address].jmap;
                linalg::Vec3<IdType> pids = fragment_buffer[address].pids;

                textures.image_texture.set_texel_(image, y, x, uniforms.out_lvl);
                textures.depth_texture.set_texel_(depth, y, x, uniforms.out_lvl);
                textures.jtra_texture.set_texel_(jtra, y, x, uniforms.out_lvl);
                textures.jrot_texture.set_texel_(jrot, y, x, uniforms.out_lvl);
                textures.jmap_texture.set_texel_(jmap, y, x, uniforms.out_lvl);
                textures.pids_texture.set_texel_(pids, y, x, uniforms.out_lvl);
            }
        }
    }

private:
};
