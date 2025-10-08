#pragma once

// #include <algorithm>
// #include <cassert>
// #include <cstddef>
// #include <memory>
// #include <utility>
// #include <vector>

#include "backends/xrt/devicexrt.h"
#include "backends/base/MappedView.h" // your MappedView + NoopReleaser

template <typename T>
class BufferXRT
{
public:
    // using value_type = T;
    // using size_type = std::size_t;

    BufferXRT() = default;

    explicit BufferXRT(std::size_t n, int group_id)
    {
        if (n > 0)
        {
            bo_ = xrt::bo(device_xrt, n * sizeof(T), group_id);
            bo_map_ = bo_.map<T *>();
            size_ = n;
        }
    }

    BufferXRT(std::size_t n, const T *src, int group_id)
    {
        if (n > 0)
        {
            bo_ = xrt::bo(device_xrt, n * sizeof(T), group_id);
            bo_map_ = bo_.map<T *>();
            size_ = n;
            std::copy_n(src, size_, bo_map_);
            bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
    }

    explicit BufferXRT(const std::vector<T> &v, int group_id) : BufferXRT(v.size(), v.data(), group_id) {}

    // Copy (deep) via copy-and-swap
    BufferXRT(const BufferXRT &other) : BufferXRT(other.size_)
    {
        if (size_)
            std::copy_n(other.bo_map_, size_, bo_map_);
    }

    BufferXRT &operator=(const BufferXRT &other)
    {
        if (this != &other)
        {
            BufferXRT tmp(other);
            swap(tmp);
        }
        return *this;
    }

    // Move (nothrow)
    BufferXRT(BufferXRT &&) noexcept = default;
    BufferXRT &operator=(BufferXRT &&) noexcept = default;

    ~BufferXRT() = default;

    // -------- capacity / info --------
    std::size_t size() const noexcept { return size_; }

    // -------- cross-backend style API --------
    [[nodiscard]] MappedView<const T, NoopReleaser> MapRead() const & noexcept
    {
        return MappedView<const T, NoopReleaser>(bo_map_, size_);
    }
    [[nodiscard]] MappedView<T, NoopReleaser> MapWrite() & noexcept
    {
        return MappedView<T, NoopReleaser>(bo_map_, size_);
    }
    // forbid mapping temporaries (view would dangle)
    MappedView<const T> MapRead() const && = delete;
    MappedView<T> MapWrite() && = delete;

    // private:
    //     template <class T2>
    //     friend class TextureXRT;

    void swap(BufferXRT &o) noexcept
    {
        std::swap(bo_map_, o.bo_map_);
        std::swap(size_, o.size_);
    }

    T *data() noexcept { return bo_map_; }
    const T *data() const noexcept { return bo_map_; }

    xrt::bo bo_;
    T *bo_map_;
    std::size_t size_ = 0;
};
