#pragma once
#include <vector>
#include <cstdint>
#include <utility>
#include <cassert>
#include <algorithm>

#include "backends/gl/devicegl_glad.h"
#include "core/delaunaytriangulation.h"

class MeshGL {
public:
    using index_type = std::uint32_t;

    // --- Default: screen-aligned quad (two tris), z=1 ---
    MeshGL() {
        static constexpr float kPos[] = {
            -1.f,  1.f, 1.f,   -1.f, -1.f, 1.f,    1.f, -1.f, 1.f,
            -1.f,  1.f, 1.f,    1.f, -1.f, 1.f,    1.f,  1.f, 1.f
        };
        static constexpr float kUV[]  = {
             0.f, 1.f,   0.f, 0.f,   1.f, 0.f,
             0.f, 1.f,   1.f, 0.f,   1.f, 1.f
        };
        static constexpr float kW[]   = { 1.f,1.f,1.f,1.f,1.f,1.f };
        static constexpr index_type kIdx[] = { 0,1,2, 3,4,5 };

        init_(kPos, std::size(kPos),
              kUV , std::size(kUV),
              kW  , std::size(kW),
              kIdx, std::size(kIdx));
    }

    // Build from arrays; if indices empty, we triangulate UVs.
    MeshGL(const std::vector<float>& positions,   // 3 floats/vertex
           const std::vector<float>& texcoords,   // 2 floats/vertex
           const std::vector<float>& weights,     // 1 float /vertex
           const std::vector<index_type>& indices = {})
    {
        std::vector<index_type> idx;
        if (indices.empty()) {
            std::vector<unsigned int> tris = BuildTriangles(texcoords);
            idx.assign(tris.begin(), tris.end());
        } else {
            idx = indices;
        }
        init_(positions.data(), positions.size(),
              texcoords.data(), texcoords.size(),
              weights.data(),   weights.size(),
              idx.data(),       idx.size());
    }

    // --- Move-only RAII ---
    ~MeshGL() { destroy_(); }

    MeshGL(MeshGL&& o) noexcept { *this = std::move(o); }
    MeshGL& operator=(MeshGL&& o) noexcept {
        if (this != &o) {
            destroy_();
            vao_ = std::exchange(o.vao_, 0);
            vbo_pos_ = std::exchange(o.vbo_pos_, 0);
            vbo_uv_  = std::exchange(o.vbo_uv_, 0);
            vbo_w_   = std::exchange(o.vbo_w_, 0);
            ebo_     = std::exchange(o.ebo_, 0);
            index_count_  = std::exchange(o.index_count_, 0);
            vertex_count_ = std::exchange(o.vertex_count_, 0);
        }
        return *this;
    }

    MeshGL(const MeshGL&) = delete;
    MeshGL& operator=(const MeshGL&) = delete;

    // --- Draw ---
    void bind()   const { glBindVertexArray(vao_); }
    void unbind() const { glBindVertexArray(0); }

    void draw() const {
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count_), GL_UNSIGNED_INT, (void*)0);
        glBindVertexArray(0);
    }

    // --- Updates (whole-buffer) ---
    void update_positions(const float* data, std::size_t count) {
        assert(count == vertex_count_ * 3);
        buffer_sub_data_(vbo_pos_, data, bytes(count));
    }
    void update_texcoords(const float* data, std::size_t count) {
        assert(count == vertex_count_ * 2);
        buffer_sub_data_(vbo_uv_, data, bytes(count));
    }
    void update_weights(const float* data, std::size_t count) {
        assert(count == vertex_count_);
        buffer_sub_data_(vbo_w_, data, bytes(count));
    }
    void update_indices(const index_type* data, std::size_t count) {
        index_count_ = count;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, bytes(count), data);
    }

    // --- Info ---
    std::size_t vertex_count() const noexcept { return vertex_count_; }
    std::size_t index_count()  const noexcept { return index_count_; }
    GLuint vao() const noexcept { return vao_; }

private:
    // Attribute locations (match your shaders)
    static constexpr GLuint ATTR_POS = 0;
    static constexpr GLuint ATTR_UV  = 1;
    static constexpr GLuint ATTR_W   = 2;

    GLuint vao_      = 0;
    GLuint vbo_pos_  = 0;
    GLuint vbo_uv_   = 0;
    GLuint vbo_w_    = 0;
    GLuint ebo_      = 0;

    std::size_t index_count_  = 0;
    std::size_t vertex_count_ = 0;

    // Helpers
    static GLsizeiptr bytes(std::size_t count) {
        return static_cast<GLsizeiptr>(count * sizeof(float));
    }
    static GLsizeiptr bytes_indices(std::size_t count) {
        return static_cast<GLsizeiptr>(count * sizeof(index_type));
    }

    void destroy_() {
        if (ebo_)     glDeleteBuffers(1, &ebo_);
        if (vbo_w_)   glDeleteBuffers(1, &vbo_w_);
        if (vbo_uv_)  glDeleteBuffers(1, &vbo_uv_);
        if (vbo_pos_) glDeleteBuffers(1, &vbo_pos_);
        if (vao_)     glDeleteVertexArrays(1, &vao_);
        vao_ = vbo_pos_ = vbo_uv_ = vbo_w_ = ebo_ = 0;
        index_count_ = vertex_count_ = 0;
    }

    void validate_(std::size_t posN, std::size_t uvN, std::size_t wN, std::size_t idxN) {
        assert(posN % 3 == 0);
        const std::size_t nverts = posN / 3;
        assert(uvN % 2 == 0 && uvN / 2 == nverts);
        assert(wN == nverts);
        assert(idxN % 3 == 0);
        vertex_count_ = nverts;
        index_count_  = idxN;
    }

    void init_(const float* pos, std::size_t posN,
               const float* uvs, std::size_t uvN,
               const float* wts, std::size_t wN,
               const index_type* idx, std::size_t idxN)
    {
        validate_(posN, uvN, wN, idxN);

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5) {
            // DSA path: no bind-side effects
            glCreateVertexArrays(1, &vao_);
            glCreateBuffers(1, &vbo_pos_);
            glCreateBuffers(1, &vbo_uv_);
            glCreateBuffers(1, &vbo_w_);
            glCreateBuffers(1, &ebo_);

            glNamedBufferData(vbo_pos_, bytes(posN), pos, GL_STATIC_DRAW);
            glNamedBufferData(vbo_uv_,  bytes(uvN),  uvs, GL_STATIC_DRAW);
            glNamedBufferData(vbo_w_,   bytes(wN),   wts, GL_STATIC_DRAW);
            glNamedBufferData(ebo_,     bytes_indices(idxN), idx, GL_STATIC_DRAW);

            // Bind VBOs to VAO binding points 0/1/2
            glVertexArrayVertexBuffer(vao_, 0, vbo_pos_, 0, sizeof(float) * 3);
            glVertexArrayVertexBuffer(vao_, 1, vbo_uv_,  0, sizeof(float) * 2);
            glVertexArrayVertexBuffer(vao_, 2, vbo_w_,   0, sizeof(float) * 1);

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

            glVertexArrayElementBuffer(vao_, ebo_);
            return;
        }
#endif
        // Fallback path (GL 3.3 core): bind-based
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_pos_);
        glGenBuffers(1, &vbo_uv_);
        glGenBuffers(1, &vbo_w_);
        glGenBuffers(1, &ebo_);

        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_);
        glBufferData(GL_ARRAY_BUFFER, bytes(posN), pos, GL_STATIC_DRAW);
        glEnableVertexAttribArray(ATTR_POS);
        glVertexAttribPointer(ATTR_POS, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_uv_);
        glBufferData(GL_ARRAY_BUFFER, bytes(uvN), uvs, GL_STATIC_DRAW);
        glEnableVertexAttribArray(ATTR_UV);
        glVertexAttribPointer(ATTR_UV, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_w_);
        glBufferData(GL_ARRAY_BUFFER, bytes(wN), wts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(ATTR_W);
        glVertexAttribPointer(ATTR_W, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, bytes_indices(idxN), idx, GL_STATIC_DRAW);

        glBindVertexArray(0);
    }

    void buffer_sub_data_(GLuint buf, const void* src, GLsizeiptr nbytes) {
#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5) {
            glNamedBufferSubData(buf, 0, nbytes, src);
            return;
        }
#endif
        // we don't know the last ARRAY_BUFFER binding; do a local bind/update/unbind
        GLint prev = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev);
        glBindBuffer(GL_ARRAY_BUFFER, buf);
        glBufferSubData(GL_ARRAY_BUFFER, 0, nbytes, src);
        glBindBuffer(GL_ARRAY_BUFFER, prev);
    }
};
