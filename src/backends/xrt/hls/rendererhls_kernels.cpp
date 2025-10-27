#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/meshhls.h"
#include "backends/xrt/hls/rendererhls.h"
#include "core/camera.h"

extern "C"
{

    void DepthRenderHLS(float *vertex_buffer_data,
                        unsigned int *ebo_buffer_data,
                        float *diffuse_texture_data,
                        float *out_texture_data,
                        unsigned int vertex_buffer_size,
                        unsigned int ebo_buffer_size,
                        unsigned int diffuse_texture_width,
                        unsigned int diffuse_texture_height,
                        float diffuse_nodata_value,
                        unsigned int diffuse_lvl,
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

        linalg::SE3<MathType> pose(linalg::SO3<MathType>(linalg::Quaternion<MathType>(q_w, q_x, q_y, q_z)), linalg::Vec3<MathType>(t_x, t_y, t_z));
        Camera<MathType> cam(fx, fy, cx, cy);

        // copy data to bram
        MeshHLS<MeshType, BufferRAM, TextureRAM> mesh(vertex_buffer_data, vertex_buffer_size,
                                                      ebo_buffer_data, ebo_buffer_size,
                                                      diffuse_texture_data, diffuse_texture_width, diffuse_texture_height, diffuse_nodata_value);

        // data too large, has to be in ram
        TextureRAM<float> out_texture(out_texture_width, out_texture_height, out_nodata_value, out_texture_data);

        DepthRendererRAM renderer;
        renderer.Render(mesh, pose, cam, out_lvl, out_texture);
    }

    void ImageRenderHLS(float *vertex_buffer_data,
                        unsigned int *ebo_buffer_data,
                        float *diffuse_texture_data,
                        float *depth_texture_data,
                        float *out_texture_data,
                        unsigned int vertex_buffer_size,
                        unsigned int ebo_buffer_size,
                        unsigned int diffuse_texture_width,
                        unsigned int diffuse_texture_height,
                        float diffuse_nodata_value,
                        unsigned int diffuse_lvl,
                        unsigned int out_texture_width,
                        unsigned int out_texture_height,
                        float out_nodata_value,
                        unsigned int out_lvl,
                        float q_x, float q_y, float q_z, float q_w,
                        float t_x, float t_y, float t_z,
                        float fx, float fy, float cx, float cy)
    {
#pragma HLS INTERFACE m_axi port = vertex_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = diffuse_texture_data bundle = gmem1
#pragma HLS INTERFACE m_axi port = depth_texture_data bundle = gmem2
#pragma HLS INTERFACE m_axi port = out_texture_data bundle = gmem3

        linalg::SE3<MathType> pose(linalg::SO3<MathType>(
                                       linalg::Quaternion<MathType>(q_w, q_x, q_y, q_z)),
                                   linalg::Vec3<MathType>(t_x, t_y, t_z));
        Camera<MathType> cam(fx, fy, cx, cy);

        MeshHLS<MeshType, BufferRAM, TextureRAM> mesh(vertex_buffer_data, vertex_buffer_size,
                                                      ebo_buffer_data, ebo_buffer_size,
                                                      diffuse_texture_data, diffuse_texture_width, diffuse_texture_height, diffuse_nodata_value);

        TextureRAM<float> depth_texture(out_texture_width, out_texture_height, out_nodata_value, depth_texture_data);
        TextureRAM<float> out_texture(out_texture_width, out_texture_height, out_nodata_value, out_texture_data);

        ImageRendererRAM renderer;
        renderer.Render(mesh, pose, cam, diffuse_lvl, out_lvl, depth_texture, out_texture);
    }
    /*
            void DiffRenderHLS(const float *pos_buffer_data,
                               const float *tex_buffer_data,
                               const float *wei_buffer_data,
                               const unsigned int *ebo_buffer_data,
                               float *f_texture_data,
                               float *image_texture_data,
                               float *depth_texture_data,
                               linalg::Vec3<float> *jtra_texture_data,
                               linalg::Vec3<float> *jrot_texture_data,
                               linalg::Vec3<float> *jmap_texture_data,
                               linalg::Vec3<float> *pids_texture_data,
                               unsigned int pos_buffer_size,
                               unsigned int tex_buffer_size,
                               unsigned int wei_buffer_size,
                               unsigned int ebo_buffer_size,
                               unsigned int f_texture_width,
                               unsigned int f_texture_height,
                               float f_nodata_value,
                               unsigned int in_lvl,
                               unsigned int out_texture_width,
                               unsigned int out_texture_height,
                               float image_nodata_value,
                               float depth_nodata_value,
                               linalg::Vec3<float> jtra_nodata_value,
                               linalg::Vec3<float> jrot_nodata_value,
                               linalg::Vec3<float> jmap_nodata_value,
                               linalg::Vec3<float> pids_nodata_value,
                               unsigned int out_lvl,
                               float q_x, float q_y, float q_z, float q_w,
                               float t_x, float t_y, float t_z,
                               float fx, float fy, float cx, float cy)
            {
        #pragma HLS INTERFACE m_axi port = pos_buffer_data bundle = gmem0
        #pragma HLS INTERFACE m_axi port = tex_buffer_data bundle = gmem0
        #pragma HLS INTERFACE m_axi port = wei_buffer_data bundle = gmem0
        #pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0
        #pragma HLS INTERFACE m_axi port = f_texture_data bundle = gmem0
        #pragma HLS INTERFACE m_axi port = image_texture_data bundle = gmem0
        #pragma HLS INTERFACE m_axi port = depth_texture_data bundle = gmem0
        #pragma HLS INTERFACE m_axi port = jtra_texture_data bundle = gmem0
        #pragma HLS INTERFACE m_axi port = jrot_texture_data bundle = gmem0
        #pragma HLS INTERFACE m_axi port = jmap_texture_data bundle = gmem0
        #pragma HLS INTERFACE m_axi port = pids_texture_data bundle = gmem0

                linalg::SE3<MathType> pose(linalg::SO3<MathType>(linalg::Quaternion<MathType>(q_w, q_x, q_y, q_z)), linalg::Vec3<MathType>(t_x, t_y, t_z));
                Camera<MathType> cam(fx, fy, cx, cy);

                MeshHLS<MeshType> mesh(pos_buffer_data, pos_buffer_size,
                                       tex_buffer_data, tex_buffer_size,
                                       wei_buffer_data, wei_buffer_size,
                                       ebo_buffer_data, ebo_buffer_size);

                TextureRAM<float> f_texture(f_texture_width, f_texture_height, f_nodata_value, f_texture_data);
                TextureRAM<float> image_texture(out_texture_width, out_texture_height, image_nodata_value, image_texture_data);
                TextureRAM<float> depth_texture(out_texture_width, out_texture_height, depth_nodata_value, depth_texture_data);
                TextureRAM<linalg::Vec3<float>> jtra_texture(out_texture_width, out_texture_height, jtra_nodata_value, jtra_texture_data);
                TextureRAM<linalg::Vec3<float>> jrot_texture(out_texture_width, out_texture_height, jrot_nodata_value, jrot_texture_data);
                TextureRAM<linalg::Vec3<float>> jmap_texture(out_texture_width, out_texture_height, jmap_nodata_value, jmap_texture_data);
                TextureRAM<linalg::Vec3<float>> pids_texture(out_texture_width, out_texture_height, pids_nodata_value, pids_texture_data);

                DiffRendererRAM renderer;
                renderer.Render(mesh, pose, cam, in_lvl, out_lvl, f_texture, image_texture, depth_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture);
            }
                */
}