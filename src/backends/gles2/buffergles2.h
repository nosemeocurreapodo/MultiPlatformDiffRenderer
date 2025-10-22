#pragma once
#include "backends/gl/devicegl_glad.h"
#include <stdexcept>
#include <cstdlib> // malloc/free
#include <cstring> // memcpy (if needed)

// --------- helpers to deal with ES2/ES3/EXT/OES differences ---------

// Translate desired access to whatever the platform supports.
enum class GLMapMode
{
    ReadOnly,
    WriteOnly
};

// Wrapper that unmaps OR uploads staging, depending on how we "mapped".
struct GLUnmap
{
    GLuint id{};
    GLenum target{};
    enum Kind
    {
        CoreOrES3,
        OES,
        RangeEXT,
        UploadStaging
    } kind{CoreOrES3};

    // For UploadStaging:
    void *staging{nullptr};
    GLsizeiptr nbytes{0};

    void operator()() const noexcept
    {
        if (!id)
        {
            if (kind == UploadStaging && staging)
                std::free(staging);
            return;
        }
        glBindBuffer(target, id);
        switch (kind)
        {
        case CoreOrES3:
            glUnmapBuffer(target);
            break;
        case OES:
#if defined(GLAD_GL_OES_mapbuffer)
            if (GLAD_GL_OES_mapbuffer)
                glUnmapBufferOES(target);
#endif
            break;
        case RangeEXT:
            // EXT uses the same Unmap function as core (if present) or OES.
#if defined(GLAD_GL_ES_VERSION_3_0)
            if (GLAD_GL_ES_VERSION_3_0)
            {
                glUnmapBuffer(target);
                break;
            }
#endif
#if defined(GLAD_GL_OES_mapbuffer)
            if (GLAD_GL_OES_mapbuffer)
            {
                glUnmapBufferOES(target);
                break;
            }
#endif
            break;
        case UploadStaging:
            if (staging && nbytes > 0)
            {
                glBufferSubData(target, 0, nbytes, staging);
                std::free(staging);
            }
            break;
        }
    }
};

// Try to map with the best available API. Returns (ptr, unmap-kind).
inline std::pair<void *, GLUnmap::Kind>
GLTryMap(GLenum target, GLsizeiptr size, GLMapMode mode, bool invalidateWholeBuffer)
{
#if defined(GLAD_GL_ES_VERSION_3_0)
    if (GLAD_GL_ES_VERSION_3_0)
    {
        GLbitfield bits = 0;
        if (mode == GLMapMode::ReadOnly)
            bits |= GL_MAP_READ_BIT;
        if (mode == GLMapMode::WriteOnly)
            bits |= GL_MAP_WRITE_BIT;
        if (invalidateWholeBuffer && mode == GLMapMode::WriteOnly)
            bits |= GL_MAP_INVALIDATE_BUFFER_BIT;
        void *p = glMapBufferRange(target, 0, size, bits);
        if (p)
            return {p, GLUnmap::CoreOrES3};
    }
#endif
#if defined(GLAD_GL_EXT_map_buffer_range)
    if (GLAD_GL_EXT_map_buffer_range)
    {
        GLbitfield bits = 0;
#ifdef GL_MAP_READ_BIT_EXT
        if (mode == GLMapMode::ReadOnly)
            bits |= GL_MAP_READ_BIT_EXT;
#endif
#ifdef GL_MAP_WRITE_BIT_EXT
        if (mode == GLMapMode::WriteOnly)
            bits |= GL_MAP_WRITE_BIT_EXT;
#endif
#ifdef GL_MAP_INVALIDATE_BUFFER_BIT_EXT
        if (invalidateWholeBuffer && mode == GLMapMode::WriteOnly)
            bits |= GL_MAP_INVALIDATE_BUFFER_BIT_EXT;
#endif
        // Some drivers prefer orphaning before mapping for write-discard.
        if ((bits & (
#ifdef GL_MAP_INVALIDATE_BUFFER_BIT_EXT
                        GL_MAP_INVALIDATE_BUFFER_BIT_EXT
#else
                        0
#endif
                        )) != 0)
        {
            glBufferData(target, size, nullptr,
                         /* Usage is set when you created the buffer */ GL_STATIC_DRAW);
        }
        void *p = glMapBufferRangeEXT(target, 0, size, bits);
        if (p)
            return {p, GLUnmap::RangeEXT};
    }
#endif
#if defined(GLAD_GL_OES_mapbuffer)
    if (GLAD_GL_OES_mapbuffer)
    {
        GLenum access =
            (mode == GLMapMode::ReadOnly) ? GL_READ_ONLY_OES : (mode == GLMapMode::WriteOnly) ? GL_WRITE_ONLY_OES
                                                                                              : GL_WRITE_ONLY_OES;
        // Write-discard: orphan first
        if (invalidateWholeBuffer && mode == GLMapMode::WriteOnly)
            glBufferData(target, size, nullptr, GL_STATIC_DRAW);
        void *p = glMapBufferOES(target, access);
        if (p)
            return {p, GLUnmap::OES};
    }
#endif
    return {nullptr, GLUnmap::UploadStaging}; // signal "no mapping"
}

// ---------- your class with minimal changes to MapRead/MapWrite ----------

template <typename T, GLenum Target = GL_ARRAY_BUFFER, GLenum Usage = GL_STATIC_DRAW>
class BufferGL
{
public:
    BufferGL() = default;

    explicit BufferGL(std::size_t n) : size_(n)
    {
        glGenBuffers(1, &id_);
        glBindBuffer(Target, id_);
        glBufferData(Target, n ? GLsizeiptr(n * sizeof(T)) : 0, nullptr, Usage);
    }

    BufferGL(std::size_t n, const T *src) : BufferGL(n)
    {
        if (n)
            glBufferSubData(Target, 0, GLsizeiptr(n * sizeof(T)), src);
    }

    explicit BufferGL(const std::vector<T> &v) : BufferGL(v.size(), v.data()) {}

    ~BufferGL()
    {
        if (id_)
            glDeleteBuffers(1, &id_);
    }

    BufferGL(BufferGL &&o) noexcept : id_(std::exchange(o.id_, 0)),
                                      size_(std::exchange(o.size_, 0)) {}
    BufferGL &operator=(BufferGL &&o) noexcept
    {
        if (this != &o)
        {
            if (id_)
                glDeleteBuffers(1, &id_);
            id_ = std::exchange(o.id_, 0);
            size_ = std::exchange(o.size_, 0);
        }
        return *this;
    }

    BufferGL(const BufferGL &) = delete;
    BufferGL &operator=(const BufferGL &) = delete;

    // READ: requires real mapping (ES3 or OES/EXT). Otherwise throws.
    [[nodiscard]] MappedView<const T, GLUnmap> MapRead() const &
    {
        if (!id_)
            throw std::runtime_error("BufferGL::MapRead on empty buffer");
        glBindBuffer(Target, id_);
        const GLsizeiptr nbytes = GLsizeiptr(size_ * sizeof(T));

        auto [ptr, kind] = GLTryMap(Target, nbytes, GLMapMode::ReadOnly, /*invalidate*/ false);
        if (!ptr)
            throw std::runtime_error("MapRead not supported on this GLES2 context (no mapping extensions).");

        return MappedView<const T, GLUnmap>(
            static_cast<const T *>(ptr),
            size_,
            GLUnmap{id_, Target, kind, /*staging*/ nullptr, /*nbytes*/ 0});
    }

    // WRITE: maps if possible; otherwise stages and uploads on release.
    [[nodiscard]] MappedView<T, GLUnmap> MapWrite() &
    {
        if (!id_)
            throw std::runtime_error("BufferGL::MapWrite on empty buffer");
        glBindBuffer(Target, id_);
        const GLsizeiptr nbytes = GLsizeiptr(size_ * sizeof(T));

        auto [ptr, kind] = GLTryMap(Target, nbytes, GLMapMode::WriteOnly, /*invalidate*/ true);
        if (ptr)
        {
            return MappedView<T, GLUnmap>(
                static_cast<T *>(ptr),
                size_,
                GLUnmap{id_, Target, kind, /*staging*/ nullptr, /*nbytes*/ 0});
        }

        // No mapping available: allocate CPU memory, upload on release.
        void *staging = std::malloc(size_ * sizeof(T));
        if (!staging)
            throw std::bad_alloc();
        return MappedView<T, GLUnmap>(
            static_cast<T *>(staging),
            size_,
            GLUnmap{id_, Target, GLUnmap::UploadStaging, staging, nbytes});
    }

    std::size_t size() const noexcept { return size_; }
    GLuint id() const noexcept { return id_; }

    GLuint id_ = 0;
    std::size_t size_ = 0;
};
