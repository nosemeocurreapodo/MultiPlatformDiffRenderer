#pragma once

#include "backends/cpu/devicecpu.h"

template <typename Type>
class BufferCPU
{

public:
    BufferCPU()
    {
        size_ = 0;
        data_ = nullptr;
    }

    BufferCPU(unsigned int size)
    {
        size_ = size;
        data_ = std::make_unique<Type[]>(size);
    }

    BufferCPU(Type *data, unsigned int size)
    {
        size_ = size;
        data_ = std::make_unique<Type[]>(size);
        std::copy(data, data + size_, data_);
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

    void FromCPU(const Type *data)
    {
        std::copy(data, data + size_, data_.get());
    }

    void ToCPU(Type *data) const
    {
        std::copy(data_.get(), data_.get() + size_, data);
    }

    Type &operator[](unsigned int index)
    {
        return data_[index];
    }

    const Type &operator[](unsigned int index) const
    {
        return data_[index];
    }
    /*
    const Type *get() const
    {
        return data_.get();
    }

    Type *get()
    {
        return data_.get();
    }
    */
    unsigned int size() const
    {
        return size_;
    }

    /*
unsigned int size() const override
{
    return size_;
}
*/
    void fill(const Type &value)
    {
        std::fill_n(data_.get(), size_, value);
    }

private:
    std::unique_ptr<Type[]> data_;
    unsigned int size_;
};
