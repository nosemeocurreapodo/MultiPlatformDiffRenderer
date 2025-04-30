#pragma once

#include "xrt/devicexrt.h"

template <typename Type>
class BufferXRT
{

public:
    /*
    BufferXRT()
        {
            size_ = 0;
        }
            */

    BufferXRT(int size, const Type *data)
    {
        size_ = size;
        buffer_ = xrt::bo(xrt_device, size_ * sizeof(Type), xrt_kernel.group_id(0));
        FromCPU(data);
    }

    BufferXRT(const std::vector<Type> &data)
    {
        size_ = data.size();
        buffer_ = xrt::bo(xrt_device, size_ * sizeof(Type), xrt_kernel.group_id(0));
        FromCPU(data.data());
    }

    BufferXRT(const BufferXRT &other)
    {
        size_ = other.size_;
        buffer_ = xrt::bo(xrt_device, size_ * sizeof(Type), xrt_kernel.group_id(0));
        FromCPU(other.buffer_.map<Type *>());
    }

    BufferXRT &operator=(const BufferXRT &other)
    {
        if (this != &other)
        {
            size_ = other.size_;
            buffer_ = xrt::bo(xrt_device, size_ * sizeof(Type), xrt_kernel.group_id(0));
            FromCPU(other.buffer_.map<Type *>());
        }
        return *this;
    }

    int size() const
    {
        return size_;
    }

    void FromCPU(const Type *data)
    {
        Type *buffer_map = buffer_.map<Type *>();
        std::memcmp(data, buffer_map, size_);
        buffer_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }

    void ToCPU(Type *data)
    {
        buffer_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        Type *buffer_map = buffer_.map<Type *>();
        std::memcmp(buffer_map, data, size_)
    }

private:
    xrt::bo buffer_;
    unsigned int size_;
};
