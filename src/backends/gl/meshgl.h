#pragma once
// #include <vector>
// #include <cstdint>
// #include <cassert>
// #include <algorithm>
// #include <cstring> // memcpy

#include <Eigen/Core>
#include <opencv2/opencv.hpp>
// #include "backends/gl/devicegl_glad.h"
// #include "core/delaunaytriangulation.h"
#include "backends/gl/buffergl.h" // your BufferGL
#include "backends/base/MappedView.h"

class MeshGL
{
public:
    MeshGL(const std::vector<float> &vertex,
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
            stride_ += 3 * 4;
        }
        else
        {
            pos_offset_ = -1;
        }
        if (has_texcoord)
        {
            tex_offset_ = 3 * 4;
            stride_ += 2 * 4;
        }
        else
        {
            tex_offset_ = -1;
        }
        if (has_normal)
        {
            nor_offset_ = 5 * 4;
            stride_ += 3 * 4;
        }
        else
        {
            nor_offset_ = -1;
        }

        vbo_vertex_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(vertex.size(), vertex.data());
        ebo_ = BufferGL<unsigned int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW>(indices.size(), indices.data());

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_vertex_.id());

        int attribute_index = 0;
        if (has_position)
        {
            glEnableVertexAttribArray(attribute_index);
            glVertexAttribPointer(attribute_index, 3, GL_FLOAT, GL_FALSE, stride_, (void *)pos_offset_);
            attribute_index++;
        }
        if (has_texcoord)
        {
            glEnableVertexAttribArray(attribute_index);
            glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, stride_, (void *)tex_offset_);
            attribute_index++;
        }
        if (has_normal)
        {
            glEnableVertexAttribArray(attribute_index);
            glVertexAttribPointer(attribute_index, 3, GL_FLOAT, GL_FALSE, stride_, (void *)nor_offset_);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_.id());

        diffuse_ = TextureGL<unsigned char>(diffuse.cols, diffuse.rows, 0, (unsigned char *)diffuse.ptr());
        diffuse_.generate_mipmaps(0);

        glBindVertexArray(0);
    }

    ~MeshGL() { destroy_vao_(); }

    MeshGL(const MeshGL &) = delete;
    MeshGL &operator=(const MeshGL &) = delete;

    MeshGL(MeshGL &&o) noexcept { *this = std::move(o); }
    MeshGL &operator=(MeshGL &&o) noexcept
    {
        if (this != &o)
        {
            destroy_vao_();
            vao_ = std::exchange(o.vao_, 0);
            vbo_vertex_ = std::move(o.vbo_vertex_);
            ebo_ = std::move(o.ebo_);
        }
        return *this;
    }

    // Cross-backend style mapped views (avoid storing the view)
    //[[nodiscard]] MappedView<const float, GLUnmap> MapReadPositions() const & { return vbo_pos_.MapRead(); }
    //[[nodiscard]] MappedView<const float, GLUnmap> MapReadTexcoords() const & { return vbo_uv_.MapRead(); }
    //[[nodiscard]] MappedView<const float, GLUnmap> MapReadWeights() const & { return vbo_w_.MapRead(); }
    //[[nodiscard]] MappedView<const unsigned int, GLUnmap> MapReadIndices() const & { return ebo_.MapRead(); }

    //[[nodiscard]] MappedView<float, GLUnmap> MapWritePositions() { return vbo_pos_.MapWrite(); }
    //[[nodiscard]] MappedView<float, GLUnmap> MapWriteTexcoords() { return vbo_uv_.MapWrite(); }
    //[[nodiscard]] MappedView<float, GLUnmap> MapWriteWeights() { return vbo_w_.MapWrite(); }
    //[[nodiscard]] MappedView<unsigned int, GLUnmap> MapWriteIndices() { return ebo_.MapWrite(); }

    // --- Info ---
    std::size_t vertex_count() const noexcept { return vbo_vertex_.size() / stride_; }
    std::size_t index_count() const noexcept { return ebo_.size(); }
    std::size_t triangle_count() const noexcept { return index_count() / 3; }

    void draw() const
    {
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(ebo_.size()),
                       GL_UNSIGNED_INT,
                       (void *)0);
        glBindVertexArray(0);
    }

    void destroy_vao_()
    {
        if (vao_)
            glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }

    GLuint vao_ = 0;

    BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW> vbo_vertex_;
    BufferGL<unsigned int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW> ebo_;
    TextureGL<unsigned char> diffuse_;

    int stride_;
    long int pos_offset_;
    long int tex_offset_;
    long int nor_offset_;
};
