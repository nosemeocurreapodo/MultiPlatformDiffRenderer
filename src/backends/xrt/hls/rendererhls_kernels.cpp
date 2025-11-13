#include "backends/xrt/hls/math_common.h"
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/meshhls.h"
#include "backends/xrt/hls/rendererhls.h"
#include "core/camera.h"

extern "C"
{
    void DepthRenderHLS(float *vertex_buffer_data,
                        unsigned int *ebo_buffer_data,
                        float *out_texture_data_ch1,
                        float *out_texture_data_ch2,
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
#pragma HLS INTERFACE m_axi port = out_texture_data_ch1 bundle = gmem1 depth = 412800
#pragma HLS INTERFACE m_axi port = out_texture_data_ch2 bundle = gmem1 depth = 412800
        // #pragma HLS INTERFACE m_axi port = out_texture_data offset = slave bundle = gmem1 max_read_burst_length = 256 max_write_burst_length = 256 depth = 412800

        linalg::SE3<RealType> pose(linalg::SO3<RealType>(linalg::Quaternion<RealType>(q_w, q_x, q_y, q_z)), linalg::Vec3<RealType>(t_x, t_y, t_z));
        Camera<RealType> cam(fx, fy, cx, cy);

        // copy data to bram
        MeshHLS mesh(vertex_buffer_data, vertex_buffer_size,
                     ebo_buffer_data, ebo_buffer_size);

        // data too large, has to be in ram
        TextureRAM<float> out_texture_ch1(out_texture_width, out_texture_height, out_nodata_value, out_texture_data_ch1);
        TextureRAM<float> out_texture_ch2(out_texture_width, out_texture_height, out_nodata_value, out_texture_data_ch2);

        DepthRendererHLS renderer;
        renderer.RenderTiled(mesh, pose, cam, out_lvl, out_texture_ch1);
    }

    void ImageRenderHLS(float *vertex_buffer_data,
                        unsigned int *ebo_buffer_data,
                        ap_uint<8> *diffuse_texture_data_ch1,
                        ap_uint<8> *diffuse_texture_data_ch2,
                        ap_uint<8> *diffuse_texture_data_ch3,
                        ap_uint<8> *diffuse_texture_data_ch4,
                        ap_uint<8> *out_texture_data,
                        unsigned int vertex_buffer_size,
                        unsigned int ebo_buffer_size,
                        unsigned int diffuse_texture_width,
                        unsigned int diffuse_texture_height,
                        unsigned char diffuse_nodata_value,
                        unsigned int diffuse_lvl,
                        unsigned int out_texture_width,
                        unsigned int out_texture_height,
                        unsigned char out_nodata_value,
                        unsigned int out_lvl,
                        float q_x, float q_y, float q_z, float q_w,
                        float t_x, float t_y, float t_z,
                        float fx, float fy, float cx, float cy)
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
        Camera<RealType> cam(fx, fy, cx, cy);

        MeshHLS mesh(vertex_buffer_data, vertex_buffer_size,
                     ebo_buffer_data, ebo_buffer_size);

        TextureRAM<unsigned char> diffuse_texture_ch1(diffuse_texture_width, diffuse_texture_height, diffuse_nodata_value, diffuse_texture_data_ch1);
        TextureRAM<unsigned char> diffuse_texture_ch2(diffuse_texture_width, diffuse_texture_height, diffuse_nodata_value, diffuse_texture_data_ch2);
        TextureRAM<unsigned char> diffuse_texture_ch3(diffuse_texture_width, diffuse_texture_height, diffuse_nodata_value, diffuse_texture_data_ch3);
        TextureRAM<unsigned char> diffuse_texture_ch4(diffuse_texture_width, diffuse_texture_height, diffuse_nodata_value, diffuse_texture_data_ch4);
        TextureRAM<unsigned char> out_texture(out_texture_width, out_texture_height, out_nodata_value, out_texture_data);

        ImageRendererHLS renderer;
        // renderer.RenderNaive(mesh, pose, cam, diffuse_lvl, out_lvl, diffuse_texture_ch1, out_texture);
        // renderer.RenderTiled(mesh, pose, cam, diffuse_lvl, out_lvl, diffuse_texture_ch1, out_texture);
        renderer.RenderTiledInChannels(mesh, pose, cam, diffuse_lvl, out_lvl, diffuse_texture_ch1, diffuse_texture_ch2, diffuse_texture_ch3, diffuse_texture_ch4, out_texture);
    }

    void DiffRenderHLS(float *vertex_buffer_data,
                       unsigned int *ebo_buffer_data,
                       ap_uint<8> *f_texture_data_ch1,
                       ap_uint<8> *f_texture_data_ch2,
                       ap_uint<8> *f_texture_data_ch3,
                       ap_uint<8> *f_texture_data_ch4,
                       ap_uint<8> *image_texture_data,
                       float *depth_texture_data,
                       linalg::Vec3<float> *jtra_texture_data,
                       linalg::Vec3<float> *jrot_texture_data,
                       linalg::Vec3<float> *jmap_texture_data,
                       linalg::Vec3<unsigned int> *pids_texture_data,
                       unsigned int vertex_buffer_size,
                       unsigned int ebo_buffer_size,
                       unsigned int f_texture_width,
                       unsigned int f_texture_height,
                       unsigned char f_nodata_value,
                       unsigned int in_lvl,
                       unsigned int out_texture_width,
                       unsigned int out_texture_height,
                       unsigned char image_nodata_value,
                       float depth_nodata_value,
                       linalg::Vec3<float> jtra_nodata_value,
                       linalg::Vec3<float> jrot_nodata_value,
                       linalg::Vec3<float> jmap_nodata_value,
                       linalg::Vec3<int> pids_nodata_value,
                       unsigned int out_lvl,
                       float q_x, float q_y, float q_z, float q_w,
                       float t_x, float t_y, float t_z,
                       float fx, float fy, float cx, float cy)
    {
#pragma HLS INTERFACE m_axi port = vertex_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = f_texture_data_ch1 bundle = gmem1
#pragma HLS INTERFACE m_axi port = f_texture_data_ch2 bundle = gmem2
#pragma HLS INTERFACE m_axi port = f_texture_data_ch3 bundle = gmem3
#pragma HLS INTERFACE m_axi port = f_texture_data_ch4 bundle = gmem4
#pragma HLS INTERFACE m_axi port = image_texture_data bundle = gmem5
#pragma HLS INTERFACE m_axi port = depth_texture_data bundle = gmem6
#pragma HLS INTERFACE m_axi port = jtra_texture_data bundle = gmem7
#pragma HLS INTERFACE m_axi port = jrot_texture_data bundle = gmem8
#pragma HLS INTERFACE m_axi port = jmap_texture_data bundle = gmem9
#pragma HLS INTERFACE m_axi port = pids_texture_data bundle = gmem10

        linalg::SE3<RealType> pose(linalg::SO3<RealType>(linalg::Quaternion<RealType>(q_w, q_x, q_y, q_z)),
                                   linalg::Vec3<RealType>(t_x, t_y, t_z));
        Camera<RealType> cam(fx, fy, cx, cy);

        MeshHLS mesh(vertex_buffer_data, vertex_buffer_size,
                     ebo_buffer_data, ebo_buffer_size);

        TextureRAM<unsigned char> f_texture_ch1(f_texture_width, f_texture_height, f_nodata_value, f_texture_data_ch1);
        TextureRAM<unsigned char> f_texture_ch2(f_texture_width, f_texture_height, f_nodata_value, f_texture_data_ch2);
        TextureRAM<unsigned char> f_texture_ch3(f_texture_width, f_texture_height, f_nodata_value, f_texture_data_ch3);
        TextureRAM<unsigned char> f_texture_ch4(f_texture_width, f_texture_height, f_nodata_value, f_texture_data_ch4);

        TextureRAM<unsigned char> image_texture(out_texture_width, out_texture_height, image_nodata_value, image_texture_data);
        TextureRAM<float> depth_texture(out_texture_width, out_texture_height, depth_nodata_value, depth_texture_data);
        TextureRAM<linalg::Vec3<float>> jtra_texture(out_texture_width, out_texture_height, jtra_nodata_value, jtra_texture_data);
        TextureRAM<linalg::Vec3<float>> jrot_texture(out_texture_width, out_texture_height, jrot_nodata_value, jrot_texture_data);
        TextureRAM<linalg::Vec3<float>> jmap_texture(out_texture_width, out_texture_height, jmap_nodata_value, jmap_texture_data);
        TextureRAM<linalg::Vec3<int>> pids_texture(out_texture_width, out_texture_height, pids_nodata_value, pids_texture_data);

        DiffRendererHLS renderer;
        renderer.RenderTiledInChannels(mesh, pose, cam, in_lvl, out_lvl,
                                       f_texture_ch1, f_texture_ch2, f_texture_ch3, f_texture_ch4,
                                       image_texture, depth_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture);
    }
};