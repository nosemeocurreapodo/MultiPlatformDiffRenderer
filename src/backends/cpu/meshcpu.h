#pragma once

#include "backends/cpu/buffercpu.h"
#include "backends/base/MappedView.h"

class MeshCPU
{
public:
    // using index_type = std::uint32_t;
    //  using size_type = std::size_t;

    MeshCPU(const std::vector<float> &vertex,
            const std::vector<int> &indices,
            bool has_position,
            bool has_texcoord,
            bool has_normal)
    {
        stride_ = 0;

        if (has_position)
        {
            pos_offset_ = stride_;
            stride_ += 3;
        }
        else
        {
            pos_offset_ = -1;
        }

        if (has_texcoord)
        {
            tex_offset_ = stride_;
            stride_ += 2;
        }
        else
        {
            tex_offset_ = -1;
        }

        if (has_normal)
        {
            nor_offset_ = stride_;
            stride_ += 3;
        }
        else
        {
            nor_offset_ = -1;
        }

        vertex_buffer_ = BufferCPU<float>(vertex.size(), vertex.data());
        ebo_buffer_ = BufferCPU<int>(indices.size(), indices.data());
    }

    // Copy/move
    MeshCPU(const MeshCPU &) = default;
    MeshCPU &operator=(const MeshCPU &) = default;
    MeshCPU(MeshCPU &&) noexcept = default;
    MeshCPU &operator=(MeshCPU &&) noexcept = default;
    ~MeshCPU() = default;

    std::vector<float> get_positions() const
    {
        std::vector<float> pos;
        pos.reserve(vertex_count() * 3);

        if (pos_offset_ < 0)
            return pos;
        auto vertex_map = vertex_buffer_.MapRead();
        for (int i = 0; i < vertex_count(); i++)
        {
            float v0 = vertex_map[i * stride_ + pos_offset_ + 0];
            float v1 = vertex_map[i * stride_ + pos_offset_ + 1];
            float v2 = vertex_map[i * stride_ + pos_offset_ + 2];

            pos.push_back(v0);
            pos.push_back(v1);
            pos.push_back(v2);
        }
        return pos;
    }

    std::vector<int> get_indices() const
    {
        std::vector<int> ids;
        auto index_map = ebo_buffer_.MapRead();
        for (int i = 0; i < index_map.size(); i++)
        {
            int id = index_map[i];
            ids.push_back(id);
        }
        return ids;
    }

    void set_positions(std::vector<float> &positions)
    {
        auto vertex_map = vertex_buffer_.MapWrite();
        for (int i = 0; i < vertex_count(); i++)
        {
            vertex_map[i * stride_ + pos_offset_ + 0] = positions[i * 3 + 0];
            vertex_map[i * stride_ + pos_offset_ + 1] = positions[i * 3 + 1];
            vertex_map[i * stride_ + pos_offset_ + 2] = positions[i * 3 + 2];
        }
    }

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
    int vertex_count() const noexcept { return vertex_buffer_.size() / stride_; }
    int index_count() const noexcept { return ebo_buffer_.size(); }
    int triangle_count() const noexcept { return index_count() / 3; }

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

    BufferCPU<float> vertex_buffer_;
    BufferCPU<int> ebo_buffer_;
    int stride_;
    int pos_offset_;
    int tex_offset_;
    int nor_offset_;
};
