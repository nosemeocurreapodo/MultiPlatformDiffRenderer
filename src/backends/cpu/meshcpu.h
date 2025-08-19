#pragma once

#include "backends/cpu/devicecpu.h"
#include "backends/cpu/buffercpu.h"
#include "core/delaunaytriangulation.h"

class MeshCPU
{
    template <typename InTexType, typename VaryingType, typename OutTexType>
    friend class BaseRendererCPU;

public:
    MeshCPU(std::vector<float> &vertices, std::vector<float> &tex_coords, std::vector<float> &weights) : pos_buffer_(vertices),
                                                                                                         tex_buffer_(tex_coords),
                                                                                                         wei_buffer_(weights)
    {
        std::vector<unsigned int> triangles = BuildTriangles(tex_coords);
        ebo_buffer_ = BufferCPU<unsigned int>(triangles);
    };

    MeshCPU(const MeshCPU &other) : pos_buffer_(other.pos_buffer_),
                                    tex_buffer_(other.tex_buffer_),
                                    wei_buffer_(other.wei_buffer_),
                                    ebo_buffer_(other.ebo_buffer_)
    {
    }

    MeshCPU &operator=(const MeshCPU &other)
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

protected:
    BufferCPU<float> pos_buffer_;
    BufferCPU<float> tex_buffer_;
    BufferCPU<float> wei_buffer_;
    BufferCPU<unsigned int> ebo_buffer_;
    // TextureCPU<float> texture_;
};
