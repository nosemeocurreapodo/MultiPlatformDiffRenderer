#pragma once

#include "core/types.h"

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
                        Scalar fx, Scalar fy, Scalar cx, Scalar cy);
}