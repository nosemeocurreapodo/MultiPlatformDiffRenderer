#pragma once

#include "common/devicecpu.h"

template <typename Type>
class BufferCPU
{
    template <typename InTexType, typename VaryingType, typename OutTexType>
    friend class BaseRendererCPU;
    friend class DepthRendererCPU;
    friend class ImageRendererCPU;
    friend class MeshCPU;

public:
    BufferCPU()
    {
        data_ = nullptr;
        size_ = 0;
    }
    BufferCPU(unsigned int size)
    {
        data_ = std::make_unique<Type[]>(size);
        size_ = size;
    }

    BufferCPU(const std::vector<Type> &data)
    {
        size_ = data.size();
        data_ = std::make_unique<Type[]>(size_);
        std::copy(data.data(), data.data() + size_, data_.get());
    }

    BufferCPU(const BufferCPU &other)
    {
        size_ = other.size_;
        data_ = std::make_unique<Type[]>(size_);
        std::copy(other.data_.get(), other.data_.get() + size_, data_.get());
    }

    BufferCPU &operator=(const BufferCPU &other)
    {
        if (this != &other)
        {
            size_ = other.size_;
            data_ = std::make_unique<Type[]>(size_);
            std::copy(other.data_.get(), other.data_.get() + size_, data_.get());
        }
        return *this;
    }

    void FromCPU(Type *data)
    {
        std::copy(data, data + size_, data_.get());
    }

    void ToCPU(Type *data)
    {
        std::copy(data_.get(), data_.get() + size_, data);
    }

private:
    Type &operator[](unsigned int index)
    {
        return data_[index];
    }

    const Type &operator[](unsigned int index) const
    {
        return data_[index];
    }

    std::unique_ptr<Type[]> data_;
    unsigned int size_;
};
