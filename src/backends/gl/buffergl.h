#pragma once
#include "backends/gl/devicegl_glad.h"
#include <cstddef>
#include <utility>
#include <vector>
#include <stdexcept>

// Releaser for mapped GL buffers (binds, unmaps on destruction)
struct GLUnmap
{
    GLuint id{};
    GLenum target{};
    void operator()() const noexcept
    {
        if (!id)
            return;
        glBindBuffer(target, id);
        glUnmapBuffer(target);
    }
};

// MappedView<T, Releaser> same as in your CPU version:
// template<class T, class Releaser = NoopReleaser> class MappedView { ... };

template <typename T, GLenum Target = GL_ARRAY_BUFFER, GLenum Usage = GL_STATIC_DRAW>
class BufferGL
{
public:
    // using value_type = T;

    BufferGL() = default;

    explicit BufferGL(std::size_t n) : size_(n)
    {
        glGenBuffers(1, &id_);
        glBindBuffer(Target, id_);
        glBufferData(Target, bytes(), nullptr, Usage);
    }

    BufferGL(std::size_t n, const T *src) : BufferGL(n)
    {
        if (n)
        {
            glBufferSubData(Target, 0, bytes(), src);
        }
    }

    explicit BufferGL(const std::vector<T> &v) : BufferGL(v.size(), v.data()) {}

    ~BufferGL()
    {
        if (id_)
            glDeleteBuffers(1, &id_);
    }

    // --- Move only ---
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

    /*
    // Explicit clone if you *really* need a copy
    void clone_from(const BufferGL& other) {
        if (this == &other) return;
        if (!id_) glGenBuffers(1, &id_);
        size_ = other.size_;
        glBindBuffer(Target, id_);
        glBufferData(Target, bytes(), nullptr, Usage);
        glBindBuffer(GL_COPY_READ_BUFFER,  other.id_);
        glBindBuffer(GL_COPY_WRITE_BUFFER, id_);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, bytes());
    }
    */

    // ---- Cross-backend style API ----
    [[nodiscard]] MappedView<const T, GLUnmap> MapRead() const &
    {
        if (!id_)
            throw std::runtime_error("BufferGL::MapRead on empty buffer");
        glBindBuffer(Target, id_);
        void *p = glMapBufferRange(Target, 0, bytes(), GL_MAP_READ_BIT);
        if (!p)
            throw std::runtime_error("glMapBufferRange(read) failed");
        return MappedView<const T, GLUnmap>(static_cast<const T *>(p), size_, GLUnmap{id_, Target});
    }

    [[nodiscard]] MappedView<T, GLUnmap> MapWrite() &
    {
        if (!id_)
            throw std::runtime_error("BufferGL::MapWrite on empty buffer");
        glBindBuffer(Target, id_);
        // If you need read-modify-write, add GL_MAP_READ_BIT.
        void *p = glMapBufferRange(Target, 0, bytes(),
                                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (!p)
            throw std::runtime_error("glMapBufferRange(write) failed");
        return MappedView<T, GLUnmap>(static_cast<T *>(p), size_, GLUnmap{id_, Target});
    }

    std::size_t size() const noexcept { return size_; }

private:
    friend class MeshGL;

    GLuint id() const noexcept { return id_; }

    GLsizeiptr bytes() const noexcept
    {
        return static_cast<GLsizeiptr>(size_) * static_cast<GLsizeiptr>(sizeof(T));
    }

    GLuint id_ = 0;
    std::size_t size_ = 0;
};
