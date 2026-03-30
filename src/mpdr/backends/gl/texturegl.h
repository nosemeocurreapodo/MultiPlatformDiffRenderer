#pragma once
#include <algorithm>
// #include <cassert>
// #include <cstddef>
// #include <stdexcept>
// #include <utility>
// #include <vector>

#include "mpdr/backends/gl/devicegl_glad.h"
#include "mpdr/backends/base/mappedviewbase.h"
#include "mpdr/backends/gl/format_converters.h"
#include "mpdr/common/cv_converters.h"

struct GLPboUnmap
{
    /*
    GLPboUnmap() : pbo(0), target(0) {};
    GLPboUnmap(GLuint _pbo, GLenum _target) : pbo(_pbo), target(_target) {};
    */
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
    /*
    GLPboUpload() : tex(0),
                    texTarget(0),
                    level(0),
                    x(0), y(0),
                    w(0), h(0),
                    format(0), T(0),
                    pbo(0) {};

    GLPboUpload(GLuint _tex,
                GLenum _texTarget,
                GLint _level,
                GLint _x, GLint _y,
                GLsizei _w, GLsizei _h,
                GLenum _format, GLenum _T,
                GLuint _pbo) : tex(_tex),
                               texTarget(_texTarget),
                               level(_level),
                               x(_x), y(_y),
                               w(_w), h(_h),
                               format(_format), T(_T),
                               pbo(_pbo) {};
                               */

    GLuint tex{};       // texture id
    GLenum texTarget{}; // GL_TEXTURE_2D
    GLint level{};
    GLint x{}, y{};
    GLsizei w{}, h{};
    GLenum format{}, T{};
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
        glTexSubImage2D(texTarget, level, x, y, w, h, format, T, nullptr);

        // 5) restore
        glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);

        // cleanip
        glBindTexture(texTarget, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glDeleteBuffers(1, &pbo);
    }
};

template <typename T>
using TextureViewReadGL = TextureViewBase<T, GLPboUnmap>;

template <typename T>
using TextureViewWriteGL = TextureViewBase<T, GLPboUpload>;

// ----------------- TextureGL -----------------
template <typename T>
class TextureGL
{
public:
    // TextureGL() = default;

    TextureGL(unsigned int width, unsigned int height, T nodata_value)
    {
        init_(width, height, nodata_value, /*data=*/nullptr);
    }

    TextureGL(unsigned int width, unsigned int height, T nodata_value, const T *data)
    {
        init_(width, height, nodata_value, data);
    }

    // -----------------------------------------------------------------
    // dtor / move
    // -----------------------------------------------------------------
    ~TextureGL()
    {
        if (tex_)
            glDeleteTextures(1, &tex_);
    }

    TextureGL(TextureGL &&o) noexcept
        : tex_(std::exchange(o.tex_, 0)),
          widths_(std::move(o.widths_)),
          heights_(std::move(o.heights_)),
          internal_(o.internal_), format_(o.format_), T_(o.T_), nodata_(o.nodata_) {}

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
            T_ = o.T_;
            nodata_ = o.nodata_;
        }
        return *this;
    }

    // -----------------------------------------------------------------
    // NEW: copy ctor / copy assignment
    // -----------------------------------------------------------------
    TextureGL(const TextureGL &other)
    {
        cloneFrom(other);
    }

    TextureGL &operator=(const TextureGL &other)
    {
        if (this != &other)
        {
            TextureGL tmp(other);   // copy into a fresh texture
            *this = std::move(tmp); // reuse move assignment to swap in
        }
        return *this;
    }

    // (removed)
    // TextureGL(const TextureGL &) = delete;
    // TextureGL &operator=(const TextureGL &) = delete;

    [[nodiscard]] TextureViewReadGL<T> MapRead(int lvl) const
    {
        const GLsizeiptr bytes = GLsizeiptr(width(lvl) * height(lvl)) * GLsizeiptr(sizeof(T));
        GLuint pbo = 0;
        glGenBuffers(1, &pbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_PACK_BUFFER, bytes, nullptr, GL_STREAM_READ);

        glBindTexture(GL_TEXTURE_2D, tex_);
        glGetTexImage(GL_TEXTURE_2D, lvl, format_, T_, nullptr); // -> PBO
        glBindTexture(GL_TEXTURE_2D, 0);

        void *ptr = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, bytes, GL_MAP_READ_BIT);
        if (!ptr)
        {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            glDeleteBuffers(1, &pbo);
            throw std::runtime_error("MapRead PBO failed");
        }

        return {static_cast<T *>(ptr), width(lvl), height(lvl), nodata_, GLPboUnmap{pbo, GL_PIXEL_PACK_BUFFER}};
    }

    [[nodiscard]] TextureViewWriteGL<T> MapWrite(int lvl)
    {
        const GLsizei w = GLsizei(width(lvl));
        const GLsizei h = GLsizei(height(lvl));

        const auto compSize = (T_ == GL_FLOAT ? 4 : T_ == GL_UNSIGNED_BYTE ? 1
                                                                           :
                                                                           /* add other Ts as needed */ 4);
        const auto comps = (format_ == GL_RED ? 1 : format_ == GL_RG ? 2
                                                : format_ == GL_RGB  ? 3
                                                : format_ == GL_RGBA ? 4
                                                                     :
                                                                     /* add others if needed */ 1);
        const GLsizeiptr bytes = GLsizeiptr(w) * GLsizeiptr(h) * compSize * comps;

        //static_assert(std::is_trivially_copyable<T>::value, "T must be POD-like");

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

        return {static_cast<T *>(ptr), width(lvl), height(lvl), nodata_,
                GLPboUpload{tex_, GL_TEXTURE_2D, lvl, 0, 0, w, h, format_, T_, pbo}};
    }

    // Info
    unsigned int width(int lvl) const { return static_cast<unsigned int>(widths_[lvl]); }
    unsigned int height(int lvl) const { return static_cast<unsigned int>(heights_[lvl]); }
    unsigned int levels() const { return static_cast<unsigned int>(widths_.size()); }
    unsigned int type_size() const { return sizeof(T); };
    // std::type_index get_type_index() const { return GetTypeIndex<T>(); };
    int getOpenCVType() const
    {
        return GetOpenCVFormat<T>();
    }

    T nodata() const { return nodata_; }

    void generate_mipmaps(int base_lvl)
    {
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, base_lvl);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    [[nodiscard]] GLuint id() const noexcept
    {
        assert(tex_ != 0 && "Texture not created");
        return tex_;
    }

private:
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

    void init_(int w, int h, T nodata_val, const T *base)
    {
        nodata_ = nodata_val;

        const int channels = getChannels<T>();
        //internal_ = GetGLInternalFormat(GetTypeIndex<T>());
        internal_ = GetGLInternalFormat<T>();
        format_ = GetGLFormat(channels);
        //T_ = GetGLType(GetTypeIndex<T>());
        T_ = GetGLType<T>();

        compute_dims_(w, h);

        // Mutable fallback
        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, internal_, w, h, 0, format_, T_, base);
        for (int lvl = 1, W = std::max(1, w >> 1), H = std::max(1, h >> 1);
             lvl < static_cast<int>(levels());
             ++lvl, W = std::max(1, W >> 1), H = std::max(1, H >> 1))
        {
            glTexImage2D(GL_TEXTURE_2D, lvl, internal_, W, H, 0, format_, T_, nullptr);
        }
        if (base)
            glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(levels()) - 1);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // -----------------------------------------------------------------
    // NEW: helper used by copy ctor / copy assignment
    // -----------------------------------------------------------------
    void cloneFrom(const TextureGL &other)
    {
        // If this already owns a texture (only relevant if reused internally)
        if (tex_)
        {
            glDeleteTextures(1, &tex_);
            tex_ = 0;
        }

        // Recreate texture with same base size and nodata (no initial data)
        init_(static_cast<int>(other.width(0)),
              static_cast<int>(other.height(0)),
              other.nodata(), nullptr);

        const auto lvls = static_cast<int>(levels());
        for (int lvl = 0; lvl < lvls; ++lvl)
        {
            auto src = other.MapRead(lvl);
            auto dst = MapWrite(lvl);
            if (src.size() != dst.size())
                throw std::runtime_error("TextureGL copy: mip level size mismatch");

            std::copy_n(src.data(), src.size(), dst.data());
        }
    }

    GLuint tex_ = 0;
    std::vector<unsigned int> widths_, heights_;
    GLint internal_{};
    GLenum format_{};
    GLenum T_{};
    T nodata_{};
};
