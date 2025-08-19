#pragma once

#include "backends/gl/devicegl_glad.h"

template <typename Type, int buffer_type = GL_ARRAY_BUFFER, int usage = GL_STATIC_DRAW>
class BufferGL : public Buffer<Type>
{

public:
/*
    BufferGL()
    {
        size_ = 0;
        buffer_ = 0;
    }
*/
    BufferGL(unsigned int size)
    {
        size_ = size;
        glGenBuffers(1, &buffer_);
        glBindBuffer(buffer_type, buffer_);
        glBufferData(buffer_type, size_ * sizeof(Type), nullptr, usage);
    }

    BufferGL(unsigned int size, const Type *data)
    {
        size_ = size;
        glGenBuffers(1, &buffer_);
        glBindBuffer(buffer_type, buffer_);
        glBufferData(buffer_type, size_ * sizeof(Type), data, usage);
    }

    BufferGL(const std::vector<Type> &data)
    {
        size_ = data.size();
        glGenBuffers(1, &buffer_);
        glBindBuffer(buffer_type, buffer_);
        glBufferData(buffer_type, size_ * sizeof(Type), data.data(), usage);
    }

    BufferGL(const BufferGL &other)
    {
        glGenBuffers(1, &buffer_);
        glBindBuffer(buffer_type, buffer_);
        glBufferData(buffer_type, other.size_ * sizeof(Type), nullptr, usage);
        // 1. Bind source as COPY_READ
        glBindBuffer(GL_COPY_READ_BUFFER, other.buffer_);

        // 2. Bind destination as COPY_WRITE
        glBindBuffer(GL_COPY_WRITE_BUFFER, buffer_);
        glCopyBufferSubData(
            GL_COPY_READ_BUFFER,       // read target
            GL_COPY_WRITE_BUFFER,      // write target
            0,                         // source offset in bytes
            0,                         // destination offset in bytes
            other.size_ * sizeof(Type) // number of bytes to copy
        );
        size_ = other.size_;
    }

    BufferGL &operator=(const BufferGL &other)
    {
        if (this != &other)
        {
            glDeleteBuffers(1, &buffer_);

            glGenBuffers(1, &buffer_);
            glBindBuffer(buffer_type, buffer_);
            glBufferData(buffer_type, other.size_ * sizeof(Type), nullptr, usage);
            // 1. Bind source as COPY_READ
            glBindBuffer(GL_COPY_READ_BUFFER, other.buffer_);

            // 2. Bind destination as COPY_WRITE
            glBindBuffer(GL_COPY_WRITE_BUFFER, buffer_);
            glCopyBufferSubData(
                GL_COPY_READ_BUFFER,       // read target
                GL_COPY_WRITE_BUFFER,      // write target
                0,                         // source offset in bytes
                0,                         // destination offset in bytes
                other.size_ * sizeof(Type) // number of bytes to copy
            );
            size_ = other.size_;
        }
        return *this;
    }

    void FromCPU(const Type *data) override
    {
        glBindBuffer(buffer_type, buffer_);
        glBufferSubData(buffer_type, 0, size_ * sizeof(Type), data);
    }

    void ToCPU(Type *data) const override
    {
        glBindBuffer(buffer_type, buffer_);
        glGetBufferSubData(buffer_type, 0, size_ * sizeof(Type), data);

        /*
        void* mappedPtr = glMapBufferRange(
            GL_ARRAY_BUFFER,
            0,               // offset in bytes
            dataSize,        // size in bytes to map
            GL_MAP_READ_BIT  // we only want to read from this buffer
        );

        if (mappedPtr) {
            // 2) Copy from mappedPtr to outPtr
            memcpy(outPtr, mappedPtr, dataSize);

            // 3) Unmap
            glUnmapBuffer(GL_ARRAY_BUFFER);
        }
        else {
            // Handle error: mapping failed
        }
        */
    }

protected:
    GLuint buffer_;
    unsigned int size_;
};
