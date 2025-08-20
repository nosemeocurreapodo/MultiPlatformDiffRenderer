#pragma once

/*

// ------------------------
// Lightweight RAII view
// ------------------------

#include <span>

struct NoopReleaser { void operator()() const noexcept {} };

template <class T, class Releaser = NoopReleaser>
class MappedView {
public:
    MappedView(T* p, std::size_t n, Releaser r = {}) noexcept
        : ptr_(p), n_(n), rel_(std::move(r)) {}

    // non-copyable (keeps lifetime strictly scoped like GPU mappings)
    MappedView(const MappedView&) = delete;
    MappedView& operator=(const MappedView&) = delete;

    // movable
    MappedView(MappedView&&) noexcept = default;
    MappedView& operator=(MappedView&&) noexcept = default;

    ~MappedView() { rel_(); }

    // Accessors
    std::span<T> span() const noexcept { return { ptr_, n_ }; }
    T* data() const noexcept { return ptr_; }
    std::size_t size() const noexcept { return n_; }

    // Convenience
    T& operator[](std::size_t i) const noexcept { assert(i < n_); return ptr_[i]; }
    T* begin()  const noexcept { return ptr_; }
    T* end()    const noexcept { return ptr_ + n_; }

private:
    T*           ptr_{};
    std::size_t  n_{0};
    Releaser     rel_{};
};
*/

// --- Minimal mapped view pieces (works with CPU/GL buffers too) ---
struct NoopReleaser { void operator()() const noexcept {} };

template<class T, class Releaser = NoopReleaser>
class MappedView {
public:
    MappedView(T* p, std::size_t n, Releaser r = {}) noexcept
      : ptr_(p), n_(n), rel_(std::move(r)) {}
    MappedView(const MappedView&) = delete;
    MappedView& operator=(const MappedView&) = delete;
    MappedView(MappedView&&) noexcept = default;
    MappedView& operator=(MappedView&&) noexcept = default;
    ~MappedView() { rel_(); }

    T* data()  const noexcept { return ptr_; }
    std::size_t size() const noexcept { return n_; }
    T* begin() const noexcept { return ptr_; }
    T* end()   const noexcept { return ptr_ + n_; }
    T& operator[](std::size_t i) const noexcept { return ptr_[i]; }

private:
    T* ptr_{};
    std::size_t n_{};
    Releaser rel_{};
};