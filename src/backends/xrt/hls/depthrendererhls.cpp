#include "backends/xrt/hls/depthrendererhls.h"
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/meshhls.h"
#include "backends/xrt/hls/rendererhls.h"
#include "core/types.h"
#include "core/camera.h"

extern "C"
{
    void DepthRenderHLS(const Scalar *pos_buffer_data,
                        const Scalar *tex_buffer_data,
                        const Scalar *wei_buffer_data,
                        const UInt *ebo_buffer_data,
                        Scalar *out_texture_data,
                        UInt pos_buffer_size,
                        UInt tex_buffer_size,
                        UInt wei_buffer_size,
                        UInt ebo_buffer_size,
                        UInt out_texture_width,
                        UInt out_texture_height,
                        Scalar out_nodata_value,
                        UInt out_lvl,
                        Scalar q_x, Scalar q_y, Scalar q_z, Scalar q_w,
                        Scalar t_x, Scalar t_y, Scalar t_z,
                        Scalar fx, Scalar fy, Scalar cx, Scalar cy)
    {
#pragma HLS INTERFACE m_axi port = pos_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = tex_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = wei_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = out_texture_data bundle = gmem0

        SE3 pose(SO3(Quaternion(q_w, q_x, q_y, q_z)), Vec3(t_x, t_y, t_z));
        Camera cam(fx, fy, cx, cy);

        // copy data to bram
        MeshHLS mesh(pos_buffer_data, pos_buffer_size,
                     tex_buffer_data, tex_buffer_size,
                     wei_buffer_data, wei_buffer_size,
                     ebo_buffer_data, ebo_buffer_size);

        // data too large, has to be in ram
        TextureRAM<Scalar> out_texture(out_texture_width, out_texture_height, out_nodata_value, out_texture_data);

        DepthRendererRAM renderer;
        //DepthRendererBRAM renderer;
        renderer.Render(mesh, pose, cam, out_lvl, out_texture);
    }
}