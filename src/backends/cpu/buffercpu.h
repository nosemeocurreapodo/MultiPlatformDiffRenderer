#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "backends/base/MappedView.h" // your MappedView + NoopReleaser

template <typename T>
class BufferCPU
{
public:
    using value_type = T;
    using size_type = std::size_t;

    BufferCPU() = default;

    explicit BufferCPU(size_type n)
        : size_(n), data_(n ? std::make_unique<T[]>(n) : nullptr) {}

    BufferCPU(size_type n, const T *src) : BufferCPU(n)
    {
        if (size_)
            std::copy_n(src, size_, data_.get());
    }

    explicit BufferCPU(const std::vector<T> &v) : BufferCPU(v.size(), v.data()) {}

    // Copy (deep) via copy-and-swap
    BufferCPU(const BufferCPU &other) : BufferCPU(other.size_)
    {
        if (size_)
            std::copy_n(other.data_.get(), size_, data_.get());
    }
    BufferCPU &operator=(const BufferCPU &other)
    {
        if (this != &other)
        {
            BufferCPU tmp(other);
            swap(tmp);
        }
        return *this;
    }

    // Move (nothrow)
    BufferCPU(BufferCPU &&) noexcept = default;
    BufferCPU &operator=(BufferCPU &&) noexcept = default;

    ~BufferCPU() = default;

    // -------- capacity / info --------
    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    // Discard old contents, allocate new size (does not preserve data)
    void resize_and_discard(size_type n)
    {
        if (n == size_)
            return;
        data_.reset(n ? new T[n] : nullptr);
        size_ = n;
    }

    // Assign from contiguous memory
    void assign(const T *src, size_type n)
    {
        resize_and_discard(n);
        if (n)
            std::copy_n(src, n, data_.get());
    }
    void assign(const std::vector<T> &v) { assign(v.data(), v.size()); }

    // -------- element / raw access --------
    T &operator[](size_type i) noexcept
    {
        assert(i < size_);
        return data_.get()[i];
    }
    const T &operator[](size_type i) const noexcept
    {
        assert(i < size_);
        return data_.get()[i];
    }

    T *data() noexcept { return data_.get(); }
    const T *data() const noexcept { return data_.get(); }

    T *begin() noexcept { return data_.get(); }
    T *end() noexcept { return data_.get() + size_; }
    const T *begin() const noexcept { return data_.get(); }
    const T *end() const noexcept { return data_.get() + size_; }
    const T *cbegin() const noexcept { return data_.get(); }
    const T *cend() const noexcept { return data_.get() + size_; }

    void fill(const T &v)
    {
        if (size_)
            std::fill_n(data_.get(), size_, v);
    }

    // -------- cross-backend style API --------
    // On CPU, Map* returns a view with a no-op releaser.
    [[nodiscard]] MappedView<const T> MapRead() const & noexcept
    {
        return MappedView<const T>(data_.get(), size_);
    }
    [[nodiscard]] MappedView<T> MapWrite() & noexcept
    {
        return MappedView<T>(data_.get(), size_);
    }
    // forbid mapping temporaries (view would dangle)
    MappedView<const T> MapRead() const && = delete;
    MappedView<T> MapWrite() && = delete;

    // -------- utilities --------
    void swap(BufferCPU &o) noexcept
    {
        using std::swap;
        swap(data_, o.data_);
        swap(size_, o.size_);
    }

private:
    std::unique_ptr<T[]> data_;
    size_type size_ = 0;
};
