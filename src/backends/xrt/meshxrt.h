#pragma once

#include "xrt/devicexrt.h"
#include "xrt/bufferxrt.h"
#include "common/meshbase.h"

class MeshXRT : public MeshBase
{

public:
    MeshXRT(std::vector<float> &vertices, std::vector<float> &tex_coords, std::vector<float> &weights)
        : MeshBase()
    {
        std::vector<unsigned int> triangles = BuildTriangles(tex_coords);

        pos_buffer_ = BufferXRT<float>(vertices);
        tex_buffer_ = BufferXRT<float>(tex_coords);
        wei_buffer_ = BufferXRT<float>(weights);
        ebo_buffer_ = BufferXRT<unsigned int>(triangles);
    };

    MeshXRT(const MeshXRT &other)
    {
        pos_buffer_ = other.pos_buffer_;
        tex_buffer_ = other.tex_buffer_;
        wei_buffer_ = other.tex_buffer_;
        ebo_buffer_ = other.ebo_buffer_;
    }

    MeshXRT &operator=(const MeshXRT &other)
    {
        if (this != &other)
        {
            pos_buffer_ = other.pos_buffer_;
            tex_buffer_ = other.tex_buffer_;
            wei_buffer_ = other.wei_buffer_;
            ebo_buffer_ = other.ebo_buffer_;
        }
        return *this;
    }

private:
    BufferXRT<float> pos_buffer_;
    BufferXRT<float> tex_buffer_;
    BufferXRT<float> wei_buffer_;
    BufferXRT<unsigned int> ebo_buffer_;
    // TextureCPU<float> texture_;
};
