#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "backends/gl/devicegl_glad.h"
#include "backends/base/MappedView.h"

struct GLPboUnmap
{
    GLuint pbo{};
    GLenum target{}; // GL_PIXEL_PACK_BUFFER
    void operator()() const noexcept
    {
        if (!pbo)
            return;
        glBindBuffer(target, pbo);
        glUnmapBuffer(target);
        glBindBuffer(target, 0);
        glDeleteBuffers(1, &pbo); // <- delete to avoid leak
    }
};

struct GLPboUpload
{
    GLuint tex{};       // texture id
    GLenum texTarget{}; // GL_TEXTURE_2D
    GLint level{};
    GLint x{}, y{};
    GLsizei w{}, h{};
    GLenum format{}, type{};
    GLuint pbo{}; // PBO id (bound to GL_PIXEL_UNPACK_BUFFER)

    void operator()() const noexcept
    {
        if (!pbo || !tex)
            return;

        // 1) unmap first
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

        // 2) bind texture we’re updating
        glBindTexture(texTarget, tex);

        // 3) safe packing (tightly packed rows)
        GLint prevAlign = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // 4) upload from PBO (nullptr = offset 0)
        glTexSubImage2D(texTarget, level, x, y, w, h, format, type, nullptr);

        // 5) restore & cleanup
        glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
        glBindTexture(texTarget, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glDeleteBuffers(1, &pbo);
    }
};

// ----------------- TextureGL -----------------
template <typename Type>
class TextureGL
{
public:
    using size_type = std::size_t;

    TextureGL() = default;

    TextureGL(int width, int height, Type nodata_value)
    {
        init_(width, height, nodata_value, /*data=*/nullptr);
    }

    TextureGL(int width, int height, Type nodata_value, const Type *data)
    {
        init_(width, height, nodata_value, data);
    }

    // Move-only
    ~TextureGL()
    {
        if (tex_)
            glDeleteTextures(1, &tex_);
    }
    TextureGL(TextureGL &&o) noexcept
        : tex_(std::exchange(o.tex_, 0)),
          widths_(std::move(o.widths_)),
          heights_(std::move(o.heights_)),
          internal_(o.internal_), format_(o.format_), type_(o.type_), nodata_(o.nodata_) {}

    TextureGL &operator=(TextureGL &&o) noexcept
    {
        if (this != &o)
        {
            if (tex_)
                glDeleteTextures(1, &tex_);
            tex_ = std::exchange(o.tex_, 0);
            widths_ = std::move(o.widths_);
            heights_ = std::move(o.heights_);
            internal_ = o.internal_;
            format_ = o.format_;
            type_ = o.type_;
            nodata_ = o.nodata_;
        }
        return *this;
    }

    TextureGL(const TextureGL &) = delete;
    TextureGL &operator=(const TextureGL &) = delete;

    // Upload/Download (immutable storage friendly)
    void FromCPU(int lvl, const Type *data)
    {
        bind_();
        glTexSubImage2D(GL_TEXTURE_2D, lvl, 0, 0,
                        static_cast<GLsizei>(width(lvl)),
                        static_cast<GLsizei>(height(lvl)),
                        format_, type_, data);
        // If you updated base level and want chain rebuilt:
        if (lvl == 0)
            glGenerateMipmap(GL_TEXTURE_2D);
        unbind_();
    }

    void ToCPU(int lvl, Type *out) const
    {
        bind_();
        // For GL 4.5 you could use glGetTextureSubImage without binding.
        glGetTexImage(GL_TEXTURE_2D, lvl, format_, type_, out);
        unbind_();
    }

    [[nodiscard]] MappedView<const Type, GLPboUnmap> MapRead(int lvl) const
    {
        const GLsizeiptr bytes = GLsizeiptr(size(lvl)) * GLsizeiptr(sizeof(Type));
        GLuint pbo = 0;
        glGenBuffers(1, &pbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_PACK_BUFFER, bytes, nullptr, GL_STREAM_READ);

        bind_();
        glGetTexImage(GL_TEXTURE_2D, lvl, format_, type_, nullptr); // -> PBO
        unbind_();

        void *ptr = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, bytes, GL_MAP_READ_BIT);
        if (!ptr)
        {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            glDeleteBuffers(1, &pbo);
            throw std::runtime_error("MapRead PBO failed");
        }

        return {static_cast<const Type *>(ptr), size(lvl), GLPboUnmap{pbo, GL_PIXEL_PACK_BUFFER}};
    }

    [[nodiscard]] MappedView<Type, GLPboUpload> MapWrite(int lvl)
    {
        const GLsizei w = GLsizei(width(lvl));
        const GLsizei h = GLsizei(height(lvl));

        // Prefer bytes-per-pixel from GL format/type rather than sizeof(Type)
        const auto compSize = (type_ == GL_FLOAT ? 4 : type_ == GL_UNSIGNED_BYTE ? 1
                                                                                 :
                                                                                 /* add other types as needed */ 4);
        const auto comps = (format_ == GL_RED ? 1 : format_ == GL_RG ? 2
                                                : format_ == GL_RGB  ? 3
                                                : format_ == GL_RGBA ? 4
                                                                     :
                                                                     /* add others if needed */ 1);
        const GLsizeiptr bytes = GLsizeiptr(w) * GLsizeiptr(h) * compSize * comps;

        // If you insist on Type*: assert layout matches:
        static_assert(std::is_trivially_copyable<Type>::value, "Type must be POD-like");
        // OPTIONAL: runtime assert to catch padding issues early
        // assert(sizeof(Type) == compSize * comps && "Type size mismatches GL format");

        GLuint pbo = 0;
        glGenBuffers(1, &pbo);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, bytes, nullptr, GL_STREAM_DRAW);
        void *ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, bytes,
                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (!ptr)
        {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glDeleteBuffers(1, &pbo);
            throw std::runtime_error("MapWrite PBO failed");
        }

        return {static_cast<Type *>(ptr), size(lvl),
                GLPboUpload{tex_, GL_TEXTURE_2D, lvl, 0, 0, w, h, format_, type_, pbo}};
    }

    // Info
    unsigned int width(int lvl) const { return static_cast<unsigned int>(widths_[lvl]); }
    unsigned int height(int lvl) const { return static_cast<unsigned int>(heights_[lvl]); }
    unsigned int size(int lvl) const { return width(lvl) * height(lvl); }
    unsigned int lvls() const { return static_cast<unsigned int>(widths_.size()); }
    Type nodata() const { return nodata_; }
    GLuint id() const { return tex_; }

    // Optional: allow changing filter/wrap to match CPU sampling
    void SetFilter(GLint minf, GLint magf)
    {
        bind_();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minf);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magf); // must be NEAREST/LINEAR
        unbind_();
    }
    void SetWrap(GLint s, GLint t)
    {
        bind_();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, t);
        unbind_();
    }

private:
    GLuint tex_ = 0;
    std::vector<std::size_t> widths_, heights_;
    GLint internal_{};
    GLenum format_{};
    GLenum type_{};
    Type nodata_{};

    static std::size_t mip_levels_(int w, int h)
    {
        std::size_t levels = 0;
        std::size_t W = static_cast<std::size_t>(w);
        std::size_t H = static_cast<std::size_t>(h);
        do
        {
            ++levels;
            W = std::max<std::size_t>(1, W >> 1);
            H = std::max<std::size_t>(1, H >> 1);
        } while (W > 1 || H > 1);
        return levels;
    }

    void compute_dims_(int w, int h)
    {
        widths_.clear();
        heights_.clear();
        std::size_t W = static_cast<std::size_t>(w);
        std::size_t H = static_cast<std::size_t>(h);
        while (true)
        {
            widths_.push_back(W);
            heights_.push_back(H);
            if (W == 1 && H == 1)
                break;
            W = std::max<std::size_t>(1, W >> 1);
            H = std::max<std::size_t>(1, H >> 1);
        }
    }

    void init_(int w, int h, Type nodata_val, const Type *base)
    {
        nodata_ = nodata_val;

        const int channels = getChannels<Type>();
        internal_ = GetGLInternalFormat(GetTypeIndex<Type>());
        format_ = GetGLFormat(channels);
        type_ = GetGLType(GetTypeIndex<Type>());

        compute_dims_(w, h);

        // Create and allocate storage
#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glCreateTextures(GL_TEXTURE_2D, 1, &tex_);
            glTextureParameteri(tex_, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(tex_, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTextureParameteri(tex_, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(tex_, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // MAG must be NEAREST/LINEAR

            glTextureStorage2D(tex_, static_cast<GLint>(lvls()), internal_, w, h);
            if (base)
            {
                glTextureSubImage2D(tex_, 0, 0, 0, w, h, format_, type_, base);
                glGenerateTextureMipmap(tex_);
            }
            glTextureParameteri(tex_, GL_TEXTURE_BASE_LEVEL, 0);
            glTextureParameteri(tex_, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(lvls()) - 1);
            return;
        }
#endif
        // Fallback: bind-based immutable or mutable allocation
#ifdef GL_ARB_texture_storage
        if (GLAD_GL_ARB_texture_storage)
        {
            glGenTextures(1, &tex_);
            glBindTexture(GL_TEXTURE_2D, tex_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glTexStorage2D(GL_TEXTURE_2D, static_cast<GLint>(lvls()), internal_, w, h);
            if (base)
            {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, format_, type_, base);
                glGenerateMipmap(GL_TEXTURE_2D);
            }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(lvls()) - 1);
            glBindTexture(GL_TEXTURE_2D, 0);
            return;
        }
#endif
        // Mutable fallback
        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Allocate level 0, and each mip to keep driver happy
        glTexImage2D(GL_TEXTURE_2D, 0, internal_, w, h, 0, format_, type_, base);
        for (int lvl = 1, W = std::max(1, w >> 1), H = std::max(1, h >> 1);
             lvl < static_cast<int>(lvls());
             ++lvl, W = std::max(1, W >> 1), H = std::max(1, H >> 1))
        {
            glTexImage2D(GL_TEXTURE_2D, lvl, internal_, W, H, 0, format_, type_, nullptr);
        }
        if (base)
            glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(lvls()) - 1);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void bind_() const { glBindTexture(GL_TEXTURE_2D, tex_); }
    void unbind_() const { glBindTexture(GL_TEXTURE_2D, 0); }
};
