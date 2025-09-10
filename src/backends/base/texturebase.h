#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>
#include <cmath> // std::floor, std::fmod

#include "core/types.h"

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

template <class Derived, class T>
class TextureBase
{
public:
    /*
        TextureBase() = default;

        // Create empty pyramid filled with nodata
        TextureBase(std::size_t w, std::size_t h, T nodata)
            : nodata_(nodata)
        {
            build_pyramid_(w, h);
            // Fill base and all levels with nodata
            // for (std::size_t lvl = 0; lvl < levels(); ++lvl)
            //    fill(lvl, nodata_);
        }

        // Create and upload base level, auto-generate mipmaps
        TextureBase(std::size_t w, std::size_t h, T nodata, const T *base)
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
        TextureBase(const TextureBase &) = default;
        TextureBase &operator=(const TextureBase &) = default;
        TextureBase(TextureBase &&) noexcept = default;
        TextureBase &operator=(TextureBase &&) noexcept = default;
        ~TextureBase() = default;
        */

    // Introspection
    std::size_t width(std::size_t lvl) const { return levels_[lvl].w; }
    std::size_t height(std::size_t lvl) const { return levels_[lvl].h; }
    std::size_t levels() const { return levels_.size(); }
    std::size_t size(int lvl) const { return width(lvl) * height(lvl); }
    std::size_t type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    /*
    // Fill a level with a constant
    void fill(std::size_t lvl, const T &v)
    {
        // auto m = lvls_[lvl].buf.MapWrite();
        // std::fill(m.begin(), m.end(), v);

        auto m = MapWrite(lvl);
        std::fill(m.data(), m.data() + m.size(), v);
    }
    */

    void generate_mipmaps(int base_lvl)
    {
        // build lower levels
        for (std::size_t lvl = base_lvl + 1; lvl < levels(); ++lvl)
        {
            derived_().generate_mipmap_(lvl);
        }
    }

protected:
    struct Level
    {
        std::size_t offset; // element offset in storage_
        std::size_t size;   // elements at this level (w*h*channels)
        int w, h;
        // optional: std::size_t pitch; // elements per row if you pad rows
    };

    std::vector<Level> levels_;
    T nodata_{};

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(std::size_t y, std::size_t x, std::size_t lvl) const
    {
        assert(x < width(lvl) && y < height(lvl));
        const auto &L = levels_[lvl];
        return derived_()[L.offset + y * L.w + x]
    }
    void set_texel_(const T &v, std::size_t y, std::size_t x, std::size_t lvl)
    {
        assert(x < width(lvl) && y < height(lvl));
        const auto &L = levels_[lvl];
        derived_()[L.offset + y * L.w + x] = v;
    }

    // Normalized sampling in [0,1] (allows outside depending on address mode)
    T sample_(Scalar v, Scalar u,
              std::size_t lvl = 0,
              AddressMode addr = AddressMode::Clamp,
              FilterMode filt = FilterMode::Bilinear) const
    {
        const Scalar w = static_cast<Scalar>(width(lvl));
        const Scalar h = static_cast<Scalar>(height(lvl));

        auto wrap = [&](Scalar t)
        {
            switch (addr)
            {
            case AddressMode::Clamp:
                return std::clamp(t, 0.0f, 1.0f);
            case AddressMode::Repeat:
            {
                // wrap to [0,1)
                Scalar r = std::fmod(t, 1.0f);
                if (r < 0.0f)
                    r += 1.0f;
                return r;
            }
            case AddressMode::Mirror:
            {
                // mirror every [0,1], 0..1..0..
                Scalar ip = std::floor(t);
                Scalar f = t - ip;
                bool odd = static_cast<long>(ip) & 1L;
                return odd ? (1.0f - f) : f;
            }
            }
            return t; // unreachable
        };

        const Scalar uu = wrap(u);
        const Scalar vv = wrap(v);

        const Scalar x = uu * (w - 1.0f);
        const Scalar y = vv * (h - 1.0f);

        return (filt == FilterMode::Nearest)
                   ? nearest_(y, x, lvl)
                   : bilinear_(y, x, lvl);
    }

    void build_pyramid_(std::size_t w, std::size_t h)
    {
        levels_.clear();
        if (w == 0 || h == 0)
            return;

        std::size_t running = 0;
        // build until 1x1 (inclusive)
        while (true)
        {
            // levels_.push_back(Level{w, h, BufferCPU<T>(w * h)});
            Level L;
            L.w = w;
            L.h = h;
            L.size = std::size_t(w) * std::size_t(h);
            L.offset = running;

            levels_.push_back(L);
            running += L.size;

            if (w == 1 && h == 1)
                break;

            w = std::max<std::size_t>(1, w >> 1);
            h = std::max<std::size_t>(1, h >> 1);
        }

        storage_ = BufferCPU<T>(running);
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

    /*
    // Safe fetch with clamping to edge
    T fetch_(size_type y, size_type x, size_type lvl) const
    {
        x = std::min(x, width(lvl) - 1);
        y = std::min(y, height(lvl) - 1);
        // auto m = lvls_[lvl].buf.MapRead();
        // return m.data()[x + y * width(lvl)];
        return lvls_[lvl].buf[x + y * width(lvl)];
    }
    */

    T nearest_(Scalar y, Scalar x, std::size_t lvl) const
    {
        const auto xi = static_cast<std::size_t>(std::lround(x));
        const auto yi = static_cast<std::size_t>(std::lround(y));
        return texel_(yi, xi, lvl);
    }

    T bilinear_(float y, float x, std::size_t lvl) const
    {
        const auto w = width(lvl);
        const auto h = height(lvl);

        const float xf = std::floor(x);
        const float yf = std::floor(y);
        const auto x0 = static_cast<std::size_t>(xf < 0.0f ? 0.0f : xf);
        const auto y0 = static_cast<std::size_t>(yf < 0.0f ? 0.0f : yf);
        const auto x1 = std::min(x0 + 1, w - 1);
        const auto y1 = std::min(y0 + 1, h - 1);

        const float dx = x - static_cast<float>(x0);
        const float dy = y - static_cast<float>(y0);

        // auto m = MapRead(lvl); // one mapping, four reads
        const auto idx = [&](std::size_t yy, std::size_t xx)
        {
            // return m[xx + yy * w];
            //  return lvls_[lvl].buf[xx + yy * w];
            return texel_(yy, xx, lvl);
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

    void generate_mipmap_(std::size_t lvl)
    {
        // downsample from lvl-1 to lvl using 2x2 box, clamped at edges
        // const auto &src = lvls_[lvl - 1];
        // auto &dst = lvls_[lvl];

        const std::size_t sw = levels_[lvl - 1].w, sh = levels_[lvl - 1].h;
        const std::size_t dw = levels_[lvl].w, dh = levels_[lvl].h;

        auto src = MapRead(lvl - 1);
        auto dst = MapWrite(lvl);

        const auto s_idx = [&](std::size_t yy, std::size_t xx) -> T
        {
            yy = std::min(yy, sh - 1);
            xx = std::min(xx, sw - 1);
            // return s.data()[xx + yy * sw];
            return src[xx + yy * sw];
        };

        for (std::size_t y = 0; y < dh; ++y)
        {
            for (std::size_t x = 0; x < dw; ++x)
            {
                const std::size_t sx = x * 2;
                const std::size_t sy = y * 2;

                const T tl = s_idx(sy, sx);
                const T tr = s_idx(sy, sx + 1);
                const T bl = s_idx(sy + 1, sx);
                const T br = s_idx(sy + 1, sx + 1);

                if (is_nodata_(tl) || is_nodata_(tr) || is_nodata_(bl) || is_nodata_(br))
                {
                    // d.data()[x + y * dw] = nodata_;
                    dst[x + y * dw] = nodata_;
                }
                else
                {
                    // d.data()[x + y * dw] = static_cast<T>((tl + tr + bl + br) * 0.25f);
                    dst[x + y * dw] = static_cast<T>((tl + tr + bl + br) * 0.25f);
                }
            }
        }
    }

    Derived &derived_() { return *static_cast<Derived *>(this); }
    const Derived &derived_() const { return *static_cast<const Derived *>(this); }
};
