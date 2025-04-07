#pragma once

#include "common/devicegl.h"

template <typename Type>
class BufferGL
{
    friend class MeshGL;

public:
    BufferGL(int size)
    {
        size_ = size;
        glGenBuffers(1, &vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, size_ * sizeof(Type), nullptr, GL_STREAM_DRAW);
    }

    BufferGL(const BufferGL &other)
    {
        glGenBuffers(1, &vbo_);
        // 1. Bind source as COPY_READ
        glBindBuffer(GL_COPY_READ_BUFFER, other.vbo_);

        // 2. Bind destination as COPY_WRITE
        glBindBuffer(GL_COPY_WRITE_BUFFER, vbo_);
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
            glGenBuffers(1, &vbo_);
            // 1. Bind source as COPY_READ
            glBindBuffer(GL_COPY_READ_BUFFER, other.vbo_);

            // 2. Bind destination as COPY_WRITE
            glBindBuffer(GL_COPY_WRITE_BUFFER, vbo_);
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

    void FromCPU(Type *data)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, size_ * sizeof(Type), data, GL_STREAM_DRAW);
    }

    void ToCPU(Type *data)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glGetBufferSubData(GL_ARRAY_BUFFER, 0, size_ * sizeof(Type), data);

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
    GLuint vbo_;
    unsigned int size_;
};
