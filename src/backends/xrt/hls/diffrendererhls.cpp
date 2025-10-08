#include "backends/xrt/hls/depthrendererhls.h"
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/meshhls.h"
#include "backends/xrt/hls/rendererhls.h"
#include "core/camera.h"

extern "C"
{
    void DiffRenderHLS(const Scalar *pos_buffer_data,
                       const Scalar *tex_buffer_data,
                       const Scalar *wei_buffer_data,
                       const UInt *ebo_buffer_data,
                       Scalar *f_texture_data,
                       Scalar *image_texture_data,
                       Scalar *depth_texture_data,
                       Vec3 *jtra_texture_data,
                       Vec3 *jrot_texture_data,
                       Vec3 *jmap_texture_data,
                       Vec3 *pids_texture_data,
                       UInt pos_buffer_size,
                       UInt tex_buffer_size,
                       UInt wei_buffer_size,
                       UInt ebo_buffer_size,
                       UInt f_texture_width,
                       UInt f_texture_height,
                       Scalar f_nodata_value,
                       UInt in_lvl,
                       UInt out_texture_width,
                       UInt out_texture_height,
                       Scalar image_nodata_value,
                       Scalar depth_nodata_value,
                       Vec3 jtra_nodata_value,
                       Vec3 jrot_nodata_value,
                       Vec3 jmap_nodata_value,
                       Vec3 pids_nodata_value,
                       UInt out_lvl,
                       Scalar q_x, Scalar q_y, Scalar q_z, Scalar q_w,
                       Scalar t_x, Scalar t_y, Scalar t_z,
                       Scalar fx, Scalar fy, Scalar cx, Scalar cy)
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

        SE3 pose(SO3(Quaternion(q_w, q_x, q_y, q_z)), Vec3(t_x, t_y, t_z));
        Camera cam(fx, fy, cx, cy);

        MeshHLS mesh(pos_buffer_data, pos_buffer_size,
                     tex_buffer_data, tex_buffer_size,
                     wei_buffer_data, wei_buffer_size,
                     ebo_buffer_data, ebo_buffer_size);

        TextureRAM<Scalar> f_texture(f_texture_width, f_texture_height, f_nodata_value, f_texture_data);
        TextureRAM<Scalar> image_texture(out_texture_width, out_texture_height, jtra_nodata_value, image_texture_data);
        TextureRAM<Scalar> depth_texture(out_texture_width, out_texture_height, jtra_nodata_value, depth_texture_data);
        TextureRAM<Vec3> jtra_texture(out_texture_width, out_texture_height, jtra_nodata_value, jtra_texture_data);
        TextureRAM<Vec3> jrot_texture(out_texture_width, out_texture_height, jrot_nodata_value, jrot_texture_data);
        TextureRAM<Vec3> jmap_texture(out_texture_width, out_texture_height, jmap_nodata_value, jmap_texture_data);
        TextureRAM<Vec3> pids_texture(out_texture_width, out_texture_height, pids_nodata_value, pids_texture_data);

        DiffRendererRAM renderer;
        renderer.Render(mesh, pose, cam, in_lvl, out_lvl, f_texture, image_texture, depth_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture);
    }
}