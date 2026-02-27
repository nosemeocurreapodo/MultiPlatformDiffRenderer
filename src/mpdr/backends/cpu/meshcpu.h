#pragma once

#include "mpdr/backends/cpu/buffercpu.h"
#include "mpdr/backends/base/meshbase.h"

class MeshCPU
{
public:
    // using index_type = std::uint32_t;
    //  using size_type = std::size_t;

    //MeshCPU()
    //{
    //    stride_ = 0;
    //    pos_offset_ = -1;
    //    tex_offset_ = -1;
    //    nor_offset_ = -1;
    //};

    MeshCPU(const std::vector<float> &vertex,
            const std::vector<int> &indices,
            bool has_position,
            bool has_texcoord,
            bool has_normal)
            :vertex_buffer_(vertex.size(), vertex.data()),
            ebo_buffer_(indices.size(), indices.data())
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
    }

    // Copy/move
    MeshCPU(const MeshCPU &) = delete;
    MeshCPU &operator=(const MeshCPU &) = delete;
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
