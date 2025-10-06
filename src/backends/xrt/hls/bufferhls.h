#pragma once

#include "core/types.h"

template <typename T, int max_size>
class BufferBRAM
{
public:
    BufferBRAM() = default;

    BufferBRAM(UInt n)
    {
#ifndef USE_VITIS
        assert(n < max_size);
#endif
        size_ = n;
    }

    BufferBRAM(UInt n, const T *src)
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

    ~BufferBRAM() = default;

    // -------- capacity / info --------
    UInt size() const noexcept { return size_; }

protected:
    template <class T2>
    friend class TextureBRAM;
    template <class Derived>
    friend class RendererBase;

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

template <typename T>
class BufferRAM
{
public:
    BufferRAM() = default;

    BufferRAM(UInt n, T *src)
    {
        data_ = src;
        size_ = n;
    }

    ~BufferRAM() = default;

    // -------- capacity / info --------
    UInt size() const noexcept { return size_; }

protected:
    template <class T2>
    friend class TextureRAM;
    template <class Derived>
    friend class RendererBase;

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

    T *data_;
    UInt size_;
};
