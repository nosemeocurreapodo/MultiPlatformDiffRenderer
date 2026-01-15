#include "backends/xrt/hls/math_common.h"
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/meshhls.h"
#include "backends/xrt/hls/rendererhls.h"
#include "core/camera.h"

extern "C"
{
    void DepthRenderHLS(float *vertex_buffer_data,
                        int *ebo_buffer_data,
                        float *out_texture_data,
                        unsigned int vertex_buffer_size,
                        unsigned int ebo_buffer_size,
                        unsigned int out_texture_width,
                        unsigned int out_texture_height,
                        float out_nodata_value,
                        unsigned int out_lvl,
                        float q_x, float q_y, float q_z, float q_w,
                        float t_x, float t_y, float t_z,
                        float fx, float fy, float cx, float cy)
    {
#pragma HLS INTERFACE m_axi port = vertex_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = out_texture_data bundle = gmem1 depth = 412800
        // #pragma HLS INTERFACE m_axi port = out_texture_data offset = slave bundle = gmem1 max_read_burst_length = 256 max_write_burst_length = 256 depth = 412800

        linalg::SE3<RealType> pose(linalg::SO3<RealType>(linalg::Quaternion<RealType>(q_w, q_x, q_y, q_z)), linalg::Vec3<RealType>(t_x, t_y, t_z));
        PinholeCamera<RealType> cam(fx, fy, cx, cy);

        // copy data to bram
        MeshHLS mesh(vertex_buffer_data, vertex_buffer_size,
                     ebo_buffer_data, ebo_buffer_size);

        // data too large, has to be in ram
        TextureRAM<float> out_texture(out_texture_width, out_texture_height, out_nodata_value, out_texture_data);

        DepthRendererHLS renderer;
        renderer.Render(mesh, pose, cam, out_lvl, out_texture);
    }

    void ImageRenderHLS(float *vertex_buffer_data,
                        int *ebo_buffer_data,
                        ap_uint<8> *diffuse_texture_data_ch1,
                        ap_uint<8> *diffuse_texture_data_ch2,
                        ap_uint<8> *diffuse_texture_data_ch3,
                        ap_uint<8> *diffuse_texture_data_ch4,
                        ap_uint<8> *out_texture_data,
                        unsigned int vertex_buffer_size,
                        unsigned int ebo_buffer_size,
                        unsigned int diffuse_texture_width,
                        unsigned int diffuse_texture_height,
                        ImageType diffuse_nodata_value,
                        unsigned int diffuse_lvl,
                        unsigned int out_texture_width,
                        unsigned int out_texture_height,
                        ImageType out_nodata_value,
                        unsigned int out_lvl,
                        float q_x, float q_y, float q_z, float q_w,
                        float t_x, float t_y, float t_z,
                        float fx, float fy, float cx, float cy,
                        float exp_a, float exp_b)
    {
#pragma HLS INTERFACE m_axi port = vertex_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = diffuse_texture_data_ch1 bundle = gmem1 depth = 412800
#pragma HLS INTERFACE m_axi port = diffuse_texture_data_ch2 bundle = gmem2 depth = 412800
#pragma HLS INTERFACE m_axi port = diffuse_texture_data_ch3 bundle = gmem3 depth = 412800
#pragma HLS INTERFACE m_axi port = diffuse_texture_data_ch4 bundle = gmem4 depth = 412800
#pragma HLS INTERFACE m_axi port = out_texture_data bundle = gmem5 depth = 412800
        // #pragma HLS INTERFACE m_axi port = out_texture_data offset = slave bundle = gmem2 max_read_burst_length = 256 max_write_burst_length = 256 depth = 412800

        // #pragma HLS cache port = diffuse_texture_data_ch1 lines = 64 depth = 64
        // #pragma HLS cache port = diffuse_texture_data_ch2 lines = 64 depth = 64

        linalg::SE3<RealType> pose(linalg::SO3<RealType>(
                                       linalg::Quaternion<RealType>(q_w, q_x, q_y, q_z)),
                                   linalg::Vec3<RealType>(t_x, t_y, t_z));
        linalg::Vec2<RealType> exposure(exp_a, exp_b);
        PinholeCamera<RealType> cam(fx, fy, cx, cy);

        MeshHLS mesh(vertex_buffer_data, vertex_buffer_size,
                     ebo_buffer_data, ebo_buffer_size);

        TextureRAM<ImageType> diffuse_texture_ch1(diffuse_texture_width, diffuse_texture_height, diffuse_nodata_value, diffuse_texture_data_ch1);
        TextureRAM<ImageType> diffuse_texture_ch2(diffuse_texture_width, diffuse_texture_height, diffuse_nodata_value, diffuse_texture_data_ch2);
        TextureRAM<ImageType> diffuse_texture_ch3(diffuse_texture_width, diffuse_texture_height, diffuse_nodata_value, diffuse_texture_data_ch3);
        TextureRAM<ImageType> diffuse_texture_ch4(diffuse_texture_width, diffuse_texture_height, diffuse_nodata_value, diffuse_texture_data_ch4);
        TextureRAM<ImageType> out_texture(out_texture_width, out_texture_height, out_nodata_value, out_texture_data);

        ImageRendererHLS renderer;
        renderer.Render(mesh, pose, exposure, cam, diffuse_lvl, out_lvl,
                        diffuse_texture_ch1, diffuse_texture_ch2, diffuse_texture_ch3, diffuse_texture_ch4,
                        out_texture);
    }
};