#pragma once

#include "bufferfpga.h"
#include "rendererfpga.h"

class MeshFPGA
{
template <typename  InTexType,
          typename  VaryingType,
          typename  OutTexType,
          typename  Derived>   
friend class BaseRendererFPGA;

public:
    MeshFPGA(const BufferFPGA<float> &pos_buffer,
             const BufferFPGA<float> &tex_buffer,
             const BufferFPGA<float> &wei_buffer,
             const BufferFPGA<unsigned int> &ebo_buffer)
        : pos_buffer_(pos_buffer),
          tex_buffer_(tex_buffer),
          wei_buffer_(wei_buffer),
          ebo_buffer_(ebo_buffer) {

          };

    MeshFPGA(const MeshFPGA &other)
        : pos_buffer_(other.pos_buffer_),
          tex_buffer_(other.tex_buffer_),
          wei_buffer_(other.wei_buffer_),
          ebo_buffer_(other.ebo_buffer_)
    {
    }

    MeshFPGA &operator=(const MeshFPGA &other)
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
    BufferFPGA<float> pos_buffer_;
    BufferFPGA<float> tex_buffer_;
    BufferFPGA<float> wei_buffer_;
    BufferFPGA<unsigned int> ebo_buffer_;
    // TextureFPGA<float> texture_;
};
