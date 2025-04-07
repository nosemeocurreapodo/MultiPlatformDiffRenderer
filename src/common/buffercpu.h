#pragma once

#include "common/deviceGL.h"

template <typename Type>
class BufferCPU
{
public:
    BufferCPU(unsigned int size)
    {
        data_ = std::make_unique<Type[]>(size);
        size_ = size;
    }

    BufferCPU(const BufferCPU &other)
    {
        size_ = other.size_;
        data_ = std::make_unique<Type[]>(size_);
        std::copy(other.data_.get(), other.data_.get() + size, data_.get());
    }

    BufferCPU &operator=(const BufferCPU &other)
    {
        if (this != &other)
        {
            size = other.size;
            data = std::make_unique<Type[]>(size);
            std::copy(other.data.get(), other.data.get() + size, data.get());
        }
        return *this;
    }

    void FromCPU(Type *data)
    {
        std::copy(data, data + channels_ * width_ * height_, data_.get());
    }

    void ToCPU(Type *data)
    {
        std::copy(data_.get(), data_.get() + channels_ * width_ * height_, data);
    }

private:
    std::unique_ptr<Type[]> data_;
    unsigned int size_;
};
