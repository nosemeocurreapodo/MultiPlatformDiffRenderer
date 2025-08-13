#pragma once

#include "backends/gl/devicegl_glad.h"
#include "core/delaunaytriangulation.h"

class MeshGL
{
    template <typename InTexType, typename OutTexType>
    friend class BaseRendererGL;
    friend class ImageRendererGL;

public:
    MeshGL(std::vector<float> &vertices, std::vector<float> &tex_coords, std::vector<float> &weights)
    {
        std::vector<unsigned int> triangles = BuildTriangles(tex_coords);

        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &pos_bo_);
        glGenBuffers(1, &tex_bo_);
        glGenBuffers(1, &wei_bo_);
        glGenBuffers(1, &ebo_);

        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, pos_bo_);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, tex_bo_);
        glBufferData(GL_ARRAY_BUFFER, tex_coords.size() * sizeof(float), tex_coords.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, wei_bo_);
        glBufferData(GL_ARRAY_BUFFER, weights.size() * sizeof(float), weights.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangles.size() * sizeof(unsigned int), triangles.data(), GL_STATIC_DRAW);
        
        glBindVertexArray(0);
        
        ebo_size_ = triangles.size();
    };

    /*
    MeshGPU(const MeshGPU &other)
    {
    }

    MeshGPU &operator=(const MeshGPU &other)
    {
        if (this != &other)
        {
        }
        return *this;
    }
    */

protected:
    GLuint vao_;
    GLuint pos_bo_;
    GLuint tex_bo_;
    GLuint wei_bo_;
    GLuint ebo_;

    int ebo_size_;
};
