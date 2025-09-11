#pragma once

#include "backends/core/types.h"

extern "C"
{
    void ImageRenderFPGA(Scalar *pos_buffer_data,
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
                         Scalar fx, Scalar fy, Scalar cx, Scalar cy);
}