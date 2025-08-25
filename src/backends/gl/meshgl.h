#pragma once
#include <vector>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <cstring> // memcpy

#include "backends/gl/devicegl_glad.h"
#include "core/delaunaytriangulation.h"
#include "backends/gl/buffergl.h" // your BufferGL
#include "backends/base/MappedView.h"

class MeshGL
{
public:
    using index_type = std::uint32_t;

    MeshGL(const std::vector<float> &positions, // 3 floats/vertex
           const std::vector<float> &texcoords, // 2 floats/vertex
           const std::vector<float> &weights,   // 1 float /vertex
           const std::vector<index_type> &indices = {})
    {
        std::vector<index_type> idx =
            indices.empty() ? [&]
        {
            std::vector<unsigned int> tris = BuildTriangles(texcoords);
            return std::vector<index_type>(tris.begin(), tris.end());
        }()
                            : indices;

        validate_(positions.size(), texcoords.size(), weights.size(), idx.size());

        // Create and fill buffers
        vbo_pos_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(positions.size(), positions.data());
        vbo_uv_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(texcoords.size(), texcoords.data());
        vbo_w_ = BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(weights.size(), weights.data());
        ebo_ = BufferGL<index_type, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW>(idx.size(), idx.data());

        create_vao_();
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
            vertex_count_ = std::exchange(o.vertex_count_, 0);
            index_count_ = std::exchange(o.index_count_, 0);
            vbo_pos_ = std::move(o.vbo_pos_);
            vbo_uv_ = std::move(o.vbo_uv_);
            vbo_w_ = std::move(o.vbo_w_);
            ebo_ = std::move(o.ebo_);
        }
        return *this;
    }

    /*
    // --- Whole-buffer updates via MapWrite (no global binds) ---
    void update_positions(const float *data, std::size_t count)
    {
        assert(count == vertex_count_ * 3);
        auto m = vbo_pos_.MapWrite();
        std::memcpy(m.data(), data, count * sizeof(float));
    }
    void update_texcoords(const float *data, std::size_t count)
    {
        assert(count == vertex_count_ * 2);
        auto m = vbo_uv_.MapWrite();
        std::memcpy(m.data(), data, count * sizeof(float));
    }
    void update_weights(const float *data, std::size_t count)
    {
        assert(count == vertex_count_);
        auto m = vbo_w_.MapWrite();
        std::memcpy(m.data(), data, count * sizeof(float));
    }
    void update_indices(const index_type *data, std::size_t count)
    {
        index_count_ = count;
        auto m = ebo_.MapWrite();
        std::memcpy(m.data(), data, count * sizeof(index_type));
    }
    */

    // Cross-backend style mapped views (avoid storing the view)
    [[nodiscard]] MappedView<const float, GLUnmap> MapReadPositions() const & { return vbo_pos_.MapRead(); }
    [[nodiscard]] MappedView<const float, GLUnmap> MapReadTexcoords() const & { return vbo_uv_.MapRead(); }
    [[nodiscard]] MappedView<const float, GLUnmap> MapReadWeights() const & { return vbo_w_.MapRead(); }
    [[nodiscard]] MappedView<const index_type, GLUnmap> MapReadIndices() const & { return ebo_.MapRead(); }

    [[nodiscard]] MappedView<float, GLUnmap> MapWritePositions() { return vbo_pos_.MapWrite(); }
    [[nodiscard]] MappedView<float, GLUnmap> MapWriteTexcoords() { return vbo_uv_.MapWrite(); }
    [[nodiscard]] MappedView<float, GLUnmap> MapWriteWeights() { return vbo_w_.MapWrite(); }
    [[nodiscard]] MappedView<index_type, GLUnmap> MapWriteIndices() { return ebo_.MapWrite(); }

    // --- Info ---
    std::size_t vertex_count() const noexcept { return vertex_count_; }
    std::size_t index_count() const noexcept { return index_count_; }
    std::size_t triangle_count() const noexcept { return index_count() / 3; }

private:
    template <typename InTexType, typename OutTexType>
    friend class BaseRendererGL;

    // Attribute locations (match your shaders)
    static constexpr GLuint ATTR_POS = 0;
    static constexpr GLuint ATTR_UV = 1;
    static constexpr GLuint ATTR_W = 2;

    GLuint vao_ = 0;

    BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW> vbo_pos_;
    BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW> vbo_uv_;
    BufferGL<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW> vbo_w_;
    BufferGL<index_type, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW> ebo_;

    std::size_t vertex_count_ = 0;
    std::size_t index_count_ = 0;

    GLuint vao() const noexcept { return vao_; }

    // --- Draw ---

    void bind() const { glBindVertexArray(vao_); }
    void unbind() const { glBindVertexArray(0); }

    void draw() const
    {
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(index_count_),
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
        vertex_count_ = nverts;
        index_count_ = idxN;
    }

    void destroy_vao_()
    {
        if (vao_)
            glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }

    void create_vao_()
    {

        /*
#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5) {
            glCreateVertexArrays(1, &vao_);

            // Bind VBOs to VAO binding points 0/1/2 (stride in bytes)
            glVertexArrayVertexBuffer(vao_, 0, vbo_pos_.id(), 0, sizeof(float) * 3);
            glVertexArrayVertexBuffer(vao_, 1, vbo_uv_.id(),  0, sizeof(float) * 2);
            glVertexArrayVertexBuffer(vao_, 2, vbo_w_.id(),   0, sizeof(float) * 1);

            // Describe formats
            glEnableVertexArrayAttrib(vao_, ATTR_POS);
            glVertexArrayAttribFormat(vao_, ATTR_POS, 3, GL_FLOAT, GL_FALSE, 0);
            glVertexArrayAttribBinding(vao_, ATTR_POS, 0);

            glEnableVertexArrayAttrib(vao_, ATTR_UV);
            glVertexArrayAttribFormat(vao_, ATTR_UV, 2, GL_FLOAT, GL_FALSE, 0);
            glVertexArrayAttribBinding(vao_, ATTR_UV, 1);

            glEnableVertexArrayAttrib(vao_, ATTR_W);
            glVertexArrayAttribFormat(vao_, ATTR_W, 1, GL_FLOAT, GL_FALSE, 0);
            glVertexArrayAttribBinding(vao_, ATTR_W, 2);

            // Index buffer
            glVertexArrayElementBuffer(vao_, ebo_.id());
            return;
        }
#endif
*/
        // GL 3.3 fallback
        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_.id());
        glEnableVertexAttribArray(ATTR_POS);
        glVertexAttribPointer(ATTR_POS, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_uv_.id());
        glEnableVertexAttribArray(ATTR_UV);
        glVertexAttribPointer(ATTR_UV, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_w_.id());
        glEnableVertexAttribArray(ATTR_W);
        glVertexAttribPointer(ATTR_W, 1, GL_FLOAT, GL_FALSE, 0, (void *)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_.id());

        glBindVertexArray(0);
    }
};
