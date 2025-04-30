#pragma once

template <typename Type>
class BufferFPGA
{
template <typename  InTexType,
          typename  VaryingType,
          typename  OutTexType,
          typename  Derived>   
    friend class BaseRendererFPGA;
    
public:
    /*
    BufferFPGA()
        {
    buffer_ = nullptr;
            size_ = 0;
        }
            */

    BufferFPGA(int size, Type *data)
    {
        size_ = size;
        buffer_ = data;
    }

    BufferFPGA(const BufferFPGA &other)
    {
        size_ = other.size_;
        buffer_ = other.buffer_;
    }

    BufferFPGA &operator=(const BufferFPGA &other)
    {
        //if (this != &other)
        {
            size_ = other.size_;
            buffer_ = other.buffer_;
        }
        return *this;
    }

    int size() const
    {
        return size_;
    }

protected:
    Type &operator[](int index)
    {
        return buffer_[index];
    }

    const Type &operator[](int index) const
    {
        return buffer_[index];
    }

    Type *buffer_;
    unsigned int size_;
};
