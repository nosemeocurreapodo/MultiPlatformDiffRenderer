#pragma once

//#include "core/types.h"
#include "backends/xrt/hls/bufferhls.h"

template <typename T>
class MeshHLS
{
public:
    // using index_type = std::uint32_t;
    // using size_type = std::size_t;

    // Construct from host vectors; if indices empty, build via Delaunay on UVs
    MeshHLS(const T *positions, // 3 floats per vertex
            unsigned int positions_size,
            const T *texcoords, // 2 floats per vertex
            unsigned int texcoords_size,
            const T *weights, // 1 float  per vertex
            unsigned int weights_size,
            const unsigned int *indices,
            unsigned int indices_size)
        : pos_buffer_(positions_size, positions),
          tex_buffer_(texcoords_size, texcoords),
          wei_buffer_(weights_size, weights),
          ebo_buffer_(indices_size, indices)
    {
    }

    // Copy/move
    // MeshHLS(const MeshHLS &) = default;
    // MeshHLS &operator=(const MeshHLS &) = default;
    // MeshHLS(MeshHLS &&) noexcept = default;
    // MeshHLS &operator=(MeshHLS &&) noexcept = default;
    ~MeshHLS() = default;

    // Info
    unsigned int vertex_count() const noexcept { return pos_buffer_.size() / 3; }
    unsigned int index_count() const noexcept { return ebo_buffer_.size(); }
    unsigned int triangle_count() const noexcept { return index_count() / 3; }

    // protected:
    // template <class Derived>
    // friend class RendererBase;

    // BufferHLS<Scalar> Positions() const { return pos_buffer_; }
    // BufferHLS<Scalar> Texcoords() const { return tex_buffer_; }
    // BufferHLS<Scalar> Weights() const { return wei_buffer_; }
    // BufferHLS<UInt> Indices() const { return ebo_buffer_; }

    BufferBRAM<T, 32 * 32 * 3> pos_buffer_;
    BufferBRAM<T, 32 * 32 * 2> tex_buffer_;
    BufferBRAM<T, 32 * 32 * 1> wei_buffer_;
    BufferBRAM<unsigned int, 32 * 32 * 2 * 3> ebo_buffer_;
};
