#pragma once
// #include <cassert>
// #include <cstddef>
// #include <cstdint>
// #include <vector>
// #include <array>
// #include <algorithm>

#include "backends/xrt/devicexrt.h"
#include "backends/xrt/bufferxrt.h"

class MeshXRT
{
public:
    // using index_type = std::uint32_t;
    //  using size_type = std::size_t;

    // Construct from host vectors; if indices empty, build via Delaunay on UVs
    MeshXRT(const std::vector<float> &positions, // 3 floats per vertex
            const std::vector<float> &texcoords, // 2 floats per vertex
            const std::vector<float> &weights,   // 1 float  per vertex
            const std::vector<unsigned int> &indices,
            xrt::kernel &kernel)
        : pos_buffer_(positions, kernel.group_id(0)),
          tex_buffer_(texcoords, kernel.group_id(1)),
          wei_buffer_(weights, kernel.group_id(2)),
          ebo_buffer_(indices, kernel.group_id(3))
    {
        validate_();
    }

    // Copy/move
    MeshXRT(const MeshXRT &) = default;
    MeshXRT &operator=(const MeshXRT &) = default;
    MeshXRT(MeshXRT &&) noexcept = default;
    MeshXRT &operator=(MeshXRT &&) noexcept = default;
    ~MeshXRT() = default;

    /*
    // Read-only access to buffers (keeps ownership internal)
    const BufferCPU<float> &positions() const noexcept { return pos_buffer_; }
    const BufferCPU<float> &texcoords() const noexcept { return tex_buffer_; }
    const BufferCPU<float> &weights() const noexcept { return wei_buffer_; }
    const BufferCPU<index_type> &indices() const noexcept { return ebo_buffer_; }
    */

    // Cross-backend style mapped views (avoid storing the view)
    [[nodiscard]] MappedView<const float> MapReadPositions() const & { return pos_buffer_.MapRead(); }
    [[nodiscard]] MappedView<const float> MapReadTexcoords() const & { return tex_buffer_.MapRead(); }
    [[nodiscard]] MappedView<const float> MapReadWeights() const & { return wei_buffer_.MapRead(); }
    [[nodiscard]] MappedView<const unsigned int> MapReadIndices() const & { return ebo_buffer_.MapRead(); }

    [[nodiscard]] MappedView<float> MapWritePositions() { return pos_buffer_.MapWrite(); }
    [[nodiscard]] MappedView<float> MapWriteTexcoords() { return tex_buffer_.MapWrite(); }
    [[nodiscard]] MappedView<float> MapWriteWeights() { return wei_buffer_.MapWrite(); }
    [[nodiscard]] MappedView<unsigned int> MapWriteIndices() { return ebo_buffer_.MapWrite(); }

    // Info
    std::size_t vertex_count() const noexcept { return pos_buffer_.size() / 3; }
    std::size_t index_count() const noexcept { return ebo_buffer_.size(); }
    std::size_t triangle_count() const noexcept { return index_count() / 3; }

    /*
    // Rebuild indices from current texcoords (e.g., after UV edit)
    void rebuild_indices_from_uv()
    {
        //std::vector<unsigned int> tris = BuildTriangles(
        //    std::vector<float>(tex_buffer_.begin(), tex_buffer_.end()));

        auto m = tex_buffer_.MapRead();
        std::vector<unsigned int> tris = BuildTriangles(
            std::vector<float>(m.begin(), m.end()));

        std::vector<index_type> idx(tris.begin(), tris.end());
        ebo_buffer_ = BufferCPU<index_type>(idx);
        assert(index_count() % 3 == 0);
    }
    */

//private:
//    friend class DepthRendererXRT;

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

    BufferXRT<float> pos_buffer_;
    BufferXRT<float> tex_buffer_;
    BufferXRT<float> wei_buffer_;
    BufferXRT<unsigned int> ebo_buffer_;
};
