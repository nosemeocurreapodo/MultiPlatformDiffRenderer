#pragma once
// #include <algorithm>
// #include <cassert>
// #include <cstddef>
// #include <type_traits>
// #include <utility>
// #include <vector>
// #include <cmath>
#include <cstring>
#include "backends/base/texturebase.h"
#include "backends/cpu/buffercpu.h"

template <class T>
class TextureCPU
{
public:
    // using value_type = T;
    // using size_type = UInt;

    // Default-construct an empty texture. Safe to assign later.
    TextureCPU() = default;

    // Create empty pyramid filled with nodata
    TextureCPU(unsigned int w, unsigned int h, T nodata)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
        storage_ = BufferCPU<T>(total_size_);
        // Fill base and all levels with nodata
        // for (UInt lvl = 0; lvl < levels(); ++lvl)
        //    fill(lvl, nodata);
    }

    // Create and upload base level
    TextureCPU(unsigned int w, unsigned int h, T nodata, const T *base)
        : TextureCPU(w, h, nodata)
    {
        if (w > 0 && h > 0)
        {
            auto m = MapWrite(0);
            // std::copy_n(base, w * h, m.data());
            std::memcpy(m.data(), base, w * h * sizeof(T));
        }
    }

    // Rule of 5
    TextureCPU(const TextureCPU &) = default;
    TextureCPU &operator=(const TextureCPU &) = default;
    TextureCPU(TextureCPU &&) noexcept = default;
    TextureCPU &operator=(TextureCPU &&) noexcept = default;
    ~TextureCPU() = default;

    // Introspection
    unsigned int width(unsigned int lvl) const { return levels_[lvl].w; }
    unsigned int height(unsigned int lvl) const { return levels_[lvl].h; }
    unsigned int levels() const { return levels_.size(); }
    // UInt size() const { return total_size_; }
    unsigned int type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(unsigned int lvl, const T &v)
    {
        auto m = MapWrite(lvl);
        std::fill(m.data(), m.data() + m.size(), v);
    }

    void generate_mipmaps(unsigned int base_lvl)
    {
        // build lower levels
        for (unsigned int lvl = base_lvl + 1; lvl < levels(); ++lvl)
        {
            // generate_mipmap_(lvl);
            generate_mipmap<T, TextureCPU>(*this, lvl);
        }
    }

    [[nodiscard]] MappedView<const T, NoopReleaser> MapRead(unsigned int lvl) const
    {
        const auto &L = levels_[lvl];
        return MappedView<const T, NoopReleaser>(storage_.data() + L.offset, L.w * L.h);
    }

    [[nodiscard]] MappedView<T, NoopReleaser> MapWrite(unsigned int lvl)
    {
        const auto &L = levels_[lvl];
        return MappedView<T, NoopReleaser>(storage_.data() + L.offset, L.w * L.h);
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(unsigned int y, unsigned int x, unsigned int lvl) const
    {
        assert(x < width(lvl) && y < height(lvl));

        const auto &L = levels_[lvl];
        return storage_.data()[L.offset + y * L.w + x];
    }

    void set_texel_(const T &v, unsigned int y, unsigned int x, unsigned int lvl)
    {
        assert(x < width(lvl) && y < height(lvl));

        const auto &L = levels_[lvl];
        storage_.data()[L.offset + y * L.w + x] = v;
    }

    void set_texel_(const T &v, unsigned int address, unsigned int lvl)
    {
        //assert(x < width(lvl) && y < height(lvl));

        const auto &L = levels_[lvl];
        storage_.data()[L.offset + address] = v;
    }

    T *data(unsigned int lvl) noexcept { return &storage_[levels_[lvl].offset]; }
    const T *data(unsigned int lvl) const noexcept { return &storage_[levels_[lvl].offset]; }

protected:
    // template <class T, class Mesh, template <class> class Texture>
    // friend class DepthRendererBase;
    // template <class T, class Mesh, template <class> class Texture>
    // friend class ImageRendererBase;
    //  friend class DepthRendererCPU;
    //  friend class ImageRendererCPU;
    // friend class ResidualRendererCPU;
    // friend class L2RendererCPU;
    // friend class DIDxyRendererCPU;
    // friend class JPoseRendererCPU;
    // friend class JMapRendererCPU;

    struct Level
    {
        unsigned int offset; // element offset in storage_
        int w, h;
        // optional: UInt pitch; // elements per row if you pad rows
    };

    void build_pyramid_(unsigned int w, unsigned int h)
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
            L.offset = total_size_;

            levels_.push_back(L);
            total_size_ += w * h;

            if (w == 1 && h == 1)
                break;

            w = std::max<unsigned int>(1, w >> 1);
            h = std::max<unsigned int>(1, h >> 1);
        }
    }

    unsigned int total_size_;
    std::vector<Level> levels_;
    BufferCPU<T> storage_;
    T nodata_{};
};
