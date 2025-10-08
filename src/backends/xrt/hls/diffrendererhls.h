#pragma once

#include "core/types.h"

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
                       Vec3 *pid_texture_data,
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
                       Scalar fx, Scalar fy, Scalar cx, Scalar cy);
}