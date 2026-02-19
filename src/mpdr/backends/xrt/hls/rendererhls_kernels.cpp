#include "backends/xrt/hls/math_common.h"
#include "backends/xrt/hls/bufferhls.h"
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/meshhls.h"
#include "backends/xrt/hls/rendererhls.h"
#include "backends/xrt/hls/deferredrendererhls.h"
#include "core/camera.h"

extern "C"
{
    void DepthRenderHLS(float *vertex_buffer_data,
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
#pragma HLS INTERFACE m_axi port = vertex_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = out_texture_data bundle = gmem1 depth = 412800
        // #pragma HLS INTERFACE m_axi port = out_texture_data offset = slave bundle = gmem1 max_read_burst_length = 256 max_write_burst_length = 256 depth = 412800

        DepthRendererHLS renderer;
        renderer.Render(vertex_buffer_data,
                        ebo_buffer_data,
                        out_texture_data,
                        out_texture_offset,
                        vertex_buffer_size,
                        ebo_buffer_size,
                        out_texture_width,
                        out_texture_height,
                        out_nodata_value,
                        q_x, q_y, q_z, q_w,
                        t_x, t_y, t_z,
                        fx, fy, cx, cy);
    }

    void ImageRenderHLS(float *vertex_buffer_data,
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
#pragma HLS INTERFACE m_axi port = vertex_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = diffuse_texture_data bundle = gmem1 depth = 412800
#pragma HLS INTERFACE m_axi port = out_texture_data bundle = gmem5 depth = 412800
        // #pragma HLS INTERFACE m_axi port = out_texture_data offset = slave bundle = gmem2 max_read_burst_length = 256 max_write_burst_length = 256 depth = 412800

        ImageDRendererHLS renderer;
        renderer.Render(vertex_buffer_data,
                        ebo_buffer_data,
                        diffuse_texture_data,
                        out_texture_data,
                        diffuse_texture_offset,
                        out_texture_offset,
                        vertex_buffer_size,
                        ebo_buffer_size,
                        diffuse_texture_width,
                        diffuse_texture_height,
                        diffuse_nodata_value,
                        out_texture_width,
                        out_texture_height,
                        out_nodata_value,
                        q_x, q_y, q_z, q_w,
                        t_x, t_y, t_z,
                        fx, fy, cx, cy,
                        exp_a, exp_b);
    }

    void DIDxyRenderHLS(float *vertex_buffer_data,
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
#pragma HLS INTERFACE m_axi port = vertex_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem1 depth = 412800
#pragma HLS INTERFACE m_axi port = in_texture_data bundle = gmem2 depth = 412800
#pragma HLS INTERFACE m_axi port = out_texture_data bundle = gmem3 depth = 412800
        // #pragma HLS INTERFACE m_axi port = out_texture_data offset = slave bundle = gmem2 max_read_burst_length = 256 max_write_burst_length = 256 depth = 412800

        // #pragma HLS cache port = diffuse_texture_data_ch1 lines = 64 depth = 64
        // #pragma HLS cache port = diffuse_texture_data_ch2 lines = 64 depth = 64

        DIDxyRendererHLS renderer;
        renderer.Render(vertex_buffer_data,
                        ebo_buffer_data,
                        in_texture_data,
                        out_texture_data,
                        in_texture_offset,
                        out_texture_offset,
                        vertex_buffer_size,
                        ebo_buffer_size,
                        in_texture_width,
                        in_texture_height,
                        in_nodata_value,
                        out_texture_width,
                        out_texture_height,
                        out_nodata_value_x,
                        out_nodata_value_y,
                        out_nodata_value_z);
    }

    void DiffRenderHLS(float *vertex_buffer_data,
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
#pragma HLS INTERFACE m_axi port = vertex_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0 depth = 412800
#pragma HLS INTERFACE m_axi port = kf_texture_data bundle = gmem1 depth = 412800
#pragma HLS INTERFACE m_axi port = dkfdxy_texture_data bundle = gmem2 depth = 412800
#pragma HLS INTERFACE m_axi port = image_texture_data bundle = gmem3 depth = 412800
#pragma HLS INTERFACE m_axi port = jtra_texture_data bundle = gmem4 depth = 412800
#pragma HLS INTERFACE m_axi port = jrot_texture_data bundle = gmem5 depth = 412800
#pragma HLS INTERFACE m_axi port = jexp_texture_data bundle = gmem6 depth = 412800
#pragma HLS INTERFACE m_axi port = jmap_texture_data bundle = gmem7 depth = 412800
#pragma HLS INTERFACE m_axi port = pids_texture_data bundle = gmem8 depth = 412800

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

        DiffRendererHLS renderer;
        renderer.Render(vertex_buffer_data,
                       ebo_buffer_data,
                       kf_texture_data,
                       dkfdxy_texture_data,
                       image_texture_data,
                       jtra_texture_data,
                       jrot_texture_data,
                       jexp_texture_data,
                       jmap_texture_data,
                       pids_texture_data,
                       in_texture_offset,
                       out_texture_offset,
                       vertex_buffer_size,
                       ebo_buffer_size,
                       in_texture_width,
                       in_texture_height,
                       out_texture_width,
                       out_texture_height,
                       q_x, q_y, q_z, q_w,
                       t_x, t_y, t_z,
                       fx, fy, cx, cy,
                       exp_a, exp_b);
    }
};