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
           const std::vector<int> &indices,
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
        ebo_ = BufferGL<int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW>(indices.size(), indices.data());

        create_vao_();
    }

    ~MeshGL() { destroy_vao_(); }

    // ---------- COPY CONSTRUCTOR ----------
    MeshGL(const MeshGL &other)
        : vao_(0),
          vbo_vertex_(other.vbo_vertex_),
          ebo_(other.ebo_),
          stride_(other.stride_),
          pos_offset_(other.pos_offset_),
          tex_offset_(other.tex_offset_),
          nor_offset_(other.nor_offset_)
    {
        create_vao_();
    }

    // ---------- COPY ASSIGNMENT ----------
    MeshGL &operator=(const MeshGL &other)
    {
        if (this != &other)
        {
            destroy_vao_();

            vbo_vertex_ = other.vbo_vertex_;
            ebo_ = other.ebo_;

            stride_ = other.stride_;
            pos_offset_ = other.pos_offset_;
            tex_offset_ = other.tex_offset_;
            nor_offset_ = other.nor_offset_;

            create_vao_();
        }
        return *this;
    }

    // ---------- MOVE CONSTRUCTOR / ASSIGNMENT ----------
    MeshGL(MeshGL &&o) noexcept { *this = std::move(o); }

    MeshGL &operator=(MeshGL &&o) noexcept
    {
        if (this != &o)
        {
            destroy_vao_();

            vao_ = std::exchange(o.vao_, 0);
            vbo_vertex_ = std::move(o.vbo_vertex_);
            ebo_ = std::move(o.ebo_);

            stride_ = o.stride_;
            pos_offset_ = o.pos_offset_;
            tex_offset_ = o.tex_offset_;
            nor_offset_ = o.nor_offset_;
        }
        return *this;
    }

    // --- Info ---
    std::size_t vertex_count() const noexcept { return vbo_vertex_.size() / stride_; }
    std::size_t index_count() const noexcept { return ebo_.size(); }
    std::size_t triangle_count() const noexcept { return index_count() / 3; }

    std::vector<float> get_positions()
    {
        std::vector<float> pos;
        if (pos_offset_ < 0)
            return pos;
        auto vertex_map = vbo_vertex_.MapRead();
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

    std::vector<int> get_indices()
    {
        std::vector<int> ids;
        auto index_map = ebo_.MapRead();
        for (int i = 0; i < index_map.size(); i++)
        {
            int id = index_map[i];
            ids.push_back(id);
        }
        return ids;
    }

    void set_positions(std::vector<float> &positions)
    {
        auto vertex_map = vbo_vertex_.MapWrite();
        for (int i = 0; i < vertex_count(); i++)
        {
            vertex_map[i * stride_ + pos_offset_ + 0] = positions[i * 3 + 0];
            vertex_map[i * stride_ + pos_offset_ + 1] = positions[i * 3 + 1];
            vertex_map[i * stride_ + pos_offset_ + 2] = positions[i * 3 + 2];
        }
    }

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
    BufferGL<int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW> ebo_;

    int stride_{};
    long int pos_offset_{-1};
    long int tex_offset_{-1};
    long int nor_offset_{-1};

private:
    void create_vao_()
    {
        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_vertex_.id());

        int attribute_index = 0;
        if (pos_offset_ >= 0)
        {
            glEnableVertexAttribArray(attribute_index);
            glVertexAttribPointer(attribute_index, 3, GL_FLOAT, GL_FALSE, stride_,
                                  reinterpret_cast<void *>(pos_offset_));
            attribute_index++;
        }
        if (tex_offset_ >= 0)
        {
            glEnableVertexAttribArray(attribute_index);
            glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, stride_,
                                  reinterpret_cast<void *>(tex_offset_));
            attribute_index++;
        }
        if (nor_offset_ >= 0)
        {
            glEnableVertexAttribArray(attribute_index);
            glVertexAttribPointer(attribute_index, 3, GL_FLOAT, GL_FALSE, stride_,
                                  reinterpret_cast<void *>(nor_offset_));
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_.id());

        glBindVertexArray(0);
    }
};
