#pragma once

template <typename Type>
class Buffer
{
public:
    // virtual Buffer(unsigned int size) = 0;
    // virtual Buffer(unsigned int size, const Type *data) = 0;
    // virtual Buffer(const std::vector<Type> &data) = 0;
    // virtual Buffer(const BufferGL &other) = 0;
    // virtual Buffer &operator=(const Buffer &other) = 0;
    virtual void FromCPU(const Type *data) = 0;
    virtual void ToCPU(Type *data) const = 0;
    unsigned int size()
    {
        return size_;
    }
    // virtual void fill(const Type &value) = 0;
private:
    unsigned int size_;
};
