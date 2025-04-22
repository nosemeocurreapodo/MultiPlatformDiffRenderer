#pragma once

template <typename Type>
class BufferFPGA
{

public:
    /*
    BufferXRT()
        {
            size_ = 0;
        }
            */

    BufferFPGA(int size, const Type *data)
    {
        size_ = size;
        buffer_ = data;
    }

    BufferFPGA(const BufferFPGA &other)
    {
        size_ = other.size_;
        buffer_ = other.buffer_;
    }

    BufferXRT &operator=(const BufferXRT &other)
    {
        if (this != &other)
        {
            size_ = other.size_;
            buffer_ = other.buffer_;
        }
        return *this;
    }

    Type &operator[](int index)
    {
        return buffer_[index];
    }

    const Type &operator[](int index) const
    {
        return buffer_[index];
    }

    int size() const
    {
        return size_;
    }

private:
    Type *buffer_;
    unsigned int size_;
};
