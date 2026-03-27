#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
// #include "core/types.h"
#include "mpdr/backends/xrt/hls/math_common.h"
#include "linalg/linalg.h"
#include "mpdr/common/camera.h"
#include "mpdr/common/boundingbox.h"
#include "mpdr/common/render_constants.h"
#include "mpdr/backends/base/rendererbase.h"
#include "mpdr/backends/base/deferredrendererbase.h"
#include "mpdr/backends/base/renderersbase.h"
#include "mpdr/backends/xrt/hls/texturehls.h"
#include "mpdr/backends/xrt/hls/bufferhls.h"
#include "mpdr/backends/xrt/hls/meshhls.h"

#include "hls_task.h"

template <class Base>
class DeferredRendererBaseHLS
    : public DeferredBase<TextureViewWriteHLS, Base>
{
public:
    // RendererBaseHLS() = default;
    //~RendererBaseHLS() = default;

    using DeferredRendererBase = DeferredRendererBase<TextureViewWriteHLS>;

    using Triangle = Triangle<DeferredRendererBase>;

    using DBase = DeferredBase<TextureViewWriteHLS, Base>;
    using DFragment = typename DBase::DFragment;
    using DUniforms = typename DBase::DUniforms;

    using BFragment = typename Base::Fragment;
    using BUniforms = typename Base::Uniforms;
    using BInTextures = typename Base::InTextures;
    using BOutTextures = typename Base::OutTextures;

    void RenderNaive(const BoundingBox<IntType> &viewport,
                     const BufferViewReadHLS<float> &vertex_buffer,
                     const BufferViewReadHLS<int> &ebo_buffer,
                     const BUniforms &uniforms,
                     const BInTextures &intextures,
                     BOutTextures &outtextures)
    {
        // MathType depth_buffer[viewport.width_ * viewport.height_] = {MathType(-1)};
        DFragment dfragment_buffer[MAX_WIDTH * MAX_HEIGHT];
        BFragment bfragment_buffer[MAX_WIDTH * MAX_HEIGHT];
        RealType depth_buffer[MAX_WIDTH * MAX_HEIGHT];
#pragma HLS BIND_STORAGE variable = dfragment_buffer type = ram_t2p impl = uram
#pragma HLS BIND_STORAGE variable = bfragment_buffer type = ram_t2p impl = uram
#pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = uram

    renderbase_reset_loop_y:
        for (int y = viewport.min_y_; y < viewport.max_y_; y++)
        {
#pragma HLS loop_tripcount min = MAX_HEIGHT max = MAX_HEIGHT avg = MAX_HEIGHT

            int base_address = y * viewport.width_;

        renderbase_reset_loop_x:
            for (int x = viewport.min_x_; x < viewport.max_x_; x++)
            {
#pragma HLS loop_tripcount min = MAX_WIDTH max = MAX_WIDTH avg = MAX_WIDTH

                int address = base_address + x;
                // dfragment_buffer[address] =
                bfragment_buffer[address] = Base::fragment_nodata(outtextures);
                depth_buffer[address] = RealType(-1);
            }
        }

        this->RenderTileDeferred(viewport,
                                 viewport,
                                 vertex_buffer,
                                 ebo_buffer,
                                 uniforms,
                                 intextures,
                                 outtextures,
                                 dfragment_buffer,
                                 bfragment_buffer,
                                 depth_buffer);
    }

    void RenderTiledFragBuff2(const BoundingBox<IntType> &viewport,
                              const BufferViewReadHLS<float> &vertex_buffer,
                              const BufferViewReadHLS<int> &ebo_buffer,
                              const BUniforms &uniforms,
                              const BInTextures &intextures,
                              BOutTextures &outtextures)
    {
        IntType tile_width = IntType(ceil(RealType(viewport.width_) / NUM_TILES_X));
        IntType tile_height = IntType(ceil(RealType(viewport.height_) / NUM_TILES_Y));
        // IntType num_tiles = num_tiles_x * num_tiles_y;

        Triangle triangle_buffer[MAX_TRI_PER_TILE];
        DFragment dfragment_buffer[MAX_TILE_WIDTH * MAX_TILE_HEIGHT];
        BFragment bfragment_buffer[MAX_TILE_WIDTH * MAX_TILE_HEIGHT];
        RealType depth_buffer[MAX_TILE_WIDTH * MAX_TILE_HEIGHT];

        DUniforms duniforms;
        duniforms.pose_matrix = uniforms.pose_matrix;
        duniforms.view_matrix = uniforms.view_matrix;

// #pragma HLS BIND_STORAGE variable = triangle_buffer type = ram_t2p impl = bram
// #pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = uram
// #pragma HLS BIND_STORAGE variable = depth_buffer type = ram_t2p impl = bram
//  #pragma HLS array_partition variable = depth_buffer type = cyclic factor = tile_width * 2
//  #pragma HLS array_partition variable = depth_buffer type = complete

// #pragma HLS aggregate variable = fragment_buffer compact = bit
#pragma HLS BIND_STORAGE variable = dfragment_buffer type = ram_t2p impl = uram
#pragma HLS BIND_STORAGE variable = bfragment_buffer type = ram_t2p impl = uram
        // #pragma HLS array_partition variable = fragment_buffer type = cyclic factor = 1 dim = 0

    renderbase_render_tiles_loop:
        for (int tile_y = 0; tile_y < NUM_TILES_Y; tile_y++)
        {
            // #pragma HLS loop_tripcount min = max_tiles_y max = max_tiles_y avg = max_tiles_y

        renderbase_render_tiles_x_loop:
            for (int tile_x = 0; tile_x < NUM_TILES_X; tile_x++)
            {
                // #pragma HLS loop_tripcount min = max_tiles_x max = max_tiles_x avg = max_tiles_x

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

                renderbase_reset_depth_buffer_loop_x:
                    for (int x = 0; x < tile_width; x++)
                    {
#pragma HLS loop_tripcount min = MAX_TILE_WIDTH max = MAX_TILE_WIDTH avg = MAX_TILE_WIDTH

                        int address = x + base_address;
                        dfragment_buffer[address].fpos = Vec3<float>(0.0, 0.0, 0.0);
                        dfragment_buffer[address].kfpos = Vec3<float>(0.0, 0.0, 0.0);
                        dfragment_buffer[address].bcid = Vec3<float>(0.0, 0.0, 0.0);
                        bfragment_buffer[address] = Base::fragment_nodata(outtextures);
                        depth_buffer[address] = RealType(-1);
                    }
                }

                IntType min_x_ = IntType(RealType(viewport.width_ * tile_x) / RealType(NUM_TILES_X)) + viewport.min_x_;
                IntType max_x_ = IntType(RealType(viewport.width_ * (tile_x + 1)) / RealType(NUM_TILES_X)) + viewport.min_x_;
                IntType min_y_ = IntType(RealType(viewport.height_ * tile_y) / RealType(NUM_TILES_Y)) + viewport.min_y_;
                IntType max_y_ = IntType(RealType(viewport.height_ * (tile_y + 1)) / RealType(NUM_TILES_Y)) + viewport.min_y_;

                BoundingBox<IntType> viewport_buffer = BoundingBox<IntType>(min_x_, max_x_, min_y_, max_y_);
                IntType triangle_count = 0;

                get_triangles<BufferViewReadHLS<float>,
                              BufferViewReadHLS<int>,
                              DeferredRendererBase>(vertex_buffer,
                                                    ebo_buffer,
                                                    viewport,
                                                    viewport_buffer,
                                                    duniforms,
                                                    triangle_buffer,
                                                    MAX_TRI_PER_TILE,
                                                    triangle_count);

                this->RenderTileDeferred(viewport,
                                         viewport_buffer,
                                         vertex_buffer,
                                         ebo_buffer,
                                         uniforms,
                                         intextures,
                                         outtextures,
                                         dfragment_buffer,
                                         bfragment_buffer,
                                         depth_buffer);
            }
        }
    }

private:
};

class DepthDRendererHLS
    : public DeferredRendererBaseHLS<DepthRendererBase<TextureViewWriteHLS>>
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

        uniforms.pose_matrix = pose.matrix();
        uniforms.view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE,
                                                       RenderConstants::FAR_PLANE);
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

        DeferredRendererBaseHLS<Base>::RenderNaive(viewport, vertex_buffer, ebo_buffer, uniforms, intextures, outtextures);
        // DeferredRendererBaseHLS<Base>::RenderTiledFragBuff2(viewport, vertex_buffer, ebo_buffer, uniforms, intextures, outtextures);
    }
};

class ImageDRendererHLS
    : public DeferredRendererBaseHLS<ImageRendererBase<TextureViewReadHLS, TextureViewWriteHLS>>
{
public:
    using Base = ImageRendererBase<TextureViewReadHLS, TextureViewWriteHLS>;

    ImageDRendererHLS() = default;
    ~ImageDRendererHLS() = default;

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

        linalg::Mat4<RealType> projmat = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        linalg::Mat4<RealType> viewmat = projmat;
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

        // RendererBaseHLS<Base>::RenderNaive(
        //     viewport, vertex_buffer, ebo_buffer, uniforms, intextures, outtextures);

        DeferredRendererBaseHLS<Base>::RenderTiledFragBuff2(
            viewport, vertex_buffer, ebo_buffer, uniforms, intextures, outtextures);
    }

private:
};
