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
    std::vector<unsigned int> BuildTriangles(std::vector<float> tex_coords)
    {
        DelaunayTriangulation<cpu::Vec2, cpu::Vec3, cpu::Vec3i> triangulator_;
        std::vector<cpu::Vec2> tex_coords_2d;
        for (size_t i = 0; i < tex_coords.size(); i += 2)
        {
            tex_coords_2d.push_back(cpu::Vec2(tex_coords[i], tex_coords[i + 1]));
        }
        triangulator_.LoadPoints(tex_coords_2d);
        triangulator_.Triangulate();
        std::vector<cpu::Vec3i> tris = triangulator_.GetTriangles();
        std::vector<unsigned int> tris_f;
        for (size_t i = 0; i < tris.size(); i++)
        {
            tris_f.push_back(tris[i](0));
            tris_f.push_back(tris[i](1));
            tris_f.push_back(tris[i](2));
        }
        return tris_f;
    }

    BufferCPU<float> pos_buffer_;
    BufferCPU<float> tex_buffer_;
    BufferCPU<float> wei_buffer_;
    BufferCPU<unsigned int> ebo_buffer_;
    // TextureCPU<float> texture_;
};
