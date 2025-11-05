#pragma once

// #include "core/types.h"
#include "backends/xrt/hls/bufferhls.h"
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/math_common.h"

class MeshHLS
{
public:
    template <typename DiffPointerType>
    MeshHLS(float *vertexs,
            unsigned int vertex_size,
            unsigned int *indices,
            unsigned int indices_size,
            DiffPointerType *diffuse_data,
            unsigned int diffuse_width,
            unsigned int diffuse_height,
            unsigned char diffuse_nodata_value)
        : vertex_buffer_(vertex_size, vertexs),
          ebo_buffer_(indices_size, indices),
          diffuse_(diffuse_width, diffuse_height, diffuse_nodata_value, diffuse_data)
    {
        stride_ = 8;
        pos_offset_ = 0;
        tex_offset_ = 3;
        nor_offset_ = 5;
    }

    // Copy/move
    // MeshHLS(const MeshHLS &) = default;
    // MeshHLS &operator=(const MeshHLS &) = default;
    // MeshHLS(MeshHLS &&) noexcept = default;
    // MeshHLS &operator=(MeshHLS &&) noexcept = default;
    ~MeshHLS() = default;

    // Info
    unsigned int vertex_count() const noexcept { return vertex_buffer_.size() / 3; }
    unsigned int index_count() const noexcept { return ebo_buffer_.size(); }
    unsigned int triangle_count() const noexcept { return index_count() / 3; }

    // protected:
    // template <class Derived>
    // friend class RendererBase;

    // BufferHLS<Scalar> Positions() const { return pos_buffer_; }
    // BufferHLS<Scalar> Texcoords() const { return tex_buffer_; }
    // BufferHLS<Scalar> Weights() const { return wei_buffer_; }
    // BufferHLS<UInt> Indices() const { return ebo_buffer_; }

    BufferRAM<float> vertex_buffer_;
    BufferRAM<unsigned int> ebo_buffer_;

    int stride_;
    int pos_offset_;
    int tex_offset_;
    int nor_offset_;

    TextureRAM<unsigned char> diffuse_;
};
