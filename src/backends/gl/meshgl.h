#pragma once
//#include <vector>
//#include <cstdint>
//#include <cassert>
//#include <algorithm>
//#include <cstring> // memcpy

#include <Eigen/Core>
//#include "backends/gl/devicegl_glad.h"
//#include "core/delaunaytriangulation.h"
#include "backends/gl/buffergl.h" // your BufferGL
#include "backends/base/MappedView.h"

class MeshGL
{
public:
    MeshGL(const std::vector<Eigen::Vector3f> &positions, // 3 floats/vertex
           const std::vector<unsigned int> &indices)
    {
        // validate_(positions.size(), texcoords.size(), weights.size(), indices.size());

        // Create and fill buffers
        vbo_pos_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(positions.size() * 3, (const float *)&positions[0]);
        ebo_ = BufferGL<unsigned int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW>(indices.size(), indices.data());

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_.id());
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_.id());

        glBindVertexArray(0);
    }

    MeshGL(const std::vector<Eigen::Vector3f> &positions, // 3 floats/vertex
           const std::vector<Eigen::Vector2f> &texcoords, // 2 floats/vertex
           const std::vector<unsigned int> &indices)
    {
        // validate_(positions.size(), texcoords.size(), weights.size(), indices.size());

        // Create and fill buffers
        vbo_pos_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(positions.size() * 3, (const float *)&positions[0]);
        vbo_uv_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(texcoords.size() * 2, (const float *)&texcoords[0]);
        ebo_ = BufferGL<unsigned int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW>(indices.size(), indices.data());

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_.id());
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_uv_.id());
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_.id());

        glBindVertexArray(0);
    }

    MeshGL(const std::vector<Eigen::Vector3f> &positions, // 3 floats/vertex
           const std::vector<Eigen::Vector3f> &normals,   // 3 floats/vertex
           const std::vector<Eigen::Vector2f> &texcoords, // 2 floats/vertex
           const std::vector<unsigned int> &indices)
    {
        // validate_(positions.size(), texcoords.size(), weights.size(), indices.size());

        // Create and fill buffers
        vbo_pos_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(positions.size() * 3, (const float *)&positions[0]);
        vbo_nor_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(normals.size() * 3, (const float *)&normals[0]);
        vbo_uv_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(texcoords.size() * 2, (const float *)&texcoords[0]);
        ebo_ = BufferGL<unsigned int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW>(indices.size(), indices.data());

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_.id());
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_nor_.id());
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_uv_.id());
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_.id());

        glBindVertexArray(0);
    }

    MeshGL(const std::vector<float> &positions, // 3 floats/vertex
           const std::vector<unsigned int> &indices)
    {
        // validate_(positions.size(), texcoords.size(), weights.size(), indices.size());

        // Create and fill buffers
        vbo_pos_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(positions.size(), positions.data());
        ebo_ = BufferGL<unsigned int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW>(indices.size(), indices.data());

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_.id());
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_.id());

        glBindVertexArray(0);
    }

    MeshGL(const std::vector<float> &positions, // 3 floats/vertex
           const std::vector<float> &texcoords, // 2 floats/vertex
           const std::vector<unsigned int> &indices)
    {
        // validate_(positions.size(), texcoords.size(), weights.size(), indices.size());

        // Create and fill buffers
        vbo_pos_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(positions.size(), positions.data());
        vbo_uv_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(texcoords.size(), texcoords.data());
        ebo_ = BufferGL<unsigned int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW>(indices.size(), indices.data());

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_.id());
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_uv_.id());
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_.id());

        glBindVertexArray(0);
    }

    MeshGL(const std::vector<float> &positions, // 3 floats/vertex
           const std::vector<float> &normals,   // 3 floats/vertex
           const std::vector<float> &texcoords, // 2 floats/vertex
           const std::vector<unsigned int> &indices)
    {
        // validate_(positions.size(), texcoords.size(), weights.size(), indices.size());

        // Create and fill buffers
        vbo_pos_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(positions.size(), positions.data());
        vbo_nor_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(normals.size(), normals.data());
        vbo_uv_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(texcoords.size(), texcoords.data());
        ebo_ = BufferGL<unsigned int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW>(indices.size(), indices.data());

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_.id());
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_nor_.id());
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_uv_.id());
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_.id());

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
            vbo_pos_ = std::move(o.vbo_pos_);
            vbo_uv_ = std::move(o.vbo_uv_);
            vbo_w_ = std::move(o.vbo_w_);
            ebo_ = std::move(o.ebo_);
        }
        return *this;
    }

    // Cross-backend style mapped views (avoid storing the view)
    [[nodiscard]] MappedView<const float, GLUnmap> MapReadPositions() const & { return vbo_pos_.MapRead(); }
    [[nodiscard]] MappedView<const float, GLUnmap> MapReadTexcoords() const & { return vbo_uv_.MapRead(); }
    [[nodiscard]] MappedView<const float, GLUnmap> MapReadWeights() const & { return vbo_w_.MapRead(); }
    [[nodiscard]] MappedView<const unsigned int, GLUnmap> MapReadIndices() const & { return ebo_.MapRead(); }

    [[nodiscard]] MappedView<float, GLUnmap> MapWritePositions() { return vbo_pos_.MapWrite(); }
    [[nodiscard]] MappedView<float, GLUnmap> MapWriteTexcoords() { return vbo_uv_.MapWrite(); }
    [[nodiscard]] MappedView<float, GLUnmap> MapWriteWeights() { return vbo_w_.MapWrite(); }
    [[nodiscard]] MappedView<unsigned int, GLUnmap> MapWriteIndices() { return ebo_.MapWrite(); }

    // --- Info ---
    std::size_t vertex_count() const noexcept { return vbo_pos_.size() / 3; }
    std::size_t index_count() const noexcept { return ebo_.size(); }
    std::size_t triangle_count() const noexcept { return index_count() / 3; }

private:
    friend class SimpleExampleRendererGL;
    friend class GouraudRendererGL;
    friend class DepthRendererGL;
    friend class ImageRendererGL;
    friend class ResidualRendererGL;
    friend class L2RendererGL;
    friend class DIDxyRendererGL;
    friend class JPoseRendererGL;
    friend class JMapRendererGL;
    friend class DiffRendererGL;

    void draw() const
    {
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(ebo_.size()),
                       GL_UNSIGNED_INT,
                       (void *)0);
        glBindVertexArray(0);
    }

    void validate_(std::size_t posN, std::size_t uvN, std::size_t wN, std::size_t idxN)
    {
        assert(posN % 3 == 0);
        const std::size_t nverts = posN / 3;
        assert(uvN % 2 == 0 && uvN / 2 == nverts);
        assert(wN == nverts);
        assert(idxN % 3 == 0);
    }

    void destroy_vao_()
    {
        if (vao_)
            glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }

    GLuint vao_ = 0;

    BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW> vbo_pos_;
    BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW> vbo_nor_;
    BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW> vbo_uv_;
    BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW> vbo_w_;
    BufferGL<unsigned int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW> ebo_;
};
