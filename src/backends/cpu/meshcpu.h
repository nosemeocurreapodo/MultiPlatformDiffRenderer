#pragma once

#include <Eigen/Core>
#include "backends/cpu/buffercpu.h"
#include "backends/base/MappedView.h"

class MeshCPU
{
public:
    // using index_type = std::uint32_t;
    //  using size_type = std::size_t;

    MeshCPU(const std::vector<Eigen::Vector3f> &positions,
            const std::vector<Eigen::Vector2f> &texcoords, // 2 floats per vertex
            const std::vector<unsigned int> &indices)
        : stride_(5),
          pos_offset_(0),
          tex_offset_(3),
          nor_offset_(-1),
          vertex_buffer_(positions.size() * 5),
          ebo_buffer_(indices)
    {
        assert(positions.size() == texcoords.size());

        for (int i = 0; i < positions.size(); ++i)
        {
            vertex_buffer_[stride_ * i + pos_offset_ + 0] = positions[i].x();
            vertex_buffer_[stride_ * i + pos_offset_ + 1] = positions[i].y();
            vertex_buffer_[stride_ * i + pos_offset_ + 2] = positions[i].z();

            vertex_buffer_[stride_ * i + tex_offset_ + 0] = texcoords[i].x();
            vertex_buffer_[stride_ * i + tex_offset_ + 1] = texcoords[i].y();
        }
        // validate_();
    }

    MeshCPU(const std::vector<Eigen::Vector3f> &positions, // 3 floats per vertex
            const std::vector<Eigen::Vector2f> &texcoords, // 2 floats per vertex
            const std::vector<Eigen::Vector3f> &normals,
            const std::vector<unsigned int> &indices)
        : stride_(8),
          pos_offset_(0),
          tex_offset_(3),
          nor_offset_(5),
          vertex_buffer_(positions.size() * 8),
          ebo_buffer_(indices)
    {
        assert(positions.size() == texcoords.size());

        for (int i = 0; i < positions.size(); ++i)
        {
            vertex_buffer_[stride_ * i + pos_offset_ + 0] = positions[i].x();
            vertex_buffer_[stride_ * i + pos_offset_ + 1] = positions[i].y();
            vertex_buffer_[stride_ * i + pos_offset_ + 2] = positions[i].z();

            vertex_buffer_[stride_ * i + tex_offset_ + 0] = texcoords[i].x();
            vertex_buffer_[stride_ * i + tex_offset_ + 1] = texcoords[i].y();

            vertex_buffer_[stride_ * i + nor_offset_ + 0] = normals[i].x();
            vertex_buffer_[stride_ * i + nor_offset_ + 1] = normals[i].y();
            vertex_buffer_[stride_ * i + nor_offset_ + 2] = normals[i].z();
        }

        // validate_();
    }

    MeshCPU(const std::vector<float> &positions, // 3 floats per vertex
            const std::vector<float> &texcoords, // 2 floats per vertex
            const std::vector<unsigned int> &indices)
        : stride_(5),
          pos_offset_(0),
          tex_offset_(3),
          nor_offset_(-1),
          vertex_buffer_(positions.size() + texcoords.size()),
          ebo_buffer_(indices)
    {
        for (int i = 0; i < positions.size() / 3; ++i)
        {
            vertex_buffer_[stride_ * i + pos_offset_ + 0] = positions[3 * i + 0];
            vertex_buffer_[stride_ * i + pos_offset_ + 1] = positions[3 * i + 1];
            vertex_buffer_[stride_ * i + pos_offset_ + 2] = positions[3 * i + 2];
            vertex_buffer_[stride_ * i + tex_offset_ + 0] = texcoords[2 * i + 0];
            vertex_buffer_[stride_ * i + tex_offset_ + 1] = texcoords[2 * i + 1];
        }

        // validate_();
    }

    MeshCPU(const std::vector<float> &positions, // 3 floats per vertex
            const std::vector<float> &texcoords, // 2 floats per vertex
            const std::vector<float> &normals,
            const std::vector<unsigned int> &indices)
        : vertex_buffer_(positions.size() + normals.size() + texcoords.size()),
          ebo_buffer_(indices)
    {
        for (int i = 0; i < positions.size() / 3; ++i)
        {
            vertex_buffer_[stride_ * i + pos_offset_ + 0] = positions[3 * i + 0];
            vertex_buffer_[stride_ * i + pos_offset_ + 1] = positions[3 * i + 1];
            vertex_buffer_[stride_ * i + pos_offset_ + 2] = positions[3 * i + 2];
            vertex_buffer_[stride_ * i + tex_offset_ + 0] = texcoords[2 * i + 0];
            vertex_buffer_[stride_ * i + tex_offset_ + 1] = texcoords[2 * i + 1];
            vertex_buffer_[stride_ * i + nor_offset_ + 0] = normals[3 * i + 0];
            vertex_buffer_[stride_ * i + nor_offset_ + 1] = normals[3 * i + 1];
            vertex_buffer_[stride_ * i + nor_offset_ + 2] = normals[3 * i + 2];
        }
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
};
