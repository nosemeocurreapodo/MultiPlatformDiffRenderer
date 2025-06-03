#pragma once

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
                         fpga::Scalar fx, fpga::Scalar fy, fpga::Scalar cx, fpga::Scalar cy);
}