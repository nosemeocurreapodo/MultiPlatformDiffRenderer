#pragma once

#include "bufferfpga.h"

class MeshFPGA
{
template <typename  InTexType,
          typename  VaryingType,
          typename  OutTexType,
          typename  Derived>   
friend class BaseRendererFPGA;

public:
    MeshFPGA(const BufferFPGA<fpga::Scalar> &pos_buffer,
             const BufferFPGA<fpga::Scalar> &tex_buffer,
             const BufferFPGA<fpga::Scalar> &wei_buffer,
             const BufferFPGA<fpga::UInt> &ebo_buffer)
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
    BufferFPGA<fpga::Scalar> pos_buffer_;
    BufferFPGA<fpga::Scalar> tex_buffer_;
    BufferFPGA<fpga::Scalar> wei_buffer_;
    BufferFPGA<fpga::UInt> ebo_buffer_;
    // TextureFPGA<float> texture_;
};
