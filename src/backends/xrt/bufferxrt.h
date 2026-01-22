#pragma once

// #include <algorithm>
// #include <cassert>
// #include <cstddef>
// #include <memory>
// #include <utility>
// #include <vector>

#include "backends/xrt/devicexrt.h"
#include "backends/base/mappedviewbase.h" // your MappedView + NoopReleaser

struct BufferXRTNoopReleaser
{
    void operator()() const noexcept {}
};

template <typename T>
using BufferViewXRT = BufferViewBase<T, BufferXRTNoopReleaser>;

template <typename T>
class BufferXRT
{
public:
    // using value_type = T;
    // using size_type = std::size_t;

    BufferXRT() = default;

    BufferXRT(std::size_t n, int group_id)
    {
        if (n > 0)
        {
            bo_ = xrt::bo(device_xrt, n * sizeof(T), group_id);
            group_id_ = group_id;
            bo_map_ = bo_.map<T *>();
            size_ = n;
        }
    }

    BufferXRT(std::size_t n, const T *src, int group_id) : BufferXRT(n, group_id)
    {
        if (n > 0)
        {
            // std::copy_n(src, size_, bo_map_);
            std::memcpy(bo_map_, src, sizeof(T) * size_);

            // bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
    }

    BufferXRT(const std::vector<T> &v, int group_id) : BufferXRT(v.size(), v.data(), group_id) {}

    // Copy (deep) via copy-and-swap
    BufferXRT(const BufferXRT &other) : BufferXRT(other.size_, other.group_id_)
    {
        if (size_)
        {
            // std::copy_n(other.bo_map_, size_, bo_map_);
            std::memcpy(bo_map_, other.bo_map_, sizeof(T) * size_);

            // bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
    }

    BufferXRT &operator=(const BufferXRT &other)
    {
        if (this != &other)
        {
            // BufferXRT tmp(other);
            //  swap(tmp);

            bo_ = xrt::bo(device_xrt, other.size_ * sizeof(T), other.group_id_);
            group_id_ = other.group_id_;
            bo_map_ = bo_.map<T *>();
            size_ = other.size_;

            if (size_)
            {
                // std::copy_n(other.bo_map_, size_, bo_map_);
                std::memcpy(bo_map_, other.bo_map_, sizeof(T) * size_);
                // bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
            }
        }
        return *this;
    }

    // Move (nothrow)
    // BufferXRT(BufferXRT &&) noexcept = default;
    // BufferXRT &operator=(BufferXRT &&) noexcept = default;

    ~BufferXRT() = default;

    // -------- capacity / info --------
    std::size_t size() const { return size_; }

    // -------- cross-backend style API --------
    BufferViewXRT<T> MapRead() const
    {
        return BufferViewXRT<T>(bo_map_, size_);
    }
    BufferViewXRT<T> MapWrite()
    {
        return BufferViewXRT<T>(bo_map_, size_);
    }
    // forbid mapping temporaries (view would dangle)
    // MappedView<const T> MapRead() const && = delete;
    // MappedView<T> MapWrite() && = delete;

    // private:
    //     template <class T2>
    //     friend class TextureXRT;

    // void swap(BufferXRT &o) noexcept
    //{
    //     std::swap(bo_map_, o.bo_map_);
    //     std::swap(size_, o.size_);
    // }

    T *data() { return bo_map_; }
    const T *data() const { return bo_map_; }

    xrt::bo bo_;
    int group_id_;
    T *bo_map_;
    std::size_t size_ = 0;
};
