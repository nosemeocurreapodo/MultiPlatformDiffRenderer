#pragma once

// #include "core/types.h"

template <typename T, int max_size>
class BufferBRAM
{
public:
    BufferBRAM() = default;

    BufferBRAM(unsigned int n)
    {
#ifndef USE_VITIS
        assert(n < max_size);
#endif
        size_ = n;
    }

    BufferBRAM(unsigned int n, const T *src) : BufferBRAM(n)
    {
    bufferhls_copy_data_loop:
        for (int i = 0; i < n; i++)
        {
            data_[i] = src[i];
        }
    }

    ~BufferBRAM() = default;

    // -------- capacity / info --------
    unsigned int size() const noexcept { return size_; }

protected:
    template <class T2>
    friend class TextureBRAM;
    template <class T2, class Derived>
    friend class RendererBase;

    T &operator[](unsigned int i) noexcept
    {
#ifndef USE_VITIS
        assert(i < size_);
#endif
        return data_[i];
    }

    const T &operator[](unsigned int i) const noexcept
    {
#ifndef USE_VITIS
        assert(i < size_);
#endif
        return data_[i];
    }

    T data_[max_size];
    unsigned int size_;
};

template <typename T>
class BufferRAM
{
public:
    BufferRAM() = default;

    BufferRAM(unsigned int n, T *src)
    {
        data_ = src;
        size_ = n;
    }

    BufferRAM(unsigned int n, const T *src)
    {
        data_ = src;
        size_ = n;
    }

    ~BufferRAM() = default;

    // -------- capacity / info --------
    unsigned int size() const noexcept { return size_; }

protected:
    template <class T2>
    friend class TextureRAM;
    template <class T2, class Derived>
    friend class RendererBase;

    T &operator[](unsigned int i) noexcept
    {
#ifndef USE_VITIS
        assert(i < size_);
#endif
        return data_[i];
    }

    const T &operator[](unsigned int i) const noexcept
    {
#ifndef USE_VITIS
        assert(i < size_);
#endif
        return data_[i];
    }

    T *data_;
    unsigned int size_;
};
