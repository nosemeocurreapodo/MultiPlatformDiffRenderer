#pragma once
#include "backends/base/mappedviewbase.h"
#include "backends/gl/devicegl_glad.h"
// #include <cstddef>
// #include <utility>
#include <vector>
#include <stdexcept>

// Releaser for mapped GL buffers (binds, unmaps on destruction)
struct GLUnmap
{
    GLUnmap() : id(0), target(0) {};
    GLUnmap(GLuint _id, GLenum _target) : id(_id), target(_target) {}

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

template <typename T>
using BufferViewGL = BufferViewBase<T, GLUnmap>;

template <typename T, GLenum Target = GL_ARRAY_BUFFER, GLenum Usage = GL_STATIC_DRAW>
class BufferGL
{
public:
    BufferGL() = default;

    explicit BufferGL(std::size_t n) : size_(n)
    {
        glGenBuffers(1, &id_);
        glBindBuffer(Target, id_);
        glBufferData(Target, n * sizeof(T), nullptr, Usage);
    }

    BufferGL(std::size_t n, const T *src) : BufferGL(n)
    {
        if (n)
        {
            glBufferSubData(Target, 0, n * sizeof(T), src);
        }
    }

    explicit BufferGL(const std::vector<T> &v) : BufferGL(v.size(), v.data()) {}

    ~BufferGL()
    {
        if (id_)
            glDeleteBuffers(1, &id_);
    }

    // ---------- COPY CONSTRUCTOR ----------
    BufferGL(const BufferGL &other)
    {
        clone_from(other);
    }

    // ---------- COPY ASSIGNMENT ----------
    BufferGL &operator=(const BufferGL &other)
    {
        if (this != &other)
        {
            clone_from(other);
        }
        return *this;
    }

    // ---------- MOVE CONSTRUCTOR / ASSIGNMENT ----------
    BufferGL(BufferGL &&o) noexcept
        : id_(std::exchange(o.id_, 0)),
          size_(std::exchange(o.size_, 0))
    {
    }

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

    // ---- Cross-backend style API ----
    [[nodiscard]] BufferViewGL<const T> MapRead() const &
    {
        if (!id_)
            throw std::runtime_error("BufferGL::MapRead on empty buffer");
        glBindBuffer(Target, id_);
        void *p = glMapBufferRange(Target, 0, size_ * sizeof(T), GL_MAP_READ_BIT);
        if (!p)
            throw std::runtime_error("glMapBufferRange(read) failed");
        return BufferViewGL<const T>(static_cast<const T *>(p), size_, GLUnmap{id_, Target});
    }

    [[nodiscard]] BufferViewGL<T> MapWrite() &
    {
        if (!id_)
            throw std::runtime_error("BufferGL::MapWrite on empty buffer");
        glBindBuffer(Target, id_);
        void *p = glMapBufferRange(Target, 0, size_ * sizeof(T),
                                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (!p)
            throw std::runtime_error("glMapBufferRange(write) failed");
        return BufferViewGL<T>(static_cast<T *>(p), size_, GLUnmap{id_, Target});
    }

    std::size_t size() const noexcept { return size_; }
    GLuint id() const noexcept { return id_; }

    GLuint id_ = 0;
    std::size_t size_ = 0;

private:
    std::size_t bytes() const noexcept { return size_ * sizeof(T); }

    void clone_from(const BufferGL &other)
    {
        // Copy of an "empty" buffer: release ours if we have one
        if (!other.id_ || other.size_ == 0)
        {
            if (id_)
            {
                glDeleteBuffers(1, &id_);
                id_ = 0;
            }
            size_ = 0;
            return;
        }

        if (!id_)
            glGenBuffers(1, &id_);

        size_ = other.size_;

        // Allocate our storage
        glBindBuffer(Target, id_);
        glBufferData(Target, bytes(), nullptr, Usage);

        // GPU–GPU copy
        glBindBuffer(GL_COPY_READ_BUFFER, other.id_);
        glBindBuffer(GL_COPY_WRITE_BUFFER, id_);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, bytes());
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }
};
