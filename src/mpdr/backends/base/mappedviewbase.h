#pragma once

#include <utility>
#include <mpdr/common/types.h>

template <class T, class Releaser>
class BufferViewBase
{
public:
    BufferViewBase(T *p, UIntType n, Releaser r = {}) noexcept
        //    : ptr_(p), n_(n), rel_(std::move(r)) {}
        : ptr_(p), n_(n), rel_(r)
    {
    }
    BufferViewBase(const BufferViewBase &) = delete;
    BufferViewBase &operator=(const BufferViewBase &) = delete;
    BufferViewBase(BufferViewBase &&) noexcept = default;
    BufferViewBase &operator=(BufferViewBase &&) noexcept = default;
    ~BufferViewBase() { rel_(); }

    T *data() const noexcept { return ptr_; }
    UIntType size() const noexcept { return n_; }
    T *begin() const noexcept { return ptr_; }
    T *end() const noexcept { return ptr_ + n_; }
    T &operator[](UIntType i) const noexcept
    {
        assert(i < n_);
        return ptr_[i];
    }

private:
    T *ptr_{};
    UIntType n_{};
    Releaser rel_{};
};

struct Level
{
    UIntType offset; // element offset in storage_
    UIntType w, h;
    // optional: UInt pitch; // elements per row if you pad rows
};

template <class T, class Releaser>
class TextureViewBase
{
public:
    //TextureViewBase() noexcept = default;

    TextureViewBase(T *p, UIntType width, UIntType height, T nodata, Releaser r = {}) noexcept
        : ptr_(p),
          width_(width),
          height_(height),
          nodata_(nodata),
          rel_(std::move(r)),
          owns_(p != nullptr)
    {
    }

    ~TextureViewBase() { reset(); }

    // Non-copyable (critical!)
    TextureViewBase(const TextureViewBase &) = delete;
    TextureViewBase &operator=(const TextureViewBase &) = delete;

    // Movable
    TextureViewBase(TextureViewBase &&other) noexcept
    {
        moveFrom_(std::move(other));
    }

    TextureViewBase &operator=(TextureViewBase &&other) noexcept
    {
        if (this != &other)
        {
            reset();
            moveFrom_(std::move(other));
        }
        return *this;
    }

    void reset() noexcept
    {
        if (owns_)
            rel_(); // unmap + delete PBO
        ptr_ = nullptr;
        width_ = height_ = 0;
        owns_ = false;
        rel_ = Releaser{}; // clear ownership in releaser too
    }

    T *data() const noexcept { return ptr_; }
    T *begin() const noexcept { return ptr_; }
    T *end() const noexcept { return ptr_ + width_ * height_; }

    T &operator[](UIntType i) const noexcept
    {
        assert(i < width_ * height_);
        return ptr_[i];
    }

    T &operator()(UIntType y, UIntType x) const noexcept
    {
        assert(y < height_ && x < width_);
        return ptr_[y * width_ + x];
    }

    UIntType size() const noexcept { return width_ * height_; }
    UIntType width() const noexcept { return width_; }
    UIntType height() const noexcept { return height_; }
    const T &nodata() const noexcept { return nodata_; }

private:
    void moveFrom_(TextureViewBase &&other) noexcept
    {
        ptr_ = other.ptr_;
        width_ = other.width_;
        height_ = other.height_;
        nodata_ = other.nodata_;
        rel_ = std::move(other.rel_);
        owns_ = other.owns_;

        other.ptr_ = nullptr;
        other.width_ = other.height_ = 0;
        other.owns_ = false;
        other.rel_ = Releaser{};
    }

    T *ptr_{};
    UIntType width_{};
    UIntType height_{};
    T nodata_{};
    Releaser rel_{};
    bool owns_{false};
};
