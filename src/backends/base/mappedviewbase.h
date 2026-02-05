#pragma once

#include <utility>

template <class T, class Releaser>
class BufferViewBase
{
public:
    BufferViewBase(T *p, std::size_t n, Releaser r = {}) noexcept
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
    std::size_t size() const noexcept { return n_; }
    T *begin() const noexcept { return ptr_; }
    T *end() const noexcept { return ptr_ + n_; }
    T &operator[](std::size_t i) const noexcept
    {
        assert(i < n_);
        return ptr_[i];
    }

private:
    T *ptr_{};
    std::size_t n_{};
    Releaser rel_{};
};

struct Level
{
    unsigned int offset; // element offset in storage_
    int w, h;
    // optional: UInt pitch; // elements per row if you pad rows
};

template <class T, class Releaser>
class TextureViewBase
{
public:
    //TextureViewBase() noexcept = default;

    TextureViewBase(T *p, std::size_t width, std::size_t height, T nodata, Releaser r = {}) noexcept
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

    T &operator[](std::size_t i) const noexcept
    {
        assert(i < width_ * height_);
        return ptr_[i];
    }

    T &operator()(std::size_t y, std::size_t x) const noexcept
    {
        assert(y < height_ && x < width_);
        return ptr_[y * width_ + x];
    }

    std::size_t size() const noexcept { return width_ * height_; }
    std::size_t width() const noexcept { return width_; }
    std::size_t height() const noexcept { return height_; }
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
    std::size_t width_{};
    std::size_t height_{};
    T nodata_{};
    Releaser rel_{};
    bool owns_{false};
};
