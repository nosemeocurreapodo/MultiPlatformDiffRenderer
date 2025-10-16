#pragma once

#include "backends/cpu/buffercpu.h"

class MeshCPU
{
public:
    // using index_type = std::uint32_t;
    //  using size_type = std::size_t;

    MeshCPU(const std::vector<float> &positions, // 3 floats per vertex
            const std::vector<float> &texcoords, // 2 floats per vertex
            const std::vector<unsigned int> &indices)
        : pos_buffer_(positions),
          tex_buffer_(texcoords),
          ebo_buffer_(indices)
    {
        // validate_();
    }

    MeshCPU(const std::vector<float> &positions, // 3 floats per vertex
            const std::vector<float> &normals,
            const std::vector<float> &texcoords, // 2 floats per vertex
            const std::vector<unsigned int> &indices)
        : pos_buffer_(positions),
          nor_buffer_(normals),
          tex_buffer_(texcoords),
          ebo_buffer_(indices)
    {
        // validate_();
    }

    // Copy/move
    MeshCPU(const MeshCPU &) = default;
    MeshCPU &operator=(const MeshCPU &) = default;
    MeshCPU(MeshCPU &&) noexcept = default;
    MeshCPU &operator=(MeshCPU &&) noexcept = default;
    ~MeshCPU() = default;

    // Cross-backend style mapped views (avoid storing the view)
    //[[nodiscard]] MappedView<const float> MapReadPositions() const & { return pos_buffer_.MapRead(); }
    //[[nodiscard]] MappedView<const float> MapReadTexcoords() const & { return tex_buffer_.MapRead(); }
    //[[nodiscard]] MappedView<const float> MapReadWeights() const & { return wei_buffer_.MapRead(); }
    //[[nodiscard]] MappedView<const index_type> MapReadIndices() const & { return ebo_buffer_.MapRead(); }

    //[[nodiscard]] MappedView<float> MapWritePositions() { return pos_buffer_.MapWrite(); }
    //[[nodiscard]] MappedView<float> MapWriteTexcoords() { return tex_buffer_.MapWrite(); }
    //[[nodiscard]] MappedView<float> MapWriteWeights() { return wei_buffer_.MapWrite(); }
    //[[nodiscard]] MappedView<index_type> MapWriteIndices() { return ebo_buffer_.MapWrite(); }

    // Info
    unsigned int vertex_count() const noexcept { return pos_buffer_.size() / 3; }
    unsigned int index_count() const noexcept { return ebo_buffer_.size(); }
    unsigned int triangle_count() const noexcept { return index_count() / 3; }

    // protected:
    // template <class T, class Derived>
    // friend class RendererBase;

    // const BufferCPU<float> &Positions() const { return pos_buffer_; }
    // const BufferCPU<float> &Texcoords() const { return tex_buffer_; }
    // const BufferCPU<float> &Weights() const { return wei_buffer_; }
    // const BufferCPU<index_type> &Indices() const { return ebo_buffer_; }

    // BufferCPU<float> &Positions() { return pos_buffer_; }
    // BufferCPU<float> &Texcoords() { return tex_buffer_; }
    // BufferCPU<float> &Weights() { return wei_buffer_; }
    // BufferCPU<index_type> &Indices() { return ebo_buffer_; }

    // private:
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

    BufferCPU<float> pos_buffer_;
    BufferCPU<float> nor_buffer_;
    BufferCPU<float> tex_buffer_;
    BufferCPU<float> wei_buffer_;
    BufferCPU<unsigned int> ebo_buffer_;
};
