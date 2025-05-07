#include "core/types.h"
#include "bufferfpga.h"
#include "texturefpga.h"
#include "meshfpga.h"
#include "rendererfpga.h"

void DepthRenderFPGA(float *pos_buffer_data,
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
                     unsigned int out_texture_channels)
{
#pragma HLS INTERFACE m_axi port = pos_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = tex_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = wei_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = ebo_buffer_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = in_texture_data bundle = gmem0
#pragma HLS INTERFACE m_axi port = out_texture_data bundle = gmem0

    BufferFPGA<RealType> pos_buffer(pos_buffer_size, pos_buffer_data);
    BufferFPGA<RealType> tex_buffer(tex_buffer_size, tex_buffer_data);
    BufferFPGA<RealType> wei_buffer(wei_buffer_size, wei_buffer_data);
    BufferFPGA<unsigned int> ebo_buffer(ebo_buffer_size, ebo_buffer_data);
    MeshFPGA mesh(pos_buffer, tex_buffer, wei_buffer, ebo_buffer);
    TextureFPGA<ImageType> in_texture(in_texture_width, in_texture_height, in_texture_channels, 0, in_texture_data);
    TextureFPGA<RealType> out_texture(out_texture_width, out_texture_height, out_texture_channels, 0, out_texture_data);
    DepthRendererFPGA renderer;
    renderer.Render(mesh, pose[0], cam[0], in_texture, out_texture, 0);
}