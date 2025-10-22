#pragma once

#include "backends/gl/devicegles2_glad.h"
#include "backends/base/MappedView.h"
#include "backends/gl/format_converters.h"

#include <stdexcept>
#include <vector>
#include <type_traits>
#include <cstdlib>
#include <cstring>
#include <cassert>

// ---------------- Releasers & helpers (ES2-capable) ----------------

enum class MapBackend { NoneCPU, ES3Core, OES, EXT };   // how we mapped (or not)
enum class PBOKind   { None, Pack, Unpack };            // which PBO target

struct GLPboUnmap
{
    // For READ path
    GLuint pbo{};
    PBOKind kind{PBOKind::None};
    MapBackend map{MapBackend::NoneCPU};

    // For CPU staging (no PBO/map)
    void*  staging{nullptr};

    void operator()() const noexcept
    {
        // CPU staging only
        if (staging) { std::free(staging); return; }

        if (!pbo || kind == PBOKind::None) return;

        const GLenum target = (kind == PBOKind::Pack) ? GL_PIXEL_PACK_BUFFER : GL_PIXEL_UNPACK_BUFFER;

        glBindBuffer(target, pbo);
        switch (map) {
            case MapBackend::ES3Core:
#if defined(GLAD_GL_ES_VERSION_3_0)
                if (GLAD_GL_ES_VERSION_3_0) glUnmapBuffer(target);
#endif
                break;
            case MapBackend::OES:
#if defined(GLAD_GL_OES_mapbuffer)
                if (GLAD_GL_OES_mapbuffer) glUnmapBufferOES(target);
#endif
                break;
            case MapBackend::EXT:
                // EXT uses Unmap of whichever mapping path exists; try core then OES.
#if defined(GLAD_GL_ES_VERSION_3_0)
                if (GLAD_GL_ES_VERSION_3_0) { glUnmapBuffer(target); break; }
#endif
#if defined(GLAD_GL_OES_mapbuffer)
                if (GLAD_GL_OES_mapbuffer) { glUnmapBufferOES(target); break; }
#endif
                break;
            case MapBackend::NoneCPU: break;
        }
        glBindBuffer(target, 0);
        glDeleteBuffers(1, &pbo);
    }
};

struct GLPboUpload
{
    // For WRITE path
    GLuint tex{};        // texture id
    GLenum texTarget{};  // GL_TEXTURE_2D
    GLint  level{};
    GLint  x{}, y{};
    GLsizei w{}, h{};
    GLenum format{}, T{};

    GLuint pbo{};        // if using PBO+map
    MapBackend map{MapBackend::NoneCPU};

    // CPU staging if no PBO
    void* staging{nullptr};
    GLsizeiptr nbytes{0};

    void operator()() const noexcept
    {
        if (!tex) { if (staging) std::free(staging); return; }

        glBindTexture(texTarget, tex);
        // Pack rows tightly
        GLint prevUnpack = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevUnpack);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        if (pbo) {
            // Unmap PBO first, then upload from offset 0
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            switch (map) {
                case MapBackend::ES3Core:
#if defined(GLAD_GL_ES_VERSION_3_0)
                    if (GLAD_GL_ES_VERSION_3_0) glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
#endif
                    break;
                case MapBackend::OES:
#if defined(GLAD_GL_OES_mapbuffer)
                    if (GLAD_GL_OES_mapbuffer) glUnmapBufferOES(GL_PIXEL_UNPACK_BUFFER);
#endif
                    break;
                case MapBackend::EXT:
#if defined(GLAD_GL_ES_VERSION_3_0)
                    if (GLAD_GL_ES_VERSION_3_0) glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                    else
#endif
#if defined(GLAD_GL_OES_mapbuffer)
                    if (GLAD_GL_OES_mapbuffer) glUnmapBufferOES(GL_PIXEL_UNPACK_BUFFER);
#endif
                    break;
                case MapBackend::NoneCPU: break;
            }
            glTexSubImage2D(texTarget, level, x, y, w, h, format, T, nullptr);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glDeleteBuffers(1, &pbo);
        } else if (staging) {
            // Upload from CPU memory
            glTexSubImage2D(texTarget, level, x, y, w, h, format, T, staging);
            std::free(staging);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, prevUnpack);
        glBindTexture(texTarget, 0);
    }
};

// Utility: choose component count for simple formats
inline int comps_from_format(GLenum fmt) {
    switch (fmt) {
        case GL_ALPHA: return 1;
#if defined(GL_RED) || defined(GL_RED_EXT)
        case GL_RED: return 1;
#ifdef GL_RED_EXT
        case GL_RED_EXT: return 1;
#endif
#endif
        case GL_LUMINANCE: return 1;
#if defined(GL_RG) || defined(GL_RG_EXT)
        case GL_RG: return 2;
#ifdef GL_RG_EXT
        case GL_RG_EXT: return 2;
#endif
#endif
        case GL_LUMINANCE_ALPHA: return 2;
        case GL_RGB: return 3;
        case GL_RGBA: return 4;
        default: return 4;
    }
}

// --------------------- TextureGLES2 ---------------------

template <typename T>
class TextureGLES2
{
public:
    TextureGLES2() = default;

    TextureGLES2(int width, int height, T nodata_value) {
        init_(width, height, nodata_value, /*data=*/nullptr);
    }
    TextureGLES2(int width, int height, T nodata_value, const T* data) {
        init_(width, height, nodata_value, data);
    }

    ~TextureGLES2() {
        if (tex_) glDeleteTextures(1, &tex_);
    }
    TextureGLES2(TextureGLES2&& o) noexcept
        : tex_(std::exchange(o.tex_, 0)),
          widths_(std::move(o.widths_)),
          heights_(std::move(o.heights_)),
          internal_(o.internal_), format_(o.format_), T_(o.T_), nodata_(o.nodata_) {}

    TextureGLES2& operator=(TextureGLES2&& o) noexcept {
        if (this != &o) {
            if (tex_) glDeleteTextures(1, &tex_);
            tex_ = std::exchange(o.tex_, 0);
            widths_  = std::move(o.widths_);
            heights_ = std::move(o.heights_);
            internal_ = o.internal_;
            format_   = o.format_;
            T_        = o.T_;
            nodata_   = o.nodata_;
        }
        return *this;
    }

    TextureGLES2(const TextureGLES2&) = delete;
    TextureGLES2& operator=(const TextureGLES2&) = delete;

    // ----------- READ: ES2 has no glGetTexImage -----------
    // We implement MapRead by attaching the texture level to an FBO and calling glReadPixels.
    // If PBO+mapping exists, we map it; otherwise, we allocate CPU memory and read into it.
    [[nodiscard]] MappedView<const T, GLPboUnmap> MapRead(int lvl) const
    {
        const GLsizei w = GLsizei(width(lvl));
        const GLsizei h = GLsizei(height(lvl));
        const GLsizeiptr bytes = GLsizeiptr(w) * GLsizeiptr(h) * GLsizeiptr(sizeof(T));

        // 1) Create temp FBO and attach the requested level
        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, lvl);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &fbo);
            throw std::runtime_error("MapRead: texture level not framebuffer-complete (format might not be renderable on ES2).");
        }

        // 2) Set tight packing
        GLint prevPack = 4;
        glGetIntegerv(GL_PACK_ALIGNMENT, &prevPack);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        // 3) Try PBO+map (EXT_pixel_buffer_object + map extension)
#if defined(GLAD_GL_EXT_pixel_buffer_object)
        if (GLAD_GL_EXT_pixel_buffer_object) {
            GLuint pbo = 0;
            glGenBuffers(1, &pbo);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_PACK_BUFFER, bytes, nullptr, GL_STREAM_READ);

            // Read from FBO into PBO
            glReadPixels(0, 0, w, h, format_, T_, nullptr);

            // Try to map the PBO
#if defined(GLAD_GL_ES_VERSION_3_0)
            if (GLAD_GL_ES_VERSION_3_0) {
                void* ptr = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, bytes, GL_MAP_READ_BIT);
                if (ptr) {
                    glPixelStorei(GL_PACK_ALIGNMENT, prevPack);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glDeleteFramebuffers(1, &fbo);
                    return { static_cast<const T*>(ptr), std::size_t(w)*std::size_t(h),
                             GLPboUnmap{ pbo, PBOKind::Pack, MapBackend::ES3Core, nullptr } };
                }
            }
#endif
#if defined(GLAD_GL_EXT_map_buffer_range)
            if (GLAD_GL_EXT_map_buffer_range) {
                void* ptr = glMapBufferRangeEXT(GL_PIXEL_PACK_BUFFER, 0, bytes, 
#ifdef GL_MAP_READ_BIT_EXT
                    GL_MAP_READ_BIT_EXT
#else
                    0
#endif
                );
                if (ptr) {
                    glPixelStorei(GL_PACK_ALIGNMENT, prevPack);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glDeleteFramebuffers(1, &fbo);
                    return { static_cast<const T*>(ptr), std::size_t(w)*std::size_t(h),
                             GLPboUnmap{ pbo, PBOKind::Pack, MapBackend::EXT, nullptr } };
                }
            }
#endif
#if defined(GLAD_GL_OES_mapbuffer)
            if (GLAD_GL_OES_mapbuffer) {
                void* ptr = glMapBufferOES(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY_OES);
                if (ptr) {
                    glPixelStorei(GL_PACK_ALIGNMENT, prevPack);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glDeleteFramebuffers(1, &fbo);
                    return { static_cast<const T*>(ptr), std::size_t(w)*std::size_t(h),
                             GLPboUnmap{ pbo, PBOKind::Pack, MapBackend::OES, nullptr } };
                }
            }
#endif
            // Couldn’t map PBO; fall back to CPU
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            glDeleteBuffers(1, &pbo);
        }
#endif // GL_EXT_pixel_buffer_object

        // 4) CPU fallback
        void* staging = std::malloc(bytes);
        if (!staging) {
            glPixelStorei(GL_PACK_ALIGNMENT, prevPack);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &fbo);
            throw std::bad_alloc();
        }
        glReadPixels(0, 0, w, h, format_, T_, staging);

        // restore
        glPixelStorei(GL_PACK_ALIGNMENT, prevPack);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);

        return { static_cast<const T*>(staging), std::size_t(w)*std::size_t(h),
                 GLPboUnmap{/*pbo*/0, PBOKind::None, MapBackend::NoneCPU, staging} };
    }

    // ----------- WRITE: ES2 uploads via Tex(Sub)Image; PBO only if EXT present -----------
    [[nodiscard]] MappedView<T, GLPboUpload> MapWrite(int lvl)
    {
        const GLsizei w = GLsizei(width(lvl));
        const GLsizei h = GLsizei(height(lvl));
        const GLsizeiptr bytes = GLsizeiptr(w) * GLsizeiptr(h) * GLsizeiptr(sizeof(T));

        // Prefer PBO+map if available
#if defined(GLAD_GL_EXT_pixel_buffer_object)
        if (GLAD_GL_EXT_pixel_buffer_object) {
            GLuint pbo = 0;
            glGenBuffers(1, &pbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, bytes, nullptr, GL_STREAM_DRAW);

#if defined(GLAD_GL_ES_VERSION_3_0)
            if (GLAD_GL_ES_VERSION_3_0) {
                void* ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, bytes,
                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
                if (ptr) {
                    return { static_cast<T*>(ptr), std::size_t(w)*std::size_t(h),
                             GLPboUpload{ tex_, GL_TEXTURE_2D, lvl, 0, 0, w, h, format_, T_, pbo, MapBackend::ES3Core, nullptr, 0 } };
                }
            }
#endif
#if defined(GLAD_GL_EXT_map_buffer_range)
            if (GLAD_GL_EXT_map_buffer_range) {
                GLbitfield bits =
#ifdef GL_MAP_WRITE_BIT_EXT
                    GL_MAP_WRITE_BIT_EXT
#else
                    0
#endif
#ifdef GL_MAP_INVALIDATE_BUFFER_BIT_EXT
                    | GL_MAP_INVALIDATE_BUFFER_BIT_EXT
#endif
                    ;
                void* ptr = glMapBufferRangeEXT(GL_PIXEL_UNPACK_BUFFER, 0, bytes, bits);
                if (ptr) {
                    return { static_cast<T*>(ptr), std::size_t(w)*std::size_t(h),
                             GLPboUpload{ tex_, GL_TEXTURE_2D, lvl, 0, 0, w, h, format_, T_, pbo, MapBackend::EXT, nullptr, 0 } };
                }
            }
#endif
#if defined(GLAD_GL_OES_mapbuffer)
            if (GLAD_GL_OES_mapbuffer) {
                void* ptr = glMapBufferOES(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY_OES);
                if (ptr) {
                    return { static_cast<T*>(ptr), std::size_t(w)*std::size_t(h),
                             GLPboUpload{ tex_, GL_TEXTURE_2D, lvl, 0, 0, w, h, format_, T_, pbo, MapBackend::OES, nullptr, 0 } };
                }
            }
#endif
            // couldn’t map; drop back to CPU
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glDeleteBuffers(1, &pbo);
        }
#endif // GL_EXT_pixel_buffer_object

        // CPU staging fallback: caller writes into CPU memory; releaser uploads on scope-exit
        void* staging = std::malloc(bytes);
        if (!staging) throw std::bad_alloc();
        return { static_cast<T*>(staging), std::size_t(w)*std::size_t(h),
                 GLPboUpload{ tex_, GL_TEXTURE_2D, lvl, 0, 0, w, h, format_, T_, /*pbo*/0, MapBackend::NoneCPU, staging, bytes } };
    }

    // Info
    std::size_t width(int lvl)  const { return widths_.at(lvl); }
    std::size_t height(int lvl) const { return heights_.at(lvl); }
    std::size_t levels() const  { return widths_.size(); }
    std::size_t type_size() const { return sizeof(T); }
    T nodata() const { return nodata_; }

    void generate_mipmaps(int base_lvl)
    {
        // ES2 has glGenerateMipmap, but not BASE_LEVEL/MAX_LEVEL parameters in core.
        glBindTexture(GL_TEXTURE_2D, tex_);
        (void)base_lvl; // ignored on ES2
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

protected:
    [[nodiscard]] GLuint id() const noexcept {
        assert(tex_ != 0 && "Texture not created");
        return tex_;
    }

private:
    void compute_dims_(int w, int h)
    {
        widths_.clear(); heights_.clear();
        std::size_t W = std::size_t(w), H = std::size_t(h);
        while (true) {
            widths_.push_back(W); heights_.push_back(H);
            if (W == 1 && H == 1) break;
            W = std::max<std::size_t>(1, W >> 1);
            H = std::max<std::size_t>(1, H >> 1);
        }
    }

    void init_(int w, int h, T nodata_val, const T* base)
    {
        nodata_ = nodata_val;

        // Resolve formats the same way you did, then massage for ES2
        const int channels = getChannels<T>();
        GLint  internal = GetGLInternalFormat(GetTypeIndex<T>());
        GLenum format   = GetGLFormat(channels);
        GLenum Ttype    = GetGLType(GetTypeIndex<T>());

        // ES2 quirk: internal format typically must equal 'format' and be unsized.
        // If your converter produced sized formats (e.g., GL_R8), fix them.
        // If you rely on RED/RG, make sure GL_EXT_texture_rg is present and use *_EXT enums.
#if !defined(GLAD_GL_ES_VERSION_3_0)
        // Force unsized internal = format for ES2 core
#  if defined(GL_RED_EXT) && defined(GL_RG_EXT)
        // If format is GL_RED/GL_RG (from EXT), internal should match the EXT symbol.
#  endif
        internal = format;
#endif

        // If your type/format combo isn’t color-renderable on ES2, MapRead will throw.
        // (float RGBA requires extensions like EXT_color_buffer_float)

        compute_dims_(w, h);

        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D, tex_);

        // Basic, ES2-friendly parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // NPOT-friendly
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, base ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Allocate level 0
        glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, Ttype, base);

        // Allocate remaining mips (optional but keeps driver happy for mip filtering)
        int W = std::max(1, w >> 1), H = std::max(1, h >> 1);
        for (int lvl = 1; lvl < int(levels()); ++lvl, W = std::max(1, W >> 1), H = std::max(1, H >> 1)) {
            glTexImage2D(GL_TEXTURE_2D, lvl, internal, W, H, 0, format, Ttype, nullptr);
        }
        if (base) glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);

        // Store chosen formats
        internal_ = internal;
        format_   = format;
        T_        = Ttype;
    }

    GLuint tex_ = 0;
    std::vector<std::size_t> widths_, heights_;
    GLint  internal_{};
    GLenum format_{};
    GLenum T_{};
    T      nodata_{};
};
