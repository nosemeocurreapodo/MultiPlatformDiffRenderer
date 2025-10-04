#pragma once

#include "core/types.h"

template <typename T, int max_size>
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

    BufferHLS(UInt n, T *src)
    {
#ifndef USE_VITIS
        assert(n < max_size);
#endif

    bufferhls_copy_data_loop:
        for (int i = 0; i < n; i++)
        {
            data_[i] = src[i];
        }
        size_ = n;
    }

    ~BufferHLS() = default;

    // -------- capacity / info --------
    UInt size() const noexcept { return size_; }

protected:
    template <class T2>
    friend class TextureHLS;
    template <class Derived>
    friend class RendererBase;

    // T *data() noexcept { return data_; }
    // const T *data() const noexcept { return data_; }

    // -------- element / raw access --------
    T &operator[](UInt i) noexcept
    {
#ifndef USE_VITIS
        assert(i < size_);
#endif
        return data_[i];
    }

    const T &operator[](UInt i) const noexcept
    {
#ifndef USE_VITIS
        assert(i < size_);
#endif
        return data_[i];
    }

    T data_[max_size];
    UInt size_;
};
