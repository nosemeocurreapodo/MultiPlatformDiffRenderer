#pragma once
// #include <algorithm>
// #include <cassert>
// #include <cstddef>
// #include <type_traits>
// #include <utility>
// #include <vector>
// #include <cmath> // std::floor, std::fmod

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
    TextureBase() = default;

    // Rule of 5
    TextureBase(const TextureBase &) = default;
    TextureBase &operator=(const TextureBase &) = default;
    TextureBase(TextureBase &&) noexcept = default;
    TextureBase &operator=(TextureBase &&) noexcept = default;
    ~TextureBase() = default;

    void generate_mipmaps(int base_lvl)
    {
        // build lower levels
        for (std::size_t lvl = base_lvl + 1; lvl < derived_().levels(); ++lvl)
        {
            generate_mipmap_(lvl);
        }
    }

protected:
    // Normalized sampling in [0,1] (allows outside depending on address mode)
    T sample_(Scalar v, Scalar u,
              std::size_t lvl = 0,
              AddressMode addr = AddressMode::Clamp,
              FilterMode filt = FilterMode::Bilinear) const
    {
        const Scalar w = static_cast<Scalar>(derived_().width(lvl));
        const Scalar h = static_cast<Scalar>(derived_().height(lvl));

        auto wrap = [&](Scalar t)
        {
            switch (addr)
            {
            case AddressMode::Clamp:
                return clamp(t, 0.0f, 1.0f);
            case AddressMode::Repeat:
            {
                // wrap to [0,1)
                Scalar r = fmod(t, 1.0f);
                if (r < 0.0f)
                    r += 1.0f;
                return r;
            }
            case AddressMode::Mirror:
            {
                // mirror every [0,1], 0..1..0..
                Scalar ip = floor(t);
                Scalar f = t - ip;
                bool odd = static_cast<long>(ip) & 1L;
                return odd ? (1.0f - f) : f;
            }
            }
            return t; // unreachable
        };

        const Scalar uu = wrap(u);
        const Scalar vv = wrap(v);

        const float x = uu * w - 0.5f;
        const float y = vv * h - 0.5f;

        return (filt == FilterMode::Nearest)
                   ? nearest_(y, x, lvl)
                   : bilinear_(y, x, lvl);
    }

    T nearest_(Scalar y, Scalar x, std::size_t lvl) const
    {
        const auto xi = static_cast<std::size_t>(lround(x));
        const auto yi = static_cast<std::size_t>(lround(y));
        return derived_().texel_(yi, xi, lvl);
    }

    T bilinear_(Scalar y, Scalar x, std::size_t lvl) const
    {
        const auto w = derived_().width(lvl);
        const auto h = derived_().height(lvl);

        const Scalar xf = floor(x);
        const Scalar yf = floor(y);
        const auto x0 = static_cast<std::size_t>(xf < 0.0f ? 0.0f : xf);
        const auto y0 = static_cast<std::size_t>(yf < 0.0f ? 0.0f : yf);
        const auto x1 = min(x0 + 1, w - 1);
        const auto y1 = min(y0 + 1, h - 1);

        const Scalar dx = x - static_cast<Scalar>(x0);
        const Scalar dy = y - static_cast<Scalar>(y0);

        // auto m = MapRead(lvl); // one mapping, four reads
        const auto idx = [&](std::size_t yy, std::size_t xx)
        {
            // return m[xx + yy * w];
            //  return lvls_[lvl].buf[xx + yy * w];
            return derived_().texel_(yy, xx, lvl);
        };

        const T tl = idx(y0, x0);
        const T tr = idx(y0, x1);
        const T bl = idx(y1, x0);
        const T br = idx(y1, x1);

        if (derived_().nodata() == tl || derived_().nodata() == tr || derived_().nodata() == bl || derived_().nodata() == br)
            return derived_().nodata();

        // const Scalar w_tl = (1.0f - dx) * (1.0f - dy);
        // const Scalar w_tr = (dx) * (1.0f - dy);
        // const Scalar w_bl = (1.0f - dx) * (dy);
        // const Scalar w_br = (dx) * (dy);
        // return static_cast<T>(tl * w_tl + tr * w_tr + bl * w_bl + br * w_br);

        const T Cx0 = tl * (Scalar(1) - dx) + tr * dx;
        const T Cx1 = bl * (Scalar(1) - dx) + br * dx;
        return static_cast<T>(Cx0 * (Scalar(1) - dy) + Cx1 * dy);
    }

    void generate_mipmap_(std::size_t lvl)
    {
        const std::size_t sw = derived_().width(lvl - 1);
        const std::size_t sh = derived_().height(lvl - 1);
        const std::size_t dw = derived_().width(lvl);
        const std::size_t dh = derived_().height(lvl);

        const auto s_idx = [&](std::size_t yy, std::size_t xx) -> T
        {
            yy = min(yy, sh - 1);
            xx = min(xx, sw - 1);
            return derived_().texel_(yy, xx, lvl - 1);
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

                if (derived_().nodata() == tl || derived_().nodata() == tr || derived_().nodata() == bl || derived_().nodata() == br)
                {
                    derived_().set_texel_(derived_().nodata(), y, x, lvl);
                }
                else
                {
                    T val = static_cast<T>((tl + tr + bl + br) * 0.25f);
                    derived_().set_texel_(val, y, x, lvl);
                }
            }
        }
    }

    Derived &derived_() { return *static_cast<Derived *>(this); }
    const Derived &derived_() const { return *static_cast<const Derived *>(this); }
};
