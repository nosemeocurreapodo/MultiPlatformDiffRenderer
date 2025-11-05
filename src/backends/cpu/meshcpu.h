#pragma once

#include <Eigen/Core>
#include <opencv2/opencv.hpp>
#include "backends/cpu/buffercpu.h"
#include "backends/base/MappedView.h"

class MeshCPU
{
public:
    // using index_type = std::uint32_t;
    //  using size_type = std::size_t;

    MeshCPU(const std::vector<float> &vertex,
            const std::vector<unsigned int> &indices,
            const cv::Mat &diffuse,
            bool has_position,
            bool has_texcoord,
            bool has_normal)
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

        vertex_buffer_ = BufferCPU<float>(vertex.size(), vertex.data());
        ebo_buffer_ = BufferCPU<unsigned int>(indices.size(), indices.data());

        diffuse_ = TextureCPU<unsigned char>(diffuse.cols, diffuse.rows, 0, (unsigned char *)diffuse.ptr());
        diffuse_.generate_mipmaps(0);
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
    unsigned int vertex_count() const noexcept { return vertex_buffer_.size() / stride_; }
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

    BufferCPU<float> vertex_buffer_;
    BufferCPU<unsigned int> ebo_buffer_;
    int stride_;
    int pos_offset_;
    int tex_offset_;
    int nor_offset_;
    TextureCPU<unsigned char> diffuse_;
};
