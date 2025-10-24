#pragma once
// #include <cassert>
// #include <cstddef>
// #include <cstdint>
// #include <vector>
// #include <array>
// #include <algorithm>

#include "backends/xrt/devicexrt.h"
#include "backends/xrt/bufferxrt.h"
#include "backends/xrt/texturexrt.h"

class MeshXRT
{
public:
    // using index_type = std::uint32_t;
    //  using size_type = std::size_t;

    MeshXRT(const std::vector<float> &vertex,
            const std::vector<unsigned int> &indices,
            const cv::Mat &diffuse,
            bool has_position,
            bool has_texcoord,
            bool has_normal,
            int vertex_group_id,
            int indices_group_id,
            int diffuse_group_id)
    {
        stride_ = 0;

        if (has_position)
        {
            pos_offset_ = 0;
            stride_ += 3;
        }
        else
        {
            pos_offset_ = -1;
        }
        if (has_texcoord)
        {
            tex_offset_ = 3;
            stride_ += 2;
        }
        else
        {
            tex_offset_ = -1;
        }
        if (has_normal)
        {
            nor_offset_ = 5;
            stride_ += 3;
        }
        else
        {
            nor_offset_ = -1;
        }

        vertex_buffer_ = BufferXRT<float>(vertex.size(), vertex.data(), vertex_group_id);
        ebo_buffer_ = BufferXRT<unsigned int>(indices.size(), indices.data(), indices_group_id);

        diffuse_ = TextureXRT<float>(diffuse.cols, diffuse.rows, -1.0f, (float *)diffuse.ptr(), diffuse_group_id);
        diffuse_.generate_mipmaps(0);
    }

    // Copy/move
    // MeshXRT(const MeshXRT &) = default;
    // MeshXRT &operator=(const MeshXRT &) = default;
    // MeshXRT(MeshXRT &&) noexcept = default;
    // MeshXRT &operator=(MeshXRT &&) noexcept = default;
    ~MeshXRT() = default;

    /*
    // Read-only access to buffers (keeps ownership internal)
    const BufferCPU<float> &positions() const noexcept { return pos_buffer_; }
    const BufferCPU<float> &texcoords() const noexcept { return tex_buffer_; }
    const BufferCPU<float> &weights() const noexcept { return wei_buffer_; }
    const BufferCPU<index_type> &indices() const noexcept { return ebo_buffer_; }
    */

    // Cross-backend style mapped views (avoid storing the view)
    //[[nodiscard]] MappedView<const float> MapReadPositions() const & { return pos_buffer_.MapRead(); }
    //[[nodiscard]] MappedView<const float> MapReadTexcoords() const & { return tex_buffer_.MapRead(); }
    //[[nodiscard]] MappedView<const float> MapReadWeights() const & { return wei_buffer_.MapRead(); }
    //[[nodiscard]] MappedView<const unsigned int> MapReadIndices() const & { return ebo_buffer_.MapRead(); }

    //[[nodiscard]] MappedView<float> MapWritePositions() { return pos_buffer_.MapWrite(); }
    //[[nodiscard]] MappedView<float> MapWriteTexcoords() { return tex_buffer_.MapWrite(); }
    //[[nodiscard]] MappedView<float> MapWriteWeights() { return wei_buffer_.MapWrite(); }
    //[[nodiscard]] MappedView<unsigned int> MapWriteIndices() { return ebo_buffer_.MapWrite(); }

    // Info
    std::size_t vertex_count() const noexcept { return vertex_buffer_.size() / stride_; }
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

    // private:
    //     friend class DepthRendererXRT;
    /*
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
        */

    BufferXRT<float> vertex_buffer_;
    BufferXRT<unsigned int> ebo_buffer_;
    int stride_;
    int pos_offset_;
    int tex_offset_;
    int nor_offset_;
    TextureXRT<float> diffuse_;
};
