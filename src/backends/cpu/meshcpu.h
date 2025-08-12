#pragma once

#include "backends/cpu/devicecpu.h"
#include "backends/cpu/buffercpu.h"
#include "core/delaunaytriangulation.h"

class MeshCPU
{
public:
    MeshCPU(std::vector<float> &vertices, std::vector<float> &tex_coords, std::vector<float> &weights)
    {
        std::vector<unsigned int> triangles = BuildTriangles(tex_coords);

        pos_buffer_ = BufferCPU<float>(vertices);
        tex_buffer_ = BufferCPU<float>(tex_coords);
        wei_buffer_ = BufferCPU<float>(weights);
        ebo_buffer_ = BufferCPU<unsigned int>(triangles);
    };

    MeshCPU(const MeshCPU &other)
    {
        pos_buffer_ = other.pos_buffer_;
        tex_buffer_ = other.tex_buffer_;
        wei_buffer_ = other.tex_buffer_;
        ebo_buffer_ = other.ebo_buffer_;
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

    const BufferCPU<float> GetPosBuffer() const
    {
        return pos_buffer_;
    }

    const BufferCPU<float> GetTexBuffer() const
    {
        return tex_buffer_;
    }

    const BufferCPU<float> GetWeiBuffer() const
    {
        return wei_buffer_;
    }

    const BufferCPU<unsigned int> GetEboBuffer() const
    {
        return ebo_buffer_;
    }

private:
    BufferCPU<float> pos_buffer_;
    BufferCPU<float> tex_buffer_;
    BufferCPU<float> wei_buffer_;
    BufferCPU<unsigned int> ebo_buffer_;
    // TextureCPU<float> texture_;
};
