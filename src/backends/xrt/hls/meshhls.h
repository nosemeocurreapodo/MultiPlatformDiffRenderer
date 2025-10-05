#pragma once

#include "core/types.h"
#include "backends/xrt/hls/bufferhls.h"

class MeshHLS
{
public:
    // using index_type = std::uint32_t;
    // using size_type = std::size_t;

    // Construct from host vectors; if indices empty, build via Delaunay on UVs
    MeshHLS(Scalar *positions, // 3 floats per vertex
            Int positions_size,
            Scalar *texcoords, // 2 floats per vertex
            Int texcoords_size,
            Scalar *weights, // 1 float  per vertex
            Int weights_size,
            UInt *indices,
            Int indices_size)
        : pos_buffer_(positions_size, positions),
          tex_buffer_(texcoords_size, texcoords),
          wei_buffer_(weights_size, weights),
          ebo_buffer_(indices_size, indices)
    {
    }

    // Copy/move
    MeshHLS(const MeshHLS &) = default;
    MeshHLS &operator=(const MeshHLS &) = default;
    MeshHLS(MeshHLS &&) noexcept = default;
    MeshHLS &operator=(MeshHLS &&) noexcept = default;
    ~MeshHLS() = default;

    // Info
    UInt vertex_count() const noexcept { return pos_buffer_.size() / 3; }
    UInt index_count() const noexcept { return ebo_buffer_.size(); }
    UInt triangle_count() const noexcept { return index_count() / 3; }

protected:
    template <class Derived>
    friend class RendererBase;

    // BufferHLS<Scalar> Positions() const { return pos_buffer_; }
    // BufferHLS<Scalar> Texcoords() const { return tex_buffer_; }
    // BufferHLS<Scalar> Weights() const { return wei_buffer_; }
    // BufferHLS<UInt> Indices() const { return ebo_buffer_; }

    BufferBRAM<Scalar, 32 * 32 * 3> pos_buffer_;
    BufferBRAM<Scalar, 32 * 32 * 2> tex_buffer_;
    BufferBRAM<Scalar, 32 * 32 * 1> wei_buffer_;
    BufferBRAM<UInt, 32 * 32 * 2 * 3> ebo_buffer_;
};
