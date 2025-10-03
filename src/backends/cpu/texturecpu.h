#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>
#include <cmath>
#include "backends/base/texturebase.h"
#include "backends/cpu/buffercpu.h"

template <class T>
class TextureCPU : public TextureBase<TextureCPU<T>, T>
{
public:
    // using value_type = T;
    // using size_type = UInt;

    // Default-construct an empty texture. Safe to assign later.
    TextureCPU() = default;

    // Create empty pyramid filled with nodata
    TextureCPU(UInt w, UInt h, T nodata)
        : nodata_(nodata)
    {

        build_pyramid_(w, h);
        // Fill base and all levels with nodata
        for (UInt lvl = 0; lvl < levels(); ++lvl)
            fill(lvl, nodata);
    }

    // Create and upload base level
    TextureCPU(UInt w, UInt h, T nodata, const T *base)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
        // write base
        {
            auto m = MapWrite(0);
            std::copy_n(base, w * h, m.data());
        }
    }

    // Rule of 5
    TextureCPU(const TextureCPU &) = default;
    TextureCPU &operator=(const TextureCPU &) = default;
    TextureCPU(TextureCPU &&) noexcept = default;
    TextureCPU &operator=(TextureCPU &&) noexcept = default;
    ~TextureCPU() = default;

    // Introspection
    UInt width(UInt lvl) const { return levels_[lvl].w; }
    UInt height(UInt lvl) const { return levels_[lvl].h; }
    UInt levels() const { return levels_.size(); }
    UInt size(int lvl) const { return width(lvl) * height(lvl); }
    UInt type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(UInt lvl, const T &v)
    {
        auto m = MapWrite(lvl);
        std::fill(m.data(), m.data() + m.size(), v);
    }

    [[nodiscard]] MappedView<const T, NoopReleaser> MapRead(int lvl) const
    {
        const auto &L = levels_[lvl];
        return MappedView<const T, NoopReleaser>(storage_.data() + L.offset, L.size);
    }

    [[nodiscard]] MappedView<T, NoopReleaser> MapWrite(int lvl)
    {
        const auto &L = levels_[lvl];
        return MappedView<T, NoopReleaser>(storage_.data() + L.offset, L.size);
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(UInt y, UInt x, UInt lvl) const
    {
        assert(x < width(lvl) && y < height(lvl));

        const auto &L = levels_[lvl];
        return storage_.data()[L.offset + y * L.w + x];
    }

    void set_texel_(const T &v, UInt y, UInt x, UInt lvl)
    {
        assert(x < width(lvl) && y < height(lvl));

        const auto &L = levels_[lvl];
        storage_.data()[L.offset + y * L.w + x] = v;
    }

protected:
    template <class Mesh, template<class> class Texture>
    friend class DepthRendererBase;
    template <class Mesh, template<class> class Texture>
    friend class ImageRendererBase;
    // friend class DepthRendererCPU;
    // friend class ImageRendererCPU;
    friend class ResidualRendererCPU;
    friend class L2RendererCPU;
    friend class DIDxyRendererCPU;
    friend class JPoseRendererCPU;
    friend class JMapRendererCPU;

    struct Level
    {
        UInt offset; // element offset in storage_
        UInt size;   // elements at this level (w*h*channels)
        int w, h;
        // optional: UInt pitch; // elements per row if you pad rows
    };

    std::vector<Level> levels_;

    BufferCPU<T> storage_;

    T nodata_{};

    void build_pyramid_(UInt w, UInt h)
    {
        levels_.clear();
        if (w == 0 || h == 0)
            return;

        UInt running = 0;
        // build until 1x1 (inclusive)
        while (true)
        {
            // levels_.push_back(Level{w, h, BufferCPU<T>(w * h)});
            Level L;
            L.w = w;
            L.h = h;
            L.size = UInt(w) * UInt(h);
            L.offset = running;

            levels_.push_back(L);
            running += L.size;

            if (w == 1 && h == 1)
                break;

            w = std::max<UInt>(1, w >> 1);
            h = std::max<UInt>(1, h >> 1);
        }

        storage_ = BufferCPU<T>(running);
    }
};
