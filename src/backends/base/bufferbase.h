#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "backends/base/MappedView.h" // your MappedView + NoopReleaser

template <typename T>
class BufferBase
{
public:
    // using value_type = T;
    // using size_type = std::size_t;

    BufferBase() = default;

    explicit BufferBase(std::size_t n)
        : size_(n), data_(n ? std::make_unique<T[]>(n) : nullptr) {}

    BufferBase(std::size_t n, const T *src) : BufferBase(n)
    {
        if (size_)
            std::copy_n(src, size_, data_.get());
    }

    explicit BufferBase(const std::vector<T> &v) : BufferBase(v.size(), v.data()) {}

    // Copy (deep) via copy-and-swap
    BufferBase(const BufferCPU &other) : BufferBase(other.size_)
    {
        if (size_)
            std::copy_n(other.data_.get(), size_, data_.get());
    }
    BufferBase &operator=(const BufferBase &other)
    {
        if (this != &other)
        {
            BufferCPU tmp(other);
            swap(tmp);
        }
        return *this;
    }

    // Move (nothrow)
    BufferBase(BufferBase &&) noexcept = default;
    BufferBase &operator=(BufferBase &&) noexcept = default;

    ~BufferBase() = default;

    // -------- capacity / info --------
    std::size_t size() const noexcept { return size_; }
    // bool empty() const noexcept { return size_ == 0; }

    // Discard old contents, allocate new size (does not preserve data)
    // void resize_and_discard(size_type n)
    //{
    //    if (n == size_)
    //        return;
    //    data_.reset(n ? new T[n] : nullptr);
    //    size_ = n;
    //}

    // Assign from contiguous memory
    // void assign(const T *src, size_type n)
    //{
    //    resize_and_discard(n);
    //    if (n)
    //        std::copy_n(src, n, data_.get());
    //}
    // void assign(const std::vector<T> &v) { assign(v.data(), v.size()); }

    // void fill(const T &v)
    //{
    //     if (size_)
    //         std::fill_n(data_.get(), size_, v);
    // }

    // -------- cross-backend style API --------
    // On CPU, Map* returns a view with a no-op releaser.
    [[nodiscard]] MappedView<const T, NoopReleaser> MapRead() const & noexcept
    {
        return MappedView<const T, NoopReleaser>(data_.get(), size_);
    }
    [[nodiscard]] MappedView<T, NoopReleaser> MapWrite() & noexcept
    {
        return MappedView<T, NoopReleaser>(data_.get(), size_);
    }
    // forbid mapping temporaries (view would dangle)
    MappedView<const T> MapRead() const && = delete;
    MappedView<T> MapWrite() && = delete;

private:
    template <class T2>
    friend class TextureCPU;

    void swap(BufferCPU &o) noexcept
    {
        std::swap(data_, o.data_);
        std::swap(size_, o.size_);
    }

    T *data() noexcept { return data_.get(); }
    const T *data() const noexcept { return data_.get(); }

    // T *begin() noexcept { return data_.get(); }
    // T *end() noexcept { return data_.get() + size_; }
    // const T *begin() const noexcept { return data_.get(); }
    // const T *end() const noexcept { return data_.get() + size_; }
    // const T *cbegin() const noexcept { return data_.get(); }
    // const T *cend() const noexcept { return data_.get() + size_; }

    // -------- element / raw access --------
    T &operator[](std::size_t i) noexcept
    {
        assert(i < size_);
        return data_.get()[i];
    }
    const T &operator[](std::size_t i) const noexcept
    {
        assert(i < size_);
        return data_.get()[i];
    }

    std::unique_ptr<T[]> data_;
    std::size_t size_ = 0;
};
