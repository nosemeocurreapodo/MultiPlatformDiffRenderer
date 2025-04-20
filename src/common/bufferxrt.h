#pragma once

#include "common/devicexrt.h"

template <typename Type, int buffer_type = GL_ARRAY_BUFFER, int usage = GL_STATIC_DRAW>
class BufferXRT
{
    template <typename InTexType, typename OutTexType>
    friend class BaseRendererXRT;
    friend class DepthRendererXRT;
    friend class ImageRendererXRT;
    friend class MeshXRT;

public:
BufferXRT()
    {
        size_ = 0;
        glGenBuffers(1, &buffer_);
    }

    BufferXRT(int size)
    {
        size_ = size;
        buffer_ = xrt::bo(device, size_*sizeof(Type), krnl.group_id(0));
    }

    BufferXRT(const std::vector<Type> &data)
    {
        size_ = data.size();
        buffer_ = xrt::bo(device, data.size()*sizeof(Type), krnl.group_id(0));
        FromCPU(data.data());
    }

    BufferXRT(const BufferXRT &other)
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

    BufferXRT &operator=(const BufferXRT &other)
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

    void FromCPU(const Type *data)
    {
        Type* buffer_map = buffer_.map<Type*>();
        std::memcmp(data, buffer_map, size_)
        buffer_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }

    void ToCPU(Type *data)
    {
        buffer_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        Type* buffer_map = buffer_.map<Type*>();
        std::memcmp(buffer_map, data, size_)
    }

protected:
    xrt::bo buffer_;
    unsigned int size_;
};
