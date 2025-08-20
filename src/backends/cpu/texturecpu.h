#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>
#include <cmath>                    // std::floor, std::fmod
#include "backends/cpu/buffercpu.h" // your MapRead/MapWrite version

enum class AddressMode
{
    Repeat,
    Clamp,
    Mirror
};
enum class FilterMode
{
    Nearest,
    Bilinear
};

template <class T>
class TextureCPU
{
public:
    using value_type = T;
    using size_type = std::size_t;

    TextureCPU() = default;

    // Create empty pyramid filled with nodata
    TextureCPU(size_type w, size_type h, T nodata)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
        // Fill base and all levels with nodata
        for (size_type lvl = 0; lvl < levels(); ++lvl)
            fill(lvl, nodata_);
    }

    // Create and upload base level, auto-generate mipmaps
    TextureCPU(size_type w, size_type h, T nodata, const T *base)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
        // write base
        {
            auto m = lvls_[0].buf.MapWrite();
            std::copy_n(base, w * h, m.data());
        }
        // build lower levels
        for (size_type lvl = 1; lvl < levels(); ++lvl)
        {
            generate_mipmap_(lvl);
        }
    }

    // Rule of 5
    TextureCPU(const TextureCPU &) = default;
    TextureCPU &operator=(const TextureCPU &) = default;
    TextureCPU(TextureCPU &&) noexcept = default;
    TextureCPU &operator=(TextureCPU &&) noexcept = default;
    ~TextureCPU() = default;

    // Introspection
    size_type width(size_type lvl) const { return lvls_[lvl].w; }
    size_type height(size_type lvl) const { return lvls_[lvl].h; }
    size_type levels() const { return lvls_.size(); }
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(size_type lvl, const T &v)
    {
        auto m = lvls_[lvl].buf.MapWrite();
        std::fill(m.begin(), m.end(), v);
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel(size_type y, size_type x, size_type lvl) const
    {
        assert(x < width(lvl) && y < height(lvl));
        auto m = lvls_[lvl].buf.MapRead();
        return m.data()[x + y * width(lvl)];
    }
    void set_texel(const T &v, size_type y, size_type x, size_type lvl)
    {
        assert(x < width(lvl) && y < height(lvl));
        auto m = lvls_[lvl].buf.MapWrite();
        m.data()[x + y * width(lvl)] = v;
    }

    // Normalized sampling in [0,1] (allows outside depending on address mode)
    T sample(float u, float v,
             size_type lvl = 0,
             AddressMode addr = AddressMode::Clamp,
             FilterMode filt = FilterMode::Bilinear) const
    {
        const auto w = static_cast<float>(width(lvl));
        const auto h = static_cast<float>(height(lvl));

        auto wrap = [&](float t)
        {
            switch (addr)
            {
            case AddressMode::Clamp:
                return std::clamp(t, 0.0f, 1.0f);
            case AddressMode::Repeat:
            {
                // wrap to [0,1)
                float r = std::fmod(t, 1.0f);
                if (r < 0.0f)
                    r += 1.0f;
                return r;
            }
            case AddressMode::Mirror:
            {
                // mirror every [0,1], 0..1..0..
                float ip = std::floor(t);
                float f = t - ip;
                bool odd = static_cast<long>(ip) & 1L;
                return odd ? (1.0f - f) : f;
            }
            }
            return t; // unreachable
        };

        const float uu = wrap(u);
        const float vv = wrap(v);

        const float x = uu * (w - 1.0f);
        const float y = vv * (h - 1.0f);

        return (filt == FilterMode::Nearest)
                   ? nearest_(y, x, lvl)
                   : bilinear_(y, x, lvl);
    }

    // Expose map views for bulk ops / algorithms (cross-backend shape)
    [[nodiscard]] MappedView<const T> MapRead(size_type lvl) const &
    {
        return lvls_[lvl].buf.MapRead();
    }
    [[nodiscard]] MappedView<T> MapWrite(size_type lvl) &
    {
        return lvls_[lvl].buf.MapWrite();
    }

private:
    static constexpr size_type lvl_base = 0;

    struct Level
    {
        size_type w{}, h{};
        BufferCPU<T> buf; // owns w*h elements
    };

    std::vector<Level> lvls_;
    T nodata_{};

    void build_pyramid_(size_type w, size_type h)
    {
        lvls_.clear();
        if (w == 0 || h == 0)
            return;
        // build until 1x1 (inclusive)
        while (true)
        {
            lvls_.push_back(Level{w, h, BufferCPU<T>(w * h)});
            if (w == 1 && h == 1)
                break;
            w = std::max<size_type>(1, w >> 1);
            h = std::max<size_type>(1, h >> 1);
        }
    }

    bool is_nodata_(const T &v) const
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            // exact compare is common for sentinel; tweak to epsilon if needed
            return v == nodata_;
        }
        else
        {
            return v == nodata_;
        }
    }

    // Safe fetch with clamping to edge
    T fetch_(size_type y, size_type x, size_type lvl) const
    {
        x = std::min(x, width(lvl) - 1);
        y = std::min(y, height(lvl) - 1);
        auto m = lvls_[lvl].buf.MapRead();
        return m.data()[x + y * width(lvl)];
    }

    T nearest_(float y, float x, size_type lvl) const
    {
        const auto xi = static_cast<size_type>(std::lround(x));
        const auto yi = static_cast<size_type>(std::lround(y));
        return fetch_(yi, xi, lvl);
    }

    T bilinear_(float y, float x, size_type lvl) const
    {
        const auto w = width(lvl);
        const auto h = height(lvl);

        const float xf = std::floor(x);
        const float yf = std::floor(y);
        const auto x0 = static_cast<size_type>(xf < 0.0f ? 0.0f : xf);
        const auto y0 = static_cast<size_type>(yf < 0.0f ? 0.0f : yf);
        const auto x1 = std::min(x0 + 1, w - 1);
        const auto y1 = std::min(y0 + 1, h - 1);

        const float dx = x - static_cast<float>(x0);
        const float dy = y - static_cast<float>(y0);

        auto m = lvls_[lvl].buf.MapRead(); // one mapping, four reads
        const auto idx = [&](size_type yy, size_type xx)
        {
            return m.data()[xx + yy * w];
        };

        const T tl = idx(y0, x0);
        const T tr = idx(y0, x1);
        const T bl = idx(y1, x0);
        const T br = idx(y1, x1);

        if (is_nodata_(tl) || is_nodata_(tr) || is_nodata_(bl) || is_nodata_(br))
            return nodata_;

        const float w_tl = (1.0f - dx) * (1.0f - dy);
        const float w_tr = (dx) * (1.0f - dy);
        const float w_bl = (1.0f - dx) * (dy);
        const float w_br = (dx) * (dy);

        return static_cast<T>(tl * w_tl + tr * w_tr + bl * w_bl + br * w_br);
    }

    void generate_mipmap_(size_type lvl)
    {
        // downsample from lvl-1 to lvl using 2x2 box, clamped at edges
        const auto &src = lvls_[lvl - 1];
        auto &dst = lvls_[lvl];

        const size_type sw = src.w, sh = src.h;
        const size_type dw = dst.w, dh = dst.h;

        auto s = src.buf.MapRead();
        auto d = dst.buf.MapWrite();

        const auto s_idx = [&](size_type yy, size_type xx) -> T
        {
            yy = std::min(yy, sh - 1);
            xx = std::min(xx, sw - 1);
            return s.data()[xx + yy * sw];
        };

        for (size_type y = 0; y < dh; ++y)
        {
            for (size_type x = 0; x < dw; ++x)
            {
                const size_type sx = x * 2;
                const size_type sy = y * 2;

                const T tl = s_idx(sy, sx);
                const T tr = s_idx(sy, sx + 1);
                const T bl = s_idx(sy + 1, sx);
                const T br = s_idx(sy + 1, sx + 1);

                if (is_nodata_(tl) || is_nodata_(tr) || is_nodata_(bl) || is_nodata_(br))
                {
                    d.data()[x + y * dw] = nodata_;
                }
                else
                {
                    d.data()[x + y * dw] = static_cast<T>((tl + tr + bl + br) * 0.25f);
                }
            }
        }
    }
};
