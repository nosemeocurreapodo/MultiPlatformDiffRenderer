#pragma once

#include <algorithm> // for copy_n
#include <cassert>   // for assert
// #include <cstddef>
#include <memory> // for unique_ptr
// #include <utility>
#include <vector> // for vector

#include "backends/base/mappedviewbase.h" // your MappedView

// --- Minimal mapped view pieces (works with CPU/GL buffers too) ---
struct BufferCPUNoopReleaser
{
    void operator()() const noexcept {}
};

template <typename T>
using BufferViewCPU = BufferViewBase<T, BufferCPUNoopReleaser>;

template <typename T>
class BufferCPU
{
public:
    // using value_type = T;
    // using size_type = std::size_t;

    BufferCPU() = default;

    explicit BufferCPU(std::size_t n)
        : data_(n ? std::make_unique<T[]>(n) : nullptr), size_(n) {}

    BufferCPU(std::size_t n, const T *src) : BufferCPU(n)
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
    std::size_t size() const noexcept { return size_; }

    // -------- cross-backend style API --------
    // On CPU, Map* returns a view with a no-op releaser.
    [[nodiscard]] BufferViewCPU<const T> MapRead() const & noexcept
    {
        return BufferViewCPU<const T>(data_.get(), size_);
    }
    [[nodiscard]] BufferViewCPU<T> MapWrite() & noexcept
    {
        return BufferViewCPU<T>(data_.get(), size_);
    }
    // forbid mapping temporaries (view would dangle)
    BufferViewCPU<const T> MapRead() const && = delete;
    BufferViewCPU<T> MapWrite() && = delete;

private:
    T *data() noexcept { return data_.get(); }
    const T *data() const noexcept { return data_.get(); }

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

    void swap(BufferCPU &o) noexcept
    {
        std::swap(data_, o.data_);
        std::swap(size_, o.size_);
    }

    std::unique_ptr<T[]> data_;
    std::size_t size_ = 0;
};
