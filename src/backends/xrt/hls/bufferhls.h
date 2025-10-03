#pragma once

#include "core/types.h"

template <typename T>
class BufferHLS
{
public:
    // using value_type = T;
    // using size_type = std::size_t;

    BufferHLS() = default;

    /*
    BufferHLS(std::size_t n, const T *src)
    {
        size_ = n;
        data_ = src;
    }
    */

    BufferHLS(UInt n, T *src) : size_(n), data_(src)
    {
    }

    ~BufferHLS() = default;

    // -------- capacity / info --------
    UInt size() const noexcept { return size_; }

protected:
    template <class T2>
    friend class TextureHLS;
    template <class Derived>
    friend class RendererBase;

    T *data() noexcept { return data_; }
    const T *data() const noexcept { return data_; }

    // -------- element / raw access --------
    // T operator[](UInt i) const noexcept
    //{
    // #ifndef USE_VITIS
    //         assert(i < size_);
    // #endif
    //    return data_[i];
    //}

    /*
    T get(UInt i) const
    {
        return data_[i];
    }

    void set(T data, UInt i)
    {
        data_[i] = data;
    }
    */

    T *data_;
    UInt size_ = 0;
};
