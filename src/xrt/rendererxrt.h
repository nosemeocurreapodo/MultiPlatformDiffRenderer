#pragma once

#include "xrt/meshxrt.h"

void DepthRenderXRT(MeshXRT &mesh,
                    const SE3 *pose,
                    const CameraType *cam,
                    TextureXRT<ImageType> &in_texture,
                    TextureXRT<float> &out_texture)
{
    auto run = xrt_kernel(mesh.pos_buffer_.data_, mesh.pose_buffer_.size_,
                          mesh.tex_buffer_.data_, mesh.tex_buffer_.size_,
                          mesh.wei_buffer_.data_, mesh.wei_buffer_.size_,
                          mesh.ebo_buffer_.data_, mesh.ebo_buffer_.size_,
                          pose[0], cam[0],
                          in_texture.data_, in_texture.width_, in_texture.height_, in_texture.channels_,
                          out_texture.data_, out_texture.width_, out_texture.height_, out_texture.channels_);
    run.wait();
}
