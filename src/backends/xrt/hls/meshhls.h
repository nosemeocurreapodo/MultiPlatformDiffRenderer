#pragma once

#include "backends/core/types.h"
#include "backends/xrt/hls/bufferhls.h"

class MeshHLS
{
public:
    using index_type = std::uint32_t;
    // using size_type = std::size_t;

    // Construct from host vectors; if indices empty, build via Delaunay on UVs
    MeshHLS(const Scalar *positions, // 3 floats per vertex
            Int positions_size,
            const Scalar *texcoords, // 2 floats per vertex
            Int texcoords_size,
            const Scalar *weights, // 1 float  per vertex
            Int weights_size,
            const UInt *indices,
            Int indices_size)
        : pos_buffer_(positions_size, positions),
          tex_buffer_(texcoords_size, texcoords),
          wei_buffer_(weights_size, weights),
          ebo_buffer_(indices_size, indices)
    {
        validate_();
    }

    // Copy/move
    MeshHLS(const MeshHLS &) = default;
    MeshHLS &operator=(const MeshHLS &) = default;
    MeshHLS(MeshHLS &&) noexcept = default;
    MeshHLS &operator=(MeshHLS &&) noexcept = default;
    ~MeshHLS() = default;

    // Info
    std::size_t vertex_count() const noexcept { return pos_buffer_.size() / 3; }
    std::size_t index_count() const noexcept { return ebo_buffer_.size(); }
    std::size_t triangle_count() const noexcept { return index_count() / 3; }

protected:
    template <class Derived>
    friend class RendererBase;

    const BufferCPU<float> &Positions() const { return pos_buffer_; }
    const BufferCPU<float> &Texcoords() const { return tex_buffer_; }
    const BufferCPU<float> &Weights() const { return wei_buffer_; }
    const BufferCPU<index_type> &Indices() const { return ebo_buffer_; }

    BufferCPU<float> &Positions() { return pos_buffer_; }
    BufferCPU<float> &Texcoords() { return tex_buffer_; }
    BufferCPU<float> &Weights() { return wei_buffer_; }
    BufferCPU<index_type> &Indices() { return ebo_buffer_; }

private:
    void validate_() const
    {
        // position size must be multiple of 3
        assert(pos_buffer_.size() % 3 == 0);
        const std::size_t nverts = pos_buffer_.size() / 3;

        // tex must be multiple of 2 and match vertex count
        assert(tex_buffer_.size() % 2 == 0);
        assert(tex_buffer_.size() / 2 == nverts);

        // weights match vertex count
        assert(wei_buffer_.size() == nverts);

        // indices multiple of 3
        assert(ebo_buffer_.size() % 3 == 0);

        // (Optional) indices range check in debug
#ifndef NDEBUG
        auto idx = ebo_buffer_.MapRead();
        for (std::size_t i = 0; i < idx.size(); ++i)
        {
            assert(idx[i] < static_cast<std::size_t>(nverts));
        }
#endif
    }

    BufferHLS<float> pos_buffer_;
    BufferHLS<float> tex_buffer_;
    BufferHLS<float> wei_buffer_;
    BufferHLS<index_type> ebo_buffer_;
};
