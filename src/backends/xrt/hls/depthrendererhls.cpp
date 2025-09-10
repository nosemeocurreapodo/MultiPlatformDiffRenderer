#include "bufferfpga.h"
#include "texturefpga.h"
#include "meshfpga.h"
#include "rendererfpga.h"

extern "C"
{
    void DepthRenderFPGA(Scalar *pos_buffer_data,
                         Scalar *tex_buffer_data,
                         Scalar *wei_buffer_data,
                         UInt *ebo_buffer_data,
                         ImageType *in_texture_data,
                         Scalar *out_texture_data,
                         UInt pos_buffer_size,
                         UInt tex_buffer_size,
                         UInt wei_buffer_size,
                         UInt ebo_buffer_size,
                         UInt in_texture_width,
                         UInt in_texture_height,
                         UInt in_texture_channels,
                         ImageType in_nodata_value,
                         UInt out_texture_width,
                         UInt out_texture_height,
                         UInt out_texture_channels,
                         Scalar out_nodata_value,
                         Scalar q_x, Scalar q_y, Scalar q_z, Scalar q_w,
                         Scalar t_x, Scalar t_y, Scalar t_z,
                         Scalar fx, Scalar fy, Scalar cx, Scalar cy)
    {
#pragma HLS INTERFACE m_axi port = pos_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = tex_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = wei_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = in_texture_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = out_texture_data bundle = gmem0

        BufferFPGA<fpga::Scalar> pos_buffer(pos_buffer_size, pos_buffer_data);
        BufferFPGA<fpga::Scalar> tex_buffer(tex_buffer_size, tex_buffer_data);
        BufferFPGA<fpga::Scalar> wei_buffer(wei_buffer_size, wei_buffer_data);
        BufferFPGA<fpga::UInt> ebo_buffer(ebo_buffer_size, ebo_buffer_data);
        MeshFPGA mesh(pos_buffer, tex_buffer, wei_buffer, ebo_buffer);
        TextureFPGA<fpga::ImageType> in_texture(in_texture_width, in_texture_height, in_texture_channels, in_nodata_value, in_texture_data);
        TextureFPGA<fpga::Scalar> out_texture(out_texture_width, out_texture_height, out_texture_channels, out_nodata_value, out_texture_data);
        fpga::SE3 pose(fpga::SO3(fpga::Quaternion(q_w, q_x, q_y, q_z)), fpga::Vec3(t_x, t_y, t_z));
        fpga::Camera cam(fx, fy, cx, cy);

        DepthRendererFPGA renderer;
        renderer.Render(mesh, pose, cam, in_texture, out_texture, 0);
    }
}