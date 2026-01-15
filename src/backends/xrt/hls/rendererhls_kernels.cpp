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

    void JPoseExpMapRenderHLS(float *vertex_buffer_data,
                                int *ebo_buffer_data,
                                ap_uint<8> *kf_texture_data,
                                ap_uint<8> *f_texture_data,
                                ap_uint<8> *dkfdxy_texture_data,
                                ap_uint<8> *jtra_texture_data,
                                ap_uint<8> *jrot_texture_data,
                                ap_uint<8> *jexp_texture_data,
                                ap_uint<8> *jmap_texture_data,
                                ap_uint<8> *pids_texture_data,   
                                ap_uint<8> *r_texture_data,
                                unsigned int vertex_buffer_size,
                                unsigned int ebo_buffer_size,
                                unsigned int in_texture_width,
                                unsigned int in_texture_height,
                                unsigned int in_lvl,
                                unsigned int out_texture_width,
                                unsigned int out_texture_height,
                                unsigned int out_lvl,
                                ImageType kf_nodata_value,
                                ImageType f_nodata_value,
                                float q_x, float q_y, float q_z, float q_w,
                                float t_x, float t_y, float t_z,
                                float fx, float fy, float cx, float cy,
                                float exp_a, float exp_b)
    {
#pragma HLS INTERFACE m_axi port = vertex_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = kf_texture_data bundle = gmem1 depth = 412800
#pragma HLS INTERFACE m_axi port = f_texture_data bundle = gmem2 depth = 412800
#pragma HLS INTERFACE m_axi port = dkfdxy_texture_data bundle = gmem3 depth = 412800
#pragma HLS INTERFACE m_axi port = jtra_texture_data bundle = gmem4 depth = 412800
#pragma HLS INTERFACE m_axi port = jrot_texture_data bundle = gmem5 depth = 412800
#pragma HLS INTERFACE m_axi port = jexp_texture_data bundle = gmem5 depth = 412800
#pragma HLS INTERFACE m_axi port = jmap_texture_data bundle = gmem5 depth = 412800
#pragma HLS INTERFACE m_axi port = pids_texture_data bundle = gmem5 depth = 412800
#pragma HLS INTERFACE m_axi port = r_texture_data bundle = gmem5 depth = 412800
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

        TextureRAM<ImageType> kf_texture(in_texture_width, in_texture_height, kf_nodata_value, kf_texture_data);
        TextureRAM<ImageType> f_texture(in_texture_width, in_texture_height, f_nodata_value, f_texture_data);
        TextureRAM<linalg::Vec3<float>> dkfdxy_texture(in_texture_width, in_texture_height, linalg::Vec3<float>(0, 0, 0), dkfdxy_texture_data);
        TextureRAM<linalg::Vec3<float>> jtra_texture(out_texture_width, out_texture_height, linalg::Vec3<float>(0, 0, 0), jtra_texture_data);
        TextureRAM<linalg::Vec3<float>> jrot_texture(out_texture_width, out_texture_height, linalg::Vec3<float>(0, 0, 0), jrot_texture_data);
        TextureRAM<linalg::Vec3<float>> jexp_texture(out_texture_width, out_texture_height, linalg::Vec3<float>(0, 0, 0), jexp_texture_data);
        TextureRAM<linalg::Vec3<float>> jmap_texture(out_texture_width, out_texture_height, linalg::Vec3<float>(0, 0, 0), jmap_texture_data);
        TextureRAM<linalg::Vec3<float>> pids_texture(out_texture_width, out_texture_height, linalg::Vec3<float>(0, 0, 0), pids_texture_data);
        TextureRAM<float> r_texture(out_texture_width, out_texture_height, 0, r_texture_data);

        JPoseExpMapRendererHLS renderer;
        renderer.Render(mesh, pose, exposure, cam, in_lvl, out_lvl,
                        kf_texture, f_texture, dkfdxy_texture,
                        jtra_texture, jrot_texture, jexp_texture, jmap_texture, pids_texture, r_texture);
    }
};