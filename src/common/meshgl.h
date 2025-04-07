#pragma once

#include "common/devicegl.h"
#include "common/meshbase.h"
#include "common/buffergl.h"

class MeshGL : public MeshBase
{
    friend class RendererGL;

public:
    MeshGL(std::vector<float> &vertices, std::vector<float> &tex_coords, std::vector<float> &weights)
        : MeshBase(),
          pos_buffer_(vertices.size()),
          tex_buffer_(tex_coords.size()),
          wei_buffer_(weights.size())
    {

        std::vector<Vec3i> triangles = BuildTriangles(tex_coords);

        pos_buffer_.FromCPU(vertices.data());
        tex_buffer_.FromCPU(tex_coords.data());
        wei_buffer_.FromCPU(weights.data());

        glGenBuffers(1, &ebo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangles.size() * sizeof(Vec3i), triangles.data(), GL_STATIC_DRAW);

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, pos_buffer_.vbo_);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, tex_buffer_.vbo_);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, wei_buffer_.vbo_);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBindVertexArray(0);

        // glGenBuffers(1, &pos_vb0_);
        // glGenBuffers(1, &tex_vb0_);
        // glGenBuffers(1, &wei_vb0_);

        pos_size_ = vertices.size();
        tri_size_ = triangles.size();
    };

    MeshGL(const MeshGL &other)
        : pos_buffer_(other.pos_buffer_),
          tex_buffer_(other.tex_buffer_),
          wei_buffer_(other.wei_buffer_)
    {
        glGenVertexArrays(1, &vao_);
    }

    MeshGL &operator=(const MeshGL &other)
    {
        if (this != &other)
        {
        }
    }

private:
    GLuint vao_;
    // GLuint pos_vbo_;
    // GLuint tex_vbo_;
    // GLuint wei_vbo_;
    BufferGL<float> pos_buffer_;
    BufferGL<float> tex_buffer_;
    BufferGL<float> wei_buffer_;
    GLuint ebo_;
    unsigned int pos_size_;
    unsigned int tri_size_;
};
