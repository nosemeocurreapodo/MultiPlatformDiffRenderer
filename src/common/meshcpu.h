#pragma once

#include "common/devicecpu.h"
#include "common/meshbase.h"
#include "common/buffercpu.h"

class MeshCPU : public MeshBase
{
    friend class DepthRendererCPU;

public:
    MeshCPU(std::vector<float> &vertices, std::vector<float> &tex_coords, std::vector<float> &weights)
        : MeshBase()
    {
        std::vector<float> triangles = BuildTriangles(tex_coords);

        pos_buffer_ = BufferCPU<float>(vertices);
        tex_buffer_ = BufferCPU<float>(tex_coords);
        wei_buffer_ = BufferCPU<float>(weights);
        ebo_buffer_ = BufferCPU<float>(triangles);

        pos_size_ = vertices.size();
        tri_size_ = triangles.size();
    };

    MeshCPU(const MeshCPU &other)
    {
        pos_buffer_ = other.pos_buffer_;
        tex_buffer_ = other.tex_buffer_;
        wei_buffer_ = other.tex_buffer_;
        ebo_buffer_ = other.ebo_buffer_;

        pos_size_ = other.pos_size_;
        tri_size_ = other.tri_size_;
    }

    MeshCPU &operator=(const MeshCPU &other)
    {
        if (this != &other)
        {
            pos_buffer_ = other.pos_buffer_;
            tex_buffer_ = other.tex_buffer_;
            wei_buffer_ = other.wei_buffer_;
            ebo_buffer_ = other.ebo_buffer_;
            pos_size_ = other.pos_size_;
            tri_size_ = other.tri_size_;
        }
        return *this;
    }

private:

    BufferCPU<float> pos_buffer_;
    BufferCPU<float> tex_buffer_;
    BufferCPU<float> wei_buffer_;
    BufferCPU<float> ebo_buffer_;
    unsigned int pos_size_;
    unsigned int tri_size_;
};
