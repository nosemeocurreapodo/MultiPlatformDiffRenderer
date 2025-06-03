#include "typesfpga.h"
#include "bufferfpga.h"
#include "texturefpga.h"
#include "meshfpga.h"
#include "rendererfpga.h"

extern "C"
{
    void DepthRenderFPGA(fpga::Scalar *pos_buffer_data,
                         fpga::Scalar *tex_buffer_data,
                         fpga::Scalar *wei_buffer_data,
                         unsigned int *ebo_buffer_data,
                         fpga::ImageType *in_texture_data,
                         fpga::Scalar *out_texture_data,
                         unsigned int pos_buffer_size,
                         unsigned int tex_buffer_size,
                         unsigned int wei_buffer_size,
                         unsigned int ebo_buffer_size,
                         unsigned int in_texture_width,
                         unsigned int in_texture_height,
                         unsigned int in_texture_channels,
                         fpga::ImageType in_nodata_value,
                         unsigned int out_texture_width,
                         unsigned int out_texture_height,
                         unsigned int out_texture_channels,
                         fpga::Scalar out_nodata_value,
                         fpga::Scalar q_x, fpga::Scalar q_y, fpga::Scalar q_z, fpga::Scalar q_w,
                         fpga::Scalar t_x, fpga::Scalar t_y, fpga::Scalar t_z,
                         fpga::Scalar fx, fpga::Scalar fy, fpga::Scalar cx, fpga::Scalar cy)
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
        BufferFPGA<unsigned int> ebo_buffer(ebo_buffer_size, ebo_buffer_data);
        MeshFPGA mesh(pos_buffer, tex_buffer, wei_buffer, ebo_buffer);
        TextureFPGA<fpga::ImageType> in_texture(in_texture_width, in_texture_height, in_texture_channels, in_nodata_value, in_texture_data);
        TextureFPGA<fpga::Scalar> out_texture(out_texture_width, out_texture_height, out_texture_channels, out_nodata_value, out_texture_data);
        fpga::SE3 pose(fpga::SO3(fpga::Quaternion(q_w, q_x, q_y, q_z)), fpga::Vec3(t_x, t_y, t_z));
        fpga::Camera cam(fx, fy, cx, cy);

        DepthRendererFPGA renderer;
        renderer.Render(mesh, pose, cam, in_texture, out_texture, 0);
    }
}