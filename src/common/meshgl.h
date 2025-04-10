#pragma once

#include "common/devicegl_glad.h"
#include "common/meshbase.h"
#include "common/buffergl.h"
#include "common/texturegl.h"

class MeshGL : public MeshBase
{
    template <typename InTexType, typename OutTexType>
    friend class BaseRendererGL;
    friend class DepthRendererGL;
    friend class ImageRendererGL;

public:
    MeshGL(const std::vector<float> &vertices, const std::vector<float> &tex_coords, const std::vector<float> &weights)
        : MeshBase()
    {
        std::vector<unsigned int> triangles = BuildTriangles(tex_coords);

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);
        pos_buffer_ = BufferGL<float, GL_ARRAY_BUFFER>(vertices);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(0);
        tex_buffer_ = BufferGL<float, GL_ARRAY_BUFFER>(tex_coords);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(1);
        wei_buffer_ = BufferGL<float, GL_ARRAY_BUFFER>(weights);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(2);
        ebo_buffer_ = BufferGL<unsigned int, GL_ELEMENT_ARRAY_BUFFER>(triangles);
        glBindVertexArray(0);

        //glBindBuffer(GL_ARRAY_BUFFER, pos_buffer_.buffer_);
        //glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
        //glEnableVertexAttribArray(0);
        //glBindBuffer(GL_ARRAY_BUFFER, tex_buffer_.buffer_);
        //glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
        //glEnableVertexAttribArray(1);
        //glBindBuffer(GL_ARRAY_BUFFER, wei_buffer_.buffer_);
        //glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, (void *)0);
        //glEnableVertexAttribArray(2);
        //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_buffer_.buffer_);
        //glBindVertexArray(0);

        // glGenBuffers(1, &pos_vb0_);
        // glGenBuffers(1, &tex_vb0_);
        // glGenBuffers(1, &wei_vb0_);

        pos_size_ = vertices.size();
        tri_size_ = triangles.size();
    };

    MeshGL(const MeshGL &other)
    {
        pos_buffer_ = other.pos_buffer_;
        tex_buffer_ = other.tex_buffer_;
        wei_buffer_ = other.tex_buffer_;
        ebo_buffer_ = other.ebo_buffer_;

        pos_size_ = other.pos_size_;
        tri_size_ = other.tri_size_;

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, pos_buffer_.buffer_);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, tex_buffer_.buffer_);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, wei_buffer_.buffer_);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_buffer_.buffer_);
        glBindVertexArray(0);
    }

    MeshGL &operator=(const MeshGL &other)
    {
        if (this != &other)
        {
            glDeleteVertexArrays(1, &vao_);

            pos_buffer_ = other.pos_buffer_;
            tex_buffer_ = other.tex_buffer_;
            wei_buffer_ = other.wei_buffer_;
            ebo_buffer_ = other.ebo_buffer_;
            pos_size_ = other.pos_size_;
            tri_size_ = other.tri_size_;

            glGenVertexArrays(1, &vao_);
            glBindVertexArray(vao_);
            glBindBuffer(GL_ARRAY_BUFFER, pos_buffer_.buffer_);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
            glEnableVertexAttribArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, tex_buffer_.buffer_);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
            glEnableVertexAttribArray(1);
            glBindBuffer(GL_ARRAY_BUFFER, wei_buffer_.buffer_);
            glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, (void *)0);
            glEnableVertexAttribArray(2);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_buffer_.buffer_);
            glBindVertexArray(0);
        }
        return *this;
    }

private:
    GLuint vao_;
    // GLuint pos_vbo_;
    // GLuint tex_vbo_;
    // GLuint wei_vbo_;
    BufferGL<float, GL_ARRAY_BUFFER> pos_buffer_;
    BufferGL<float, GL_ARRAY_BUFFER> tex_buffer_;
    BufferGL<float, GL_ARRAY_BUFFER> wei_buffer_;
    BufferGL<unsigned int, GL_ELEMENT_ARRAY_BUFFER> ebo_buffer_;
    unsigned int pos_size_;
    unsigned int tri_size_;
};
