#pragma once

extern "C"
{
    void DepthRenderFPGA(float *pos_buffer_data,
                         float *tex_buffer_data,
                         float *wei_buffer_data,
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
                         float q_x, float q_y, float q_z, float q_w,
                         float t_x, float t_y, float t_z,
                         float fx, float fy, float cx, float cy);
}