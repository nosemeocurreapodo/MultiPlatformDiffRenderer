#pragma once

#include "backends/base/MappedView.h"

template <typename T>
class BufferHLS
{
public:
    // using value_type = T;
    // using size_type = std::size_t;

    BufferHLS() = default;

    BufferHLS(std::size_t n, const T *src)
    {
        size_ = n;
        data_ = src;
    }

    BufferHLS(std::size_t n, T *src)
    {
        size_ = n;
        data_ = src;
    }

    ~BufferHLS() = default;

    // -------- capacity / info --------
    std::size_t size() const noexcept { return size_; }

protected:
    template <class T2>
    friend class TextureHLS;
    template <class Derived>
    friend class RendererBase;

    T *data() noexcept { return data_; }
    const T *data() const noexcept { return data_; }

    // -------- element / raw access --------
    T &operator[](std::size_t i) noexcept
    {
        assert(i < size_);
        return data_[i];
    }
    const T &operator[](std::size_t i) const noexcept
    {
        assert(i < size_);
        return data_[i];
    }

    T *data_;
    std::size_t size_ = 0;
};
