#pragma once

#include <Eigen/Core>
#include <opencv2/opencv.hpp>

#include "backends/gles2/buffergles2.h"      // BufferGLES2<T, Target, Usage>
#include "backends/base/MappedView.h"
// #include "backends/gl/texturegles2.h"   // wherever your TextureGLES2 lives
// #include "backends/gl/devicegles2_glad.h" // glad ES2 loader (included transitively in BufferGLES2/TextureGLES2)

// Forward-declare your texture type if needed:
template <typename T> class TextureGLES2;

class MeshGLES2
{
public:
    MeshGLES2(const std::vector<float> &vertex,
              const std::vector<unsigned int> &indices,
              const cv::Mat &diffuse,
              bool has_position = true,
              bool has_texcoord = true,
              bool has_normal  = true)
    {
        // ---- Layout: compute offsets/stride in BYTES based on enabled attrs ----
        has_pos_ = has_position;
        has_uv_  = has_texcoord;
        has_nor_ = has_normal;

        stride_ = 0;
        if (has_pos_)  { pos_offset_ = stride_;      stride_ += 3 * int(sizeof(float)); }
        if (has_uv_)   { tex_offset_ = stride_;      stride_ += 2 * int(sizeof(float)); }
        if (has_nor_)  { nor_offset_ = stride_;      stride_ += 3 * int(sizeof(float)); }

        // ---- Buffers ----
        vbo_vertex_ = BufferGLES2<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>(vertex.size(), vertex.data());

        // Pick index type: prefer 32-bit if OES_element_index_uint, else convert to 16-bit
        have_uint_indices_ = 
#if defined(GLAD_GL_OES_element_index_uint)
            GLAD_GL_OES_element_index_uint
#else
            false
#endif
            ;

        if (have_uint_indices_) {
            index_type_  = GL_UNSIGNED_INT;
            index_count_ = indices.size();
            ebo32_ = BufferGLES2<unsigned int, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW>(indices.size(), indices.data());
        } else {
            // Downcast to 16-bit, validate range
            unsigned int max_index = 0;
            for (unsigned int v : indices) { if (v > max_index) max_index = v; }
            if (max_index > 65535u) {
                throw std::runtime_error("MeshGLES2: 32-bit indices required but GL_OES_element_index_uint not supported.");
            }
            std::vector<unsigned short> idx16; idx16.reserve(indices.size());
            for (unsigned int v : indices) idx16.push_back(static_cast<unsigned short>(v));
            index_type_  = GL_UNSIGNED_SHORT;
            index_count_ = idx16.size();
            ebo16_ = BufferGLES2<unsigned short, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW>(idx16.size(), idx16.data());
        }

        // ---- VAO (optional): OES_vertex_array_object on ES2) ----
        have_vao_ =
#if defined(GLAD_GL_OES_vertex_array_object)
            GLAD_GL_OES_vertex_array_object
#else
            false
#endif
            ;

        if (have_vao_) {
#if defined(GLAD_GL_OES_vertex_array_object)
            glGenVertexArraysOES(1, &vao_);
            glBindVertexArrayOES(vao_);
            setup_vertex_attribs_(); // binds VBO/EBO and sets pointers
            glBindVertexArrayOES(0);
#endif
        }

        // ---- Diffuse texture (uses your ES2-safe TextureGLES2) ----
        diffuse_ = TextureGLES2<float>(diffuse.cols, diffuse.rows, -1.0f,
                                       reinterpret_cast<const float*>(diffuse.ptr()));
        diffuse_.generate_mipmaps(0);
    }

    ~MeshGLES2() { destroy_vao_(); }

    MeshGLES2(const MeshGLES2&) = delete;
    MeshGLES2& operator=(const MeshGLES2&) = delete;

    MeshGLES2(MeshGLES2&& o) noexcept { *this = std::move(o); }
    MeshGLES2& operator=(MeshGLES2&& o) noexcept
    {
        if (this != &o)
        {
            destroy_vao_();
            vao_         = std::exchange(o.vao_, 0);
            have_vao_    = o.have_vao_;
            have_uint_indices_ = o.have_uint_indices_;
            index_type_  = o.index_type_;
            index_count_ = o.index_count_;

            vbo_vertex_  = std::move(o.vbo_vertex_);
            ebo32_       = std::move(o.ebo32_);
            ebo16_       = std::move(o.ebo16_);
            diffuse_     = std::move(o.diffuse_);

            stride_      = o.stride_;
            pos_offset_  = o.pos_offset_;
            tex_offset_  = o.tex_offset_;
            nor_offset_  = o.nor_offset_;
            has_pos_     = o.has_pos_;
            has_uv_      = o.has_uv_;
            has_nor_     = o.has_nor_;
        }
        return *this;
    }

    // --- Info ---
    std::size_t vertex_count()  const noexcept { return stride_ ? (vbo_vertex_.size() * sizeof(float)) / std::size_t(stride_) : 0; }
    std::size_t index_count()   const noexcept { return index_count_; }
    std::size_t triangle_count()const noexcept { return index_count_ / 3; }

    void draw() const
    {
        if (have_vao_) {
#if defined(GLAD_GL_OES_vertex_array_object)
            glBindVertexArrayOES(vao_);
            glDrawElements(GL_TRIANGLES, GLsizei(index_count_), index_type_, (void*)0);
            glBindVertexArrayOES(0);
#endif
        } else {
            // ES2 fallback: bind buffers & set pointers each draw
            bind_index_() ;
            glBindBuffer(GL_ARRAY_BUFFER, vbo_vertex_.id());

            GLint attrib = 0;
            if (has_pos_) {
                glEnableVertexAttribArray(attrib);
                glVertexAttribPointer(attrib, 3, GL_FLOAT, GL_FALSE, stride_, reinterpret_cast<const void*>(std::intptr_t(pos_offset_)));
                ++attrib;
            }
            if (has_uv_) {
                glEnableVertexAttribArray(attrib);
                glVertexAttribPointer(attrib, 2, GL_FLOAT, GL_FALSE, stride_, reinterpret_cast<const void*>(std::intptr_t(tex_offset_)));
                ++attrib;
            }
            if (has_nor_) {
                glEnableVertexAttribArray(attrib);
                glVertexAttribPointer(attrib, 3, GL_FLOAT, GL_FALSE, stride_, reinterpret_cast<const void*>(std::intptr_t(nor_offset_)));
                ++attrib;
            }

            glDrawElements(GL_TRIANGLES, GLsizei(index_count_), index_type_, (void*)0);

            // Optionally disable to be tidy (ES2 state leaks can confuse other code)
            // for (GLint i = 0; i < attrib; ++i) glDisableVertexAttribArray(i);
        }
    }

    TextureGLES2<float> &diffuse() noexcept { return diffuse_; }
    const TextureGLES2<float> &diffuse() const noexcept { return diffuse_; }

private:
    void setup_vertex_attribs_()
    {
        bind_index_();
        glBindBuffer(GL_ARRAY_BUFFER, vbo_vertex_.id());

        GLint attrib = 0;
        if (has_pos_) {
            glEnableVertexAttribArray(attrib);
            glVertexAttribPointer(attrib, 3, GL_FLOAT, GL_FALSE, stride_, reinterpret_cast<const void*>(std::intptr_t(pos_offset_)));
            ++attrib;
        }
        if (has_uv_) {
            glEnableVertexAttribArray(attrib);
            glVertexAttribPointer(attrib, 2, GL_FLOAT, GL_FALSE, stride_, reinterpret_cast<const void*>(std::intptr_t(tex_offset_)));
            ++attrib;
        }
        if (has_nor_) {
            glEnableVertexAttribArray(attrib);
            glVertexAttribPointer(attrib, 3, GL_FLOAT, GL_FALSE, stride_, reinterpret_cast<const void*>(std::intptr_t(nor_offset_)));
        }
    }

    void bind_index_() const
    {
        if (have_uint_indices_) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo32_.id());
        else                     glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo16_.id());
    }

    void destroy_vao_()
    {
        if (vao_ && have_vao_) {
#if defined(GLAD_GL_OES_vertex_array_object)
            glDeleteVertexArraysOES(1, &vao_);
#endif
        }
        vao_ = 0;
    }

    // geometry
    BufferGLES2<float, GL_ARRAY_BUFFER, GL_STATIC_DRAW>            vbo_vertex_{};
    BufferGLES2<unsigned int,   GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW> ebo32_{};
    BufferGLES2<unsigned short, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW> ebo16_{};
    GLenum      index_type_{ GL_UNSIGNED_SHORT };
    std::size_t index_count_{ 0 };
    bool        have_uint_indices_{ false };

    // texture
    TextureGLES2<float> diffuse_{};

    // layout/state
    int  stride_ = 0;             // bytes
    int  pos_offset_ = -1;        // bytes
    int  tex_offset_ = -1;        // bytes
    int  nor_offset_ = -1;        // bytes
    bool has_pos_ = true, has_uv_ = true, has_nor_ = true;

    // VAO (OES)
    GLuint vao_ = 0;
    bool   have_vao_ = false;
};
