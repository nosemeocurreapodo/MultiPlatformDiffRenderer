#pragma once

#include "types.h"

extern void DepthRenderFPGA(float *pos_buffer_data,
                            unsigned int pos_buffer_size,
                            float *tex_buffer_data,
                            unsigned int tex_buffer_size,
                            float *wei_buffer_data,
                            unsigned int wei_buffer_size,
                            unsigned int *ebo_buffer_data,
                            unsigned int ebo_buffer_size,
                            SE3 *pose,
                            CameraType *cam,
                            ImageType *in_texture_data,
                            unsigned int in_texture_width,
                            unsigned int in_texture_height,
                            unsigned int in_texture_channels,
                            ImageType *out_texture_data,
                            unsigned int out_texture_width,
                            unsigned int out_texture_height,
                            unsigned int out_texture_channels);