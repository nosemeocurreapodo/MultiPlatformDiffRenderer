#pragma once

extern "C"
{
    void DepthRenderFPGA(fpga::Scalar *pos_buffer_data,
                         fpga::Scalar *tex_buffer_data,
                         fpga::Scalar *wei_buffer_data,
                         fpga::UInt *ebo_buffer_data,
                         fpga::ImageType *in_texture_data,
                         fpga::Scalar *out_texture_data,
                         fpga::UInt pos_buffer_size,
                         fpga::UInt tex_buffer_size,
                         fpga::UInt wei_buffer_size,
                         fpga::UInt ebo_buffer_size,
                         fpga::UInt in_texture_width,
                         fpga::UInt in_texture_height,
                         fpga::UInt in_texture_channels,
                         fpga::ImageType in_nodata_value,
                         fpga::UInt out_texture_width,
                         fpga::UInt out_texture_height,
                         fpga::UInt out_texture_channels,
                         fpga::Scalar out_nodata_value,
                         fpga::Scalar q_x, fpga::Scalar q_y, fpga::Scalar q_z, fpga::Scalar q_w,
                         fpga::Scalar t_x, fpga::Scalar t_y, fpga::Scalar t_z,
                         fpga::Scalar fx, fpga::Scalar fy, fpga::Scalar cx, fpga::Scalar cy);
}