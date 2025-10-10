#pragma once

// #include <algorithm>
// #include <cassert>
// #include <cstddef>
// #include <type_traits>
// #include <utility>
// #include <vector>
// #include <cmath>
#include "backends/xrt/bufferxrt.h"

template <class T>
class TextureXRT
{
public:
    // using value_type = T;
    // using size_type = std::size_t;

    TextureXRT() = default;

    // Create empty pyramid filled with nodata
    TextureXRT(std::size_t w, std::size_t h, T nodata, int group_id)
        : nodata_(nodata)
    {
        build_pyramid_(w, h, group_id);
        // Fill base and all levels with nodata
        // for (std::size_t lvl = 0; lvl < levels(); ++lvl)
        //    fill(lvl, nodata_);
    }

    // Create and upload base level, auto-generate mipmaps
    TextureXRT(std::size_t w, std::size_t h, T nodata, const T *base, int group_id)
        : nodata_(nodata)
    {
        build_pyramid_(w, h, group_id);
        // write base
        {
            auto m = MapWrite(0);
            std::copy_n(base, w * h, m.data());
        }
    }

    // Rule of 5
    TextureXRT(const TextureXRT &) = default;
    TextureXRT &operator=(const TextureXRT &) = default;
    TextureXRT(TextureXRT &&) noexcept = default;
    TextureXRT &operator=(TextureXRT &&) noexcept = default;
    ~TextureXRT() = default;

    // Introspection
    std::size_t width(std::size_t lvl) const { return levels_[lvl].w; }
    std::size_t height(std::size_t lvl) const { return levels_[lvl].h; }
    std::size_t levels() const { return levels_.size(); }
    std::size_t size() const { return total_size_; }
    std::size_t type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(std::size_t lvl, const T &v)
    {
        // auto m = lvls_[lvl].buf.MapWrite();
        // std::fill(m.begin(), m.end(), v);

        auto m = MapWrite(lvl);
        std::fill(m.data(), m.data() + m.size(), v);
    }

    void generate_mipmaps(int base_lvl)
    {
        // build lower levels
        for (UInt lvl = base_lvl + 1; lvl < levels(); ++lvl)
        {
            // generate_mipmap_(lvl);
            generate_mipmap<T, TextureXRT>(*this, lvl);
        }
    }

    [[nodiscard]] MappedView<const T, NoopReleaser> MapRead(int lvl) const
    {
        const auto &L = levels_[lvl];
        auto m = storage_.MapRead();
        return MappedView<const T, NoopReleaser>(m.data() + L.offset, L.w * L.h);
    }

    [[nodiscard]] MappedView<T, NoopReleaser> MapWrite(int lvl)
    {
        const auto &L = levels_[lvl];
        auto m = storage_.MapWrite();
        return MappedView<T, NoopReleaser>(m.data() + L.offset, L.w * L.h);
    }

    // private:
    //     friend class DepthRendererXRT;
    //     friend class ImageRendererXRT;
    //     friend class ResidualRendererXRT;
    //     friend class L2RendererXRT;
    //     friend class DIDxyRendererXRT;
    //     friend class JPoseRendererXRT;
    //     friend class JMapRendererXRT;

    struct Level
    {
        std::size_t offset; // element offset in storage_
        // std::size_t size;   // elements at this level (w*h*channels)
        int w, h;
        // optional: std::size_t pitch; // elements per row if you pad rows
    };

    void build_pyramid_(std::size_t w, std::size_t h, int group_id)
    {
        levels_.clear();
        if (w == 0 || h == 0)
            return;

        total_size_ = 0;
        // build until 1x1 (inclusive)
        while (true)
        {
            // levels_.push_back(Level{w, h, BufferCPU<T>(w * h)});
            Level L;
            L.w = w;
            L.h = h;
            // L.size = std::size_t(w) * std::size_t(h);
            L.offset = total_size_;

            levels_.push_back(L);
            total_size_ += L.w * L.h;

            if (w == 1 && h == 1)
                break;

            w = std::max<std::size_t>(1, w >> 1);
            h = std::max<std::size_t>(1, h >> 1);
        }

        storage_ = BufferXRT<T>(total_size_, group_id);
    }

    BufferXRT<T> storage_;
    std::vector<Level> levels_;
    UInt total_size_;
    // std::vector<Level> lvls_;
    T nodata_{};
};
