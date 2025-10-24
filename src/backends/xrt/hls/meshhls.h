#pragma once

// #include "core/types.h"
#include "backends/xrt/hls/bufferhls.h"

template <typename T, template <class> class Buffer, template <class> class Texture>
class MeshHLS
{
public:
    MeshHLS(T *vertexs,
            unsigned int vertex_size,
            unsigned int *indices,
            unsigned int indices_size,
            T *diffuse_data,
            unsigned int diffuse_width,
            unsigned int diffuse_height,
            T diffuse_nodata_value)
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

    Buffer<T> vertex_buffer_;
    Buffer<unsigned int> ebo_buffer_;

    int stride_;
    int pos_offset_;
    int tex_offset_;
    int nor_offset_;

    Texture<T> diffuse_;
};
