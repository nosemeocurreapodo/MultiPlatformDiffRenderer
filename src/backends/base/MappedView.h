#pragma once

// --- Minimal mapped view pieces (works with CPU/GL buffers too) ---
struct NoopReleaser
{
    void operator()() const noexcept {}
};

template <class T, class Releaser = NoopReleaser>
class BufferView
{
public:
    BufferView(T *p, std::size_t n, Releaser r = {}) noexcept
        : ptr_(p), n_(n), rel_(std::move(r)) {}
    BufferView(const BufferView &) = delete;
    BufferView &operator=(const BufferView &) = delete;
    BufferView(BufferView &&) noexcept = default;
    BufferView &operator=(BufferView &&) noexcept = default;
    ~BufferView() { rel_(); }

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

template <class T, class Releaser = NoopReleaser>
class TextureView
{
public:
    TextureView(T *p, std::size_t width, std::size_t height, T nodata, Releaser r = {}) noexcept
        : ptr_(p), width_(width), height_(height), nodata_(nodata), rel_(std::move(r)) {}
    TextureView(const TextureView &) = delete;
    TextureView &operator=(const TextureView &) = delete;
    TextureView(TextureView &&) noexcept = default;
    TextureView &operator=(TextureView &&) noexcept = default;
    ~TextureView() { rel_(); }

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
        assert(y >= 0 && < y < height_ && x >= 0 && x < width_);
        return ptr_[y * width + +x];
    }

    std::size_t width() const { return width_; }
    std::size_t height() const { return height_; }
    const T &nodata() const { return nodata_; }

private:
    T *ptr_{};
    std::size_t width_{};
    std::size_t height_{};
    T nodata_{};
    Releaser rel_{};
};